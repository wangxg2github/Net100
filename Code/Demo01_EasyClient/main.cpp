#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <Windows.h>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.lib")

int main()
{
	//����Windows socket 2.x����
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	do   /*��Socket API�������׵�TCP������*/
	{
		//1�������׽���
		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			printf("ERR, Sock Invalid!");
			break;
		}
		//2�����ӷ�����
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

			//3�����շ���������
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
		
		//�ر��׽���
		closesocket(sock);

		printf("Info, ����ִ�н������ͻ����Ƴ���\n");
	} while (false);

	//���Windows socket 2.x����
	WSACleanup();

	getchar();
	return 0;
}