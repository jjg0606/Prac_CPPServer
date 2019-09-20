#pragma comment(lib,"ws2_32")
#include <WinSock2.h>
#include <thread>
#include <stdlib.h>
#include <stdio.h>


#define SERVERPORT 9000
#define BUFSIZE 512
#define WM_SOCKET (WM_USER + 1)

struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFSIZE + 1];
	int recvbytes;
	int sendbytes;
	BOOL recvdelayed;
	SOCKETINFO *next;
};

SOCKETINFO* SocketInfoList;
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
void ProcessSocketMessage(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

BOOL AddSocketInfo(SOCKET sock);
SOCKETINFO* GetSocketInfo(SOCKET sock);
void RemoveSocketInfo(SOCKET sock);

void err_quit(const char* msg);
void err_display(const char* msg);
void err_display(int errcode);

int main()
{
	int retval;
	WNDCLASS WndClass;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = NULL;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = L"MyWndClass";

	if (!RegisterClass(&WndClass))
	{
		return 1;
	}

	HWND hWnd = CreateWindow(L"MyWndClass", L"TCP server", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, (HMENU)NULL, NULL, NULL);
	if (hWnd == NULL)
	{
		return 1;
	}

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return 1;
	}

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		err_quit("socket()");
	}

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

	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		err_quit("listen()");
	}

	retval = WSAAsyncSelect(listen_sock, hWnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE);
	if (retval == SOCKET_ERROR)
	{
		err_quit("WSAAsyncSelect()");
	}

	MSG msg;
	while (GetMessage(&msg, 0, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	WSACleanup();
	return msg.wParam;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	switch (iMessage)
	{
	case WM_SOCKET:
		ProcessSocketMessage(hWnd, iMessage, wParam, lParam);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	
	}
	return (DefWindowProc(hWnd, iMessage, wParam, lParam));
}

void ProcessSocketMessage(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	SOCKETINFO* ptr;
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen, retval;

	if (WSAGETSELECTERROR(lParam))
	{
		err_display(WSAGETSELECTERROR(lParam));
		RemoveSocketInfo(wParam);
		return;
	}

	switch (WSAGETSELECTEVENT(lParam))
	{
	case FD_ACCEPT:
		addrlen = sizeof(clientaddr);
		client_sock = accept(wParam, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			return;
		}
		printf("\n[TCP 서버] 클라이언트 접속 : IP주소 %s, 포트번호 %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
		AddSocketInfo(client_sock);
		retval = WSAAsyncSelect(client_sock, hWnd, WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE);
		if (retval == SOCKET_ERROR)
		{
			err_display("WSAAsyncSelect()");
			RemoveSocketInfo(client_sock);
		}
		break;
	case FD_READ:
		ptr = GetSocketInfo(wParam);
		if (ptr->recvbytes > 0)
		{
			ptr->recvdelayed = true;
			return;
		}
		// recv data
		retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			RemoveSocketInfo(wParam);
			return;
		}
		ptr->recvbytes = retval;
		// output data
		ptr->buf[retval] = '\0';
		addrlen = sizeof(clientaddr);
		getpeername(wParam, (SOCKADDR*)&clientaddr, &addrlen);
		printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), ptr->buf);
//
	case FD_WRITE:
		ptr = GetSocketInfo(wParam);
		if (ptr->recvbytes <= ptr->sendbytes)
		{
			return;
		}
		// send data
		retval = send(ptr->sock, ptr->buf + ptr->sendbytes, ptr->recvbytes - ptr->sendbytes, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			RemoveSocketInfo(wParam);
			return;
		}
		ptr->sendbytes += retval;
		if (ptr->recvbytes == ptr->sendbytes)
		{
			ptr->recvbytes = ptr->sendbytes = 0;
			if(ptr->recvdelayed)
			{
				ptr->recvdelayed = false;
				PostMessage(hWnd, WM_SOCKET, wParam, FD_READ);
			}
		}
		break;
	case FD_CLOSE:
		RemoveSocketInfo(wParam);
		break;
	}
}

BOOL AddSocketInfo(SOCKET sock)
{
	SOCKETINFO* ptr = new SOCKETINFO();
	if (ptr == nullptr)
	{
		return false;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	ptr->recvdelayed = false;
	ptr->next = SocketInfoList;
	SocketInfoList = ptr;
	return true;
}

SOCKETINFO* GetSocketInfo(SOCKET sock)
{
	SOCKETINFO* ptr = SocketInfoList;
	while (ptr!=nullptr)
	{
		if (ptr->sock == sock)
		{
			return ptr;
		}
		ptr = ptr->next;
	}
	return nullptr;
}

void RemoveSocketInfo(SOCKET sock)
{
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(sock, (SOCKADDR*)&clientaddr, &addrlen);
	printf("[TCP 서버] 클라이언트 종료 : IP %s, 포트번호 %d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	SOCKETINFO* curr = SocketInfoList;
	SOCKETINFO* prev = NULL;

	while (curr != nullptr)
	{
		if (curr->sock == sock)
		{
			if (prev != nullptr)
			{
				prev->next = curr->next;
			}
			else
			{
				SocketInfoList = curr->next;
			}
			closesocket(curr->sock);
			delete curr;
			return;
		}
		prev = curr;
		curr = curr->next;
	}
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

void err_display(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL
	);
	printf("[오류] %s", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}
