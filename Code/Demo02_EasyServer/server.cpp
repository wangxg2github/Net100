#include "EasyTcpServer.hpp"
//#include "MemoryPool/MemoryAlloc.hpp"

#include<thread>

class MyServer : public EasyTcpServer
{
public:

	//只会被一个线程触发 安全
	virtual void OnNetJoin(CClientSocket* pClient)
	{
		m_clientCount++;
		//printf("client<%llu> join\n", pClient->GetClientSock());
	}
	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetLeave(CClientSocket* pClient)
	{
		m_clientCount--;
		//printf("client<%llu> leave\n", pClient->GetClientSock());
	}

	virtual void OnNetRecv(CClientSocket* pClient)
	{
		m_recvCount++;
	}

	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetMsg(CCellServer* pServer, CClientSocket* pClient, DataHeader* header)
	{
		m_recvMsgCount++;

		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//printf("收到客户端请求：CMD_LOGIN,数据长度：%d,userName=%s PassWord=%s\n", login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程
			//LoginResult ret;
			//pClient->SendData(&ret);

			pServer->AddSendTask(pClient, header);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("收到客户端请求：CMD_LOGOUT,数据长度：%d,userName=%s \n", logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程
			//LogoutResult ret;
			//SendData(cSock, &ret);
		}
		break;
		default:
		{
			printf("<socket=%llu>收到未定义消息,数据长度：%d.\n", pClient->GetClientSock(), header->dataLength);
			//DataHeader ret;
			//SendData(cSock, &ret);
		}
		break;
		}
	}
private:

};


MyServer g_EasyTcpServer;

void cmdThread()
{
	while (true)
	{
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_EasyTcpServer.Stop();
			printf("退出cmdThread线程\n");
			break;
		}
		else {
			printf("不支持的命令。\n");
		}
	}
}

int main()
{
	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	g_EasyTcpServer.Start(nullptr, 4567, 4);


	printf("exit.\n");
	getchar();
	return 0;
}