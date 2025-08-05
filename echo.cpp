#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "V3/coroutine.h"
#include "V3/coroutine.cpp"

#define PORT 12345
#define BUF_SIZE 1024

void PrintLastError(const char* msg) {
	DWORD errorMessageID = GetLastError();
	LPSTR messageBuffer = NULL;
	size_t size =
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					   NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	printf("%s: %s\n", msg, messageBuffer);
	LocalFree(messageBuffer);
}

// Coroutine to handle communication with a client
void client_coroutine(void* arg) {
	SOCKET clientSocket = (SOCKET)(intptr_t)arg;
	char buffer[BUF_SIZE];
	int bytesReceived;

	printf("New Client [%d], [%d]\n", coroutine_id(), clientSocket);
	while (1) {
		coroutine_sleep_read(clientSocket);
		bytesReceived = recv(clientSocket, buffer, BUF_SIZE, 0);

		if (bytesReceived > 0) {
			printf("Client [%d], recieved [%d] bytes\n", coroutine_id(), bytesReceived);

			coroutine_sleep_write(clientSocket);
			send(clientSocket, buffer, bytesReceived, 0);
		} else if (bytesReceived == 0) {
			printf("Client [%d] Disconnected\n", coroutine_id());
			break;
		} else {
			PrintLastError("Recv failed");
			break;
		}
	}
	closesocket(clientSocket);
}

int main() {
	// Initialize Winsock
	{
		WSADATA wsaData;
		int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		assert(wsaResult == 0);
	}

	SOCKET serverSocket;
	// create a listening non-blocking socket on port 12345
	{
		serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		assert(serverSocket != INVALID_SOCKET);

		struct sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(PORT);

		int bindResult = bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
		assert(bindResult != SOCKET_ERROR);

		u_long mode = 1; // Non-blocking mode
		ioctlsocket(serverSocket, FIONBIO, &mode);

		int listenResult = listen(serverSocket, SOMAXCONN);
		assert(listenResult != SOCKET_ERROR);
	}

	coroutine_init();
	while (1) {
		coroutine_sleep_read(serverSocket);
		SOCKET clientSocket = accept(serverSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) {
			PrintLastError("Accept failed");
			continue;
		}

		u_long mode = 1;
		ioctlsocket(clientSocket, FIONBIO, &mode);

		coroutine_go(client_coroutine, (void*)(intptr_t)clientSocket);
	}
	coroutine_destroy();
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}
