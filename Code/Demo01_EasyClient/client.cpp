#include "EasyTcpClient.hpp"
#include "CELLTimestamp.hpp"
#include <thread>
#include <atomic>

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			printf("退出cmdThread线程\n");
			break;
		}
		else {
			printf("不支持的命令。\n");
		}
	}
}

//客户端数量
const int cCount = 40;
//发送线程数量
const int tCount = 4;
//客户端数组
EasyTcpClient* client[cCount];

//客户端Send次数
std::atomic_int g_sendCount;
std::atomic_int g_readyCount;

void sendThread(int id)
{
	//4个线程 ID 1~4
	int c = cCount / tCount;
	int begin = (id - 1) * c;
	int end = id * c;

	for (int n = begin; n < end; n++)
	{
		client[n] = new EasyTcpClient();
	}
	for (int n = begin; n < end; n++)
	{
#define CONNECT_WIN
#ifdef CONNECT_WIN
		client[n]->Connect("127.0.0.1", 4567);
#else
		client[n]->Connect("192.168.154.147", 4567);
#endif // CONNECT_WIN
		printf("thread<%d>,Connect=%d\n", id, n);
	}

	//使用readyCount等待所有线程链接完成
	g_readyCount++;
	while (g_readyCount < tCount)
	{
		std::chrono::milliseconds t(10);
		std::this_thread::sleep_for(t);
	}

#define CLIENTSENDPKTCOUNT 1
	Login login[CLIENTSENDPKTCOUNT];
	for (int n = 0; n < CLIENTSENDPKTCOUNT; n++)
	{
		strcpy(login[n].userName, "lyd");
		strcpy(login[n].PassWord, "lydmm");
	}
	const int nLen = sizeof(login);
	while (g_bRun)
	{
		for (int n = begin; n < end; n++)
		{
			if (SOCKET_ERROR != client[n]->SendData(login, nLen))
				g_sendCount++;

			//客户端接收数据
			//client[n]->OnRun();
		}
	}

	for (int n = begin; n < end; n++)
	{
		client[n]->Close();
		delete client[n];
	}
}

int main()
{
	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	//启动发送线程
	for (int n = 0; n < tCount; n++)
	{
		std::thread t1(sendThread, n + 1);
		t1.detach();
	}

	CELLTimestamp tTime;
	while (g_bRun)
	{
		auto t = tTime.getElapsedSecond();
		if (t > 1.0)
		{
			printf("thread<%d>, client<%d>, time<%lf>, sendCnt<%d>\n", tCount, cCount, t, (int)(g_sendCount / t));
			g_sendCount = 0;
			tTime.update();
		}
	}
	Sleep(100);

	printf("已退出。\n");
	return 0;
}