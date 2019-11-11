#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
	#define FD_SETSIZE      2506
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include<windows.h>
	#include<WinSock2.h>
	#pragma comment(lib,"ws2_32.lib")
#else
	#include<unistd.h> //uni std
	#include<arpa/inet.h>
	#include<string.h>

	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

#include<cstdio>
#include <map>
#include<vector>
#include<thread>
#include<mutex>
#include<atomic>

#include"MessageHeader.hpp"
#include"CELLTimestamp.hpp"

//缓冲区最小单元大小
#ifndef RECV_BUFF_SZIE
#define RECV_BUFF_SZIE 10240
#endif // !RECV_BUFF_SZIE

//客户端数据类型
class CClientSocket
{
public:
	CClientSocket(SOCKET clientSock = INVALID_SOCKET)
	{
		m_clientSock = clientSock;
		memset(m_recvMsgBuf, 0, sizeof(m_recvMsgBuf));
		m_lastPos = 0;
	}

	SOCKET GetClientSock()
	{
		return m_clientSock;
	}

	char* GetMsgBuf()
	{
		return m_recvMsgBuf;
	}

	int GetLastPos()
	{
		return m_lastPos;
	}
	void SetLastPos(int pos)
	{
		m_lastPos = pos;
	}

	//发送数据
	int SendData(DataHeader* header)
	{
		if (header)
		{
			return send(m_clientSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

private:
	SOCKET			m_clientSock;
	char			m_recvMsgBuf[RECV_BUFF_SZIE * 5];		//接收消息第二缓冲区
	unsigned int	m_lastPos;						//消息缓冲区的数据尾部位置
};

//网络事件接口
class INetEvent
{
public:
	//纯虚函数
	//客户端加入事件
	virtual void OnNetJoin(CClientSocket* pClient) = 0;
	//客户端离开事件
	virtual void OnNetLeave(CClientSocket* pClient) = 0;
	//客户端消息事件
	virtual void OnNetMsg(CClientSocket* pClient, DataHeader* header) = 0;
	//Recv事件
	virtual void OnNetRecv(CClientSocket* pClient) = 0;
private:

};

class CCellServer
{
private:
	SOCKET m_ListenSock;							//服务端监听Socket
	std::thread* _pThread;

	std::mutex m_mutexForClientsBuf;				//将缓冲队列中的客户端添加到正式队列，用此锁
	std::map<SOCKET, CClientSocket*> m_mapClients;	//正式客户队列
	std::vector<CClientSocket*> m_vecClientsBuf;	//缓冲客户队列

	fd_set m_fdReadBak;								//备份客户端所有socket
	bool m_bIsClientNumChange;						//客户列表是否有变化
	SOCKET m_maxSock;

	char m_szRecvMsg[RECV_BUFF_SZIE] = {};			//recv接收数据缓冲区，然后存入每个客户端对象中的第二缓冲区

	INetEvent* m_pNetEvent;							//网络事件对象

public:
	CCellServer(SOCKET listenSock)
	{
		m_ListenSock = listenSock;
		m_bIsClientNumChange = true;
		_pThread = nullptr;
		m_pNetEvent = nullptr;
	}

	~CCellServer()
	{
		delete _pThread;
		Close();
	}

	//响应网络消息
	void HandleNetMsgAndResponse(CClientSocket* pClient, DataHeader* header)
	{
		m_pNetEvent->OnNetMsg(pClient, header);
	}

	void addClient(CClientSocket* pClient)
	{
		std::lock_guard<std::mutex> lock(m_mutexForClientsBuf);
		m_vecClientsBuf.push_back(pClient);
		return;
	}

	void Start()
	{
		_pThread = new std::thread(std::mem_fun(&CCellServer::HandleNetData), this);
	}

	size_t getClientCount()
	{
		return m_mapClients.size() + m_vecClientsBuf.size();
	}

	void setEventObj(INetEvent* event)
	{
		m_pNetEvent = event;
	}

private:
	//关闭Socket
	void Close()
	{
		if (m_ListenSock != INVALID_SOCKET)
		{
#ifdef _WIN32
			for (int n = (int)m_mapClients.size() - 1; n >= 0; n--)
			{
				closesocket(m_mapClients[n]->GetClientSock());
				delete m_mapClients[n];
			}
			// 8 关闭套节字closesocket
			closesocket(m_ListenSock);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else
			for (int n = (int)m_mapClients.size() - 1; n >= 0; n--)
			{
				close(m_mapClients[n]->GetClientSock());
				delete m_mapClients[n];
			}
			// 8 关闭套节字closesocket
			close(m_ListenSock);
#endif
			m_ListenSock = INVALID_SOCKET;
			m_mapClients.clear();
		}
	}

	//是否工作中
	bool IsRuning()
	{
		return m_ListenSock != INVALID_SOCKET;
	}

	bool HandleNetData()
	{
		fd_set fdRead;		//描述符（socket） 集合

		while (IsRuning())
		{
			//从缓冲队列里取出客户数据
			if (m_vecClientsBuf.size() > 0)
			{
				std::lock_guard<std::mutex> lock(m_mutexForClientsBuf);
				for (auto pClient : m_vecClientsBuf)
				{
					m_mapClients[pClient->GetClientSock()] = pClient;
				}
				m_vecClientsBuf.clear();
				m_bIsClientNumChange = true;
			}

			//如果没有需要处理的客户端，就跳过
			if (m_mapClients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			FD_ZERO(&fdRead);

			/*
				当有新的客户端加入此Cell服务器时，需更新FDRead集合
			*/
			if (m_bIsClientNumChange)
			{
				m_bIsClientNumChange = false;

				//将描述符（socket）加入集合
				m_maxSock = m_mapClients.begin()->second->GetClientSock();
				for (auto iter : m_mapClients)
				{
					FD_SET(iter.second->GetClientSock(), &fdRead);
					if (m_maxSock < iter.second->GetClientSock())
					{
						m_maxSock = iter.second->GetClientSock();
					}
				}
				memcpy(&m_fdReadBak, &fdRead, sizeof(fd_set));
			}
			else 
			{
				memcpy(&fdRead, &m_fdReadBak, sizeof(fd_set));
			}

			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			int ret = select((int)m_maxSock + 1, &fdRead, nullptr, nullptr, nullptr);
			if (ret < 0)
			{
				printf("CellServer select failure!\n");
				Close();
				return false;
			}
			else if (ret == 0)
			{
				continue;
			}
			
#ifdef _WIN32
			for (unsigned int n = 0; n < fdRead.fd_count; n++)
			{
				auto iter  = m_mapClients.find(fdRead.fd_array[n]);
				if (iter != m_mapClients.end())
				{
					if (-1 == CellServerRecvData(iter->second))
					{
						if (m_pNetEvent)
						{
							m_pNetEvent->OnNetLeave(iter->second);
						}
							
						m_bIsClientNumChange = true;
						m_mapClients.erase(iter->first);
					}
				}
				else
				{
					printf("Find client failure from clientMap! \n");
				}

			}
#else
			std::vector<ClientSocket*> temp;
			for (auto iter : _clients)
			{
				if (FD_ISSET(iter.second->sockfd(), &fdRead))
				{
					if (-1 == RecvData(iter.second))
					{
						if (m_pNetEvent)
							m_pNetEvent->OnNetLeave(iter.second);
						_clients_change = false;
						temp.push_back(iter.second);
					}
				}
			}
			for (auto pClient : temp)
			{
				_clients.erase(pClient->sockfd());
				delete pClient;
			}
#endif
		}
		return true;
	}

	/*
		服务端接收数据：
			1、将接收的数据首先存入每个客户端对象中的第二缓冲区
			2、根据消息长度处理粘包以及拆分包
	*/
	int CellServerRecvData(CClientSocket* pClient)
	{
		// 5 接收客户端数据
		int nLen = (int)recv(pClient->GetClientSock(), m_szRecvMsg, 1, 0);
		if (nLen <= 0)
		{
			printf("客户端<Socket=%u>已退出，任务结束。\n", (unsigned int)pClient->GetClientSock());
			return SOCKET_ERROR;
		}
		m_pNetEvent->OnNetRecv(pClient);


		//将收取到的数据拷贝到消息缓冲区
		memcpy(pClient->GetMsgBuf() + pClient->GetLastPos(), m_szRecvMsg, nLen);
		//消息缓冲区的数据尾部位置后移

		pClient->SetLastPos(pClient->GetLastPos() + nLen);

		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (pClient->GetLastPos() >= sizeof(DataHeader))
		{
			//这时就可以知道当前消息的长度
			DataHeader* header = (DataHeader*)pClient->GetMsgBuf();
			//判断消息缓冲区的数据长度大于消息长度
			if (pClient->GetLastPos() >= header->dataLength)
			{
				//消息缓冲区剩余未处理数据的长度
				int nSize = pClient->GetLastPos() - header->dataLength;
				
				//处理网络消息
				HandleNetMsgAndResponse(pClient, header);
				
				//将消息缓冲区剩余未处理数据前移
				memcpy(pClient->GetMsgBuf(), pClient->GetMsgBuf() + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				pClient->SetLastPos(nSize);

			}
			else 
			{
				//消息缓冲区剩余数据不够一条完整消息
				break;
			}
		}
		return 0;
	}

	


};


class EasyTcpServer : public INetEvent
{
private:
	SOCKET m_listenSocket;
	std::vector<CCellServer*> m_cellServers;		//消息处理对象，内部会创建线程
	
	CELLTimestamp m_tTime;
	bool m_isStopServer;

protected:
	//收到消息计数
	std::atomic_int m_recvMsgCount;
	//客户端计数
	std::atomic_int m_clientCount;
	//服务端Recv调用次数
	std::atomic_int m_recvCount;

public:
	EasyTcpServer()
	{
		m_listenSocket = INVALID_SOCKET;
		m_recvMsgCount = 0;
		m_recvCount = 0;
		m_clientCount = 0; 
		m_isStopServer = false;
	}

	virtual ~EasyTcpServer()
	{
		Close();
	}

	void Start(const char* ip, unsigned short port, unsigned int nCellServer)
	{
		CELLTimestamp RunTime;

		if (false == IsRuning())
		{
			InitSocket();
			Bind(ip, port);
			Listen(5);

			if (m_cellServers.size() != 0)
				m_cellServers.clear();

			for (unsigned int n = 0; n < nCellServer; n++)
			{
				auto ser = new CCellServer(m_listenSocket);
				m_cellServers.push_back(ser);
				//注册网络事件接受对象
				ser->setEventObj(this);
				//启动消息处理线程
				ser->Start();
			}

			m_isStopServer = false;
		}

		while (IsRuning())
		{
			time4msg();

			HandleNetConnection();

			if (m_isStopServer == true)
				break;
		}

		Close();
		return;
	}

	void Stop()
	{
		m_isStopServer = true;
	}

	//只会被一个线程触发 安全
	virtual void OnNetJoin(CClientSocket* pClient)
	{
		m_clientCount++;
	}

	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetLeave(CClientSocket* pClient)
	{
		m_clientCount--;
	}

	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetMsg(CClientSocket* pClient, DataHeader* header)
	{
		m_recvMsgCount++;
	}

private:
	//初始化Socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != m_listenSocket)
		{
			printf("<socket=%d>关闭旧连接...\n", (int)m_listenSocket);
			Close();
		}
		m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == m_listenSocket)
		{
			printf("错误，建立socket失败...\n");
		}
		else 
		{
			printf("建立socket=<%d>成功...\n", (int)m_listenSocket);
		}
		return m_listenSocket;
	}

	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port)
	{
		// 2 bind 绑定用于接受客户端连接的网络端口
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);//host to net unsigned short

#ifdef _WIN32
		if (ip) {
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else
		if (ip) {
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(m_listenSocket, (sockaddr*)&_sin, sizeof(_sin));
		if (SOCKET_ERROR == ret)
		{
			printf("错误,绑定网络端口<%d>失败...\n", port);
		}
		else {
			printf("绑定网络端口<%d>成功...\n", port);
		}
		return ret;
	}

	//监听端口号
	int Listen(int n)
	{
		// 3 listen 监听网络端口
		int ret = listen(m_listenSocket, n);
		if (SOCKET_ERROR == ret)
		{
			printf("socket=<%u>错误,监听网络端口失败...\n", (unsigned int)m_listenSocket);
		}
		else {
			printf("socket=<%u>监听网络端口成功...\n", (unsigned int)m_listenSocket);
		}
		return ret;
	}

	//接受客户端连接
	SOCKET Accept()
	{
		// 4 accept 等待接受客户端连接
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET clientSock = INVALID_SOCKET;
#ifdef _WIN32
		clientSock = accept(m_listenSocket, (sockaddr*)&clientAddr, &nAddrLen);
#else
		clientSock = accept(m_listenSocket, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif
		if (INVALID_SOCKET == clientSock)
		{
			printf("socket=<%d>错误,接受到无效客户端SOCKET...\n", (int)m_listenSocket);
		}
		else
		{
			//将新客户端分配给客户数量最少的cellServer
			addClientToCellServer(new CClientSocket(clientSock));
		}
		return clientSock;
	}

	void addClientToCellServer(CClientSocket* pClient)
	{
		//查找客户数量最少的CellServer消息处理对象
		auto pMinServer = m_cellServers[0];
		for (auto pCellServer : m_cellServers)
		{
			if (pMinServer->getClientCount() > pCellServer->getClientCount())
			{
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);

		OnNetJoin(pClient);
		return;
	}

	//关闭Socket
	void Close()
	{
		if (m_listenSocket != INVALID_SOCKET)
		{
#ifdef _WIN32
			//关闭套节字closesocket
			closesocket(m_listenSocket);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else
			//关闭套节字closesocket
			close(m_listenSocket);
#endif
			m_listenSocket = INVALID_SOCKET;
		}
	}

	//处理网络消息
	void HandleNetConnection()
	{
		fd_set fdRead;
		FD_ZERO(&fdRead);
		FD_SET(m_listenSocket, &fdRead);

		//nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
		//既是所有文件描述符最大值+1 在Windows中这个参数可以写0
		timeval t = {0, 10};
		int ret = select((int)m_listenSocket + 1, &fdRead, 0, 0, &t); //
		if (ret < 0)
		{
			printf("Accept select failure!\n");
			Close();
			return;
		}
		//判断描述符（socket）是否在集合中
		if (FD_ISSET(m_listenSocket, &fdRead))
		{
			FD_CLR(m_listenSocket, &fdRead);
			Accept();
			return;
		}
		return;
	}
	//是否工作中
	bool IsRuning()
	{
		return m_listenSocket != INVALID_SOCKET;
	}

	//计算并输出每秒收到的网络消息
	void time4msg()
	{
		auto t1 = m_tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			printf("thread<%llu>, time<%lf>, socket<%llu>, clients<%d>, recvCnt<%d>, recvMsgCnt<%d>\n", m_cellServers.size(), t1, m_listenSocket, (int)m_clientCount, (int)(m_recvCount / t1), (int)(m_recvMsgCount/ t1));
			m_recvCount = 0;
			m_recvMsgCount = 0;
			m_tTime.update();
		}
	}


};

#endif // !_EasyTcpServer_hpp_
