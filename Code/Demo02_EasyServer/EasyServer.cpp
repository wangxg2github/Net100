#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
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
			printf("ERR, sock invalid!");
			break;
		}
		//2��bind ���ڽ��տͻ��˵���������
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(9999);
		_sin.sin_addr.S_un.S_addr = INADDR_ANY;//inet_addr("127.0.0.1");

		if (SOCKET_ERROR == bind(sock, (sockaddr *)&_sin, sizeof(_sin)))
		{
			printf("ERR, bind failure!");
			break;
		}
		//3������
		if (SOCKET_ERROR == listen(sock, 5))
		{
			printf("ERR, listen failure!");
			break;
		}
		printf("�����������ɹ��������˿ڣ�9999��\n");

		//4�����տͻ��˵�����
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET _clientSock = INVALID_SOCKET;
		_clientSock = accept(sock, (sockaddr *)&clientAddr, &nAddrLen);
		if (INVALID_SOCKET == _clientSock)
		{
			printf("ERR, accept invalid client socket!");
			break;
		}
		printf("New client arrived, IP:%s.", inet_ntoa(clientAddr.sin_addr));

		char recvBuf[128] = { 0 };
		char msgBuf[128] = "Hello, Client...";

		while (true)
		{
			//5�����շ�������
			if (0 >= recv(_clientSock, recvBuf, 128, 0))
			{
				printf("Info: client qiut.");
				break;
			}
			printf("���տͻ��˵����%s��\n", recvBuf);

			if (0 == strcmp(recvBuf, "getName"))
			{
				strcpy(msgBuf, "my name is server.");
			}
			else if (0 == strcmp(recvBuf, "getAge"))
			{
				strcpy(msgBuf, "my age is 22.");
			}
			else
			{
				strcpy(msgBuf, "???");
			}
			
			int msgBufLen = (int)strlen(msgBuf) + 1;
			send(_clientSock, msgBuf, msgBufLen, 0);
		}
		
		//�ر��׽���
		closesocket(sock);
	} while (false);

	//���Windows socket 2.x����
	WSACleanup();

	getchar();
	return 0;
}