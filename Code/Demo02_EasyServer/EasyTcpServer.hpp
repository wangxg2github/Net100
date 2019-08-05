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
	CClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		m_clientSock = sockfd;
		memset(m_szMsgBuf, 0, sizeof(m_szMsgBuf));
		_lastPos = 0;
	}

	SOCKET GetClientSock()
	{
		return m_clientSock;
	}

	char* GetMsgBuf()
	{
		return m_szMsgBuf;
	}

	int GetLastPos()
	{
		return _lastPos;
	}
	void SetLastPos(int pos)
	{
		_lastPos = pos;
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
	SOCKET m_clientSock;
	char m_szMsgBuf[RECV_BUFF_SZIE];		//第二缓冲区 消息缓冲区
	int _lastPos;	//消息缓冲区的数据尾部位置
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
private:

};

class CCellServer
{
public:
	CCellServer(SOCKET listenSock)
	{
		m_ListenSock = listenSock;
		_pThread = nullptr;
		_pNetEvent = nullptr;
	}

	~CCellServer()
	{
		Close();
		m_ListenSock = INVALID_SOCKET;
	}

	void setEventObj(INetEvent* event)
	{
		_pNetEvent = event;
	}

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
			m_mapClients.clear();
		}
	}

	//是否工作中
	bool isRun()
	{
		return m_ListenSock != INVALID_SOCKET;
	}


	bool OnRun()
	{
		m_bIsClientNumChange = true;
		while (isRun())
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

			//伯克利套接字 BSD socket
			fd_set fdRead;//描述符（socket） 集合
			//清理集合
			FD_ZERO(&fdRead);
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
			else {
				memcpy(&fdRead, &m_fdReadBak, sizeof(fd_set));
			}

			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			int ret = select((int)m_maxSock + 1, &fdRead, nullptr, nullptr, nullptr);
			if (ret < 0)
			{
				printf("select任务结束。\n");
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
					if (-1 == RecvData(iter->second))
					{
						if (_pNetEvent)
							_pNetEvent->OnNetLeave(iter->second);
						m_bIsClientNumChange = true;
						m_mapClients.erase(iter->first);
					}
				}else
				{
					printf("error. if (iter != _clients.end())\n");
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
						if (_pNetEvent)
							_pNetEvent->OnNetLeave(iter.second);
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

	//缓冲区
	char _szRecv[RECV_BUFF_SZIE] = {};
	//接收数据 处理粘包 拆分包
	int RecvData(CClientSocket* pClient)
	{
		// 5 接收客户端数据
		int nLen = (int)recv(pClient->GetClientSock(), _szRecv, RECV_BUFF_SZIE, 0);
		//printf("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			printf("客户端<Socket=%u>已退出，任务结束。\n", (unsigned int)pClient->GetClientSock());
			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		memcpy(pClient->GetMsgBuf() + pClient->GetLastPos(), _szRecv, nLen);
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
				OnNetMsg(pClient, header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(pClient->GetMsgBuf(), pClient->GetMsgBuf() + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				pClient->SetLastPos(nSize);
			}
			else {
				//消息缓冲区剩余数据不够一条完整消息
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(CClientSocket* pClient, DataHeader* header)
	{
		_pNetEvent->OnNetMsg(pClient, header);
	}

	void addClient(CClientSocket* pClient)
	{
		std::lock_guard<std::mutex> lock(m_mutexForClientsBuf);
		m_vecClientsBuf.push_back(pClient);
		return;
	}

	void Start()
	{
		_pThread = new std::thread(std::mem_fun(&CCellServer::OnRun), this);
	}

	size_t getClientCount()
	{
		return m_mapClients.size() + m_vecClientsBuf.size();
	}

private:
	SOCKET m_ListenSock;							//服务端监听Socket
	std::thread* _pThread;

	std::mutex m_mutexForClientsBuf;				//将缓冲队列中的客户端添加到正式队列，用此锁
	std::map<SOCKET, CClientSocket*> m_mapClients;	//正式客户队列
	std::vector<CClientSocket*> m_vecClientsBuf;	//缓冲客户队列
	
	fd_set m_fdReadBak;								//备份客户端所有socket
	bool m_bIsClientNumChange;						//客户列表是否有变化
	SOCKET m_maxSock;
	
	//网络事件对象
	INetEvent* _pNetEvent;
};


class EasyTcpServer : public INetEvent
{
private:
	SOCKET _sock;
	//消息处理对象，内部会创建线程
	std::vector<CCellServer*> _cellServers;
	//每秒消息计时
	CELLTimestamp _tTime;
protected:
	//收到消息计数
	std::atomic_int _recvCount;
	//客户端计数
	std::atomic_int _clientCount;
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
		_recvCount = 0;
		_clientCount = 0;
	}
	virtual ~EasyTcpServer()
	{
		Close();
	}
	//初始化Socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			printf("<socket=%d>关闭旧连接...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("错误，建立socket失败...\n");
		}
		else {
			printf("建立socket=<%d>成功...\n", (int)_sock);
		}
		return _sock;
	}

	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port)
	{
		//if (INVALID_SOCKET == _sock)
		//{
		//	InitSocket();
		//}
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
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
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
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("socket=<%u>错误,监听网络端口失败...\n", (unsigned int)_sock);
		}
		else {
			printf("socket=<%u>监听网络端口成功...\n", (unsigned int)_sock);
		}
		return ret;
	}

	//接受客户端连接
	SOCKET Accept()
	{
		// 4 accept 等待接受客户端连接
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif
		if (INVALID_SOCKET == cSock)
		{
			printf("socket=<%d>错误,接受到无效客户端SOCKET...\n", (int)_sock);
		}
		else
		{
			//将新客户端分配给客户数量最少的cellServer
			addClientToCellServer(new CClientSocket(cSock));
			//获取IP地址 inet_ntoa(clientAddr.sin_addr)
		}
		return cSock;
	}

	void addClientToCellServer(CClientSocket* pClient)
	{
		//查找客户数量最少的CellServer消息处理对象
		auto pMinServer = _cellServers[0];
		for (auto pCellServer : _cellServers)
		{
			if (pMinServer->getClientCount() > pCellServer->getClientCount())
			{
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
		OnNetJoin(pClient);
	}

	void Start(int nCellServer)
	{
		for (int n = 0; n < nCellServer; n++)
		{
			auto ser = new CCellServer(_sock);
			_cellServers.push_back(ser);
			//注册网络事件接受对象
			ser->setEventObj(this);
			//启动消息处理线程
			ser->Start();
		}
	}
	//关闭Socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			//关闭套节字closesocket
			closesocket(_sock);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else
			//关闭套节字closesocket
			close(_sock);
#endif
		}
	}
	//处理网络消息
	bool OnRun()
	{
		if (isRun())
		{
			time4msg();
			//伯克利套接字 BSD socket
			fd_set fdRead;//描述符（socket） 集合
			//清理集合
			FD_ZERO(&fdRead);
			//将描述符（socket）加入集合
			FD_SET(_sock, &fdRead);
			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			timeval t = { 0,10};
			int ret = select((int)_sock + 1, &fdRead, 0, 0, &t); //
			if (ret < 0)
			{
				printf("Accept Select任务结束。\n");
				Close();
				return false;
			}
			//判断描述符（socket）是否在集合中
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				Accept();
				return true;
			}
			return true;
		}
		return false;
	}
	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//计算并输出每秒收到的网络消息
	void time4msg()
	{
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			printf("thread<%llu>, time<%lf>, socket<%llu>, clients<%d>, recvCount<%d>\n", _cellServers.size(), t1, _sock, (int)_clientCount, (int)(_recvCount/ t1));
			_recvCount = 0;
			_tTime.update();
		}
	}
	//只会被一个线程触发 安全
	virtual void OnNetJoin(CClientSocket* pClient)
	{
		_clientCount++;
	}
	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetLeave(CClientSocket* pClient)
	{
		_clientCount--;
	}
	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetMsg(CClientSocket* pClient, DataHeader* header)
	{
		_recvCount++;
	}
};

#endif // !_EasyTcpServer_hpp_
