#pragma comment(lib,"ws2_32")
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE 512

void Listen(SOCKET sock);

void err_quit(const char* msg)
{
	int len = strlen(msg);
	wchar_t buf[BUFSIZE];
	mbstowcs(buf, msg, len);

	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL
	);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, buf, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL
	);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = strlen(buf);

	while (left > 0)
	{
		received = recv(s, ptr, BUFSIZE, flags);
		if (received == SOCKET_ERROR)
		{
			return SOCKET_ERROR;
		}
		else if (received == 0)
		{
			break;
		}
		left -= received;
		ptr += received;
	}

	return (len - left);
}

int main(int argc, char* argv[])
{
	int retval;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return 1;
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		err_quit("socket()");
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		err_quit("connect()");
	}

	char buf[BUFSIZE + 1];
	int len;

	std::thread listen(Listen, sock);

	while (true)
	{
		
		if (fgets(buf, BUFSIZE + 1, stdin) == NULL)
		{
			break;
		}

		len = strlen(buf);
		if (buf[len - 1] == '\n')
		{
			buf[len - 1] = '\0';
		}
		if (strlen(buf) == 0)
		{
			break;
		}

		retval = send(sock, buf, strlen(buf), 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			break;
		}
		//printf("\n[보낼 데이터] ");
		//printf("[TCP 클라이언트] %d 바이트를 보냈습니다.\n", retval);

		//retval = recvn(sock, buf, retval, 0);
		//if (retval == SOCKET_ERROR)
		//{
		//	err_display("recv()");
		//	break;
		//}
		//else if (retval == 0)
		//{
		//	break;
		//}

		//buf[retval] = '\0';
		//printf("[TCP 클라이언트] %d 바이트를 받았습니다.\n", retval);
		//printf("[받은 데이터] %s \n", buf);
	}
	closesocket(sock);
	listen.join();

	WSACleanup();
	return 0;
}

void Listen(SOCKET sock)
{
	int retval = 0;
	char buf[BUFSIZE + 1];
	while (true)
	{
		retval = recv(sock, buf, BUFSIZE, 0);
		if (retval == SOCKET_ERROR)
		{
			break;
		}
		else if (retval == 0)
		{
			break;
		}

		buf[retval] = '\0';
		//printf("\n[TCP 클라이언트] %d 바이트를 받았습니다.", retval);
		printf("[받은 데이터] %s \n", buf);
	}
}