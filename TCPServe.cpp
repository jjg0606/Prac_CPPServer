#pragma comment(lib,"ws2_32")
#include <WinSock2.h>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <list>
#include <mutex>

#define SERVERPORT 9000
#define BUFSIZE 512

void err_quit(const char* msg);
void err_display(const char* msg);
void server_listen(SOCKET client_sock, SOCKADDR_IN clientaddr);
void BroadCast(char* msg);

std::list<SOCKET> clisock;

std::mutex CriticalSession;

int main(int argc, char* argv[])
{
	int retval;
	std::vector<std::thread*> threadVec;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa)  != 0)
	{
		return 1;
	}
	// socket
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		err_quit("socket()");
	}
	//bind
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		err_quit("bind()");
	}
	//listen
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		err_quit("listen()");
	}

	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	char buf[BUFSIZE + 1];

	while (true)
	{
		//accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
		}

		printf("\n[TCP 서버] 클라이언트 접속 : IP주소 %s, 포트번호 %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
		std::thread* listenThread = new std::thread(server_listen,client_sock,clientaddr);
		threadVec.push_back(listenThread);		
		
	}

	closesocket(listen_sock);
	for (int i = 0; i < threadVec.size(); i++)
	{
		delete threadVec[i];
	}
	//
	WSACleanup();
	return 0;
}


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

void server_listen(SOCKET client_sock, SOCKADDR_IN clientaddr)
{
	CriticalSession.lock();
	clisock.push_back(client_sock);
	CriticalSession.unlock();
	int retval;
	char buf[BUFSIZE + 1];
	while (true)
	{
		retval = recv(client_sock, buf, BUFSIZE, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			break;
		}
		else if (retval == 0)
		{
			break;
		}

		buf[retval] = '\0';
		printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), buf);
		// send
		/*retval = send(client_sock, buf, retval, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
		}*/
		BroadCast(buf);
	}
	// close
	closesocket(client_sock);
	CriticalSession.lock();
	for (auto iter = clisock.begin(); iter != clisock.end(); iter++)
	{
		if (*iter == client_sock)
		{
			clisock.erase(iter);
			break;
		}
	}
	CriticalSession.unlock();
	printf("[TCP 서버] 클라이언트 종료 : IP주소 %s, 포트번호 %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
}

void BroadCast(char* msg)
{
	CriticalSession.lock();
	int len = strlen(msg);
	for (auto iter = clisock.begin(); iter != clisock.end(); iter++)
	{
		send(*iter, msg, len, 0);
	}
	CriticalSession.unlock();
}