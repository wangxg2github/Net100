#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <Windows.h>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.lib")

int main()
{
	//启动Windows socket 2.x环境
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	do   /*用Socket API建立简易的TCP服务器*/
	{
		//1、创建套接字
		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			printf("ERR, Sock Invalid!");
			break;
		}
		//2、连接服务器
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(9999);
		_sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

		if (SOCKET_ERROR == connect(sock, (sockaddr *)&_sin, sizeof(sockaddr_in)))
		{
			printf("ERR, connect server failure!");
			break;
		}

		char cmdBuf[128] = { 0 };
		char recvBuf[128] = { 0 };
		while (true)
		{
			scanf("%s", cmdBuf);
			if (0 == strcmp(cmdBuf, "exit"))
			{
				break;
			}
			else
			{
				send(sock, cmdBuf, (int)strlen(cmdBuf) + 1, 0);
				printf("Info, send cmdbuf:%s.\n", cmdBuf);
			}

			//3、接收服务器数据
			int ret = recv(sock, recvBuf, sizeof(recvBuf), 0);
			if (ret > 0)
			{
				printf("Info, recvBuf:%s.\n", recvBuf);
			}
			else
			{
				printf("Err, recv data failure!\n");
			}
		}
		
		//关闭套接字
		closesocket(sock);

		printf("Info, 任务执行结束，客户端推出。\n");
	} while (false);

	//清除Windows socket 2.x环境
	WSACleanup();

	getchar();
	return 0;
}