#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "V2/coroutine.h"

#pragma comment(lib, "ws2_32.lib")

#include "V2/coroutine.c"

#define PORT 12345
#define BUF_SIZE 1024

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
			fprintf(stderr, "Recv failed: %d\n", WSAGetLastError());
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
	// create a listening socket on port 12345
	{
		// Create a server socket
		serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		assert(serverSocket != INVALID_SOCKET);

		// Set server address
		struct sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(PORT);

		// Bind the server socket
		int bindResult = bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
		assert(bindResult != SOCKET_ERROR);

		// Set the socket to non-blocking mode
		u_long mode = 1; // Non-blocking mode
		ioctlsocket(serverSocket, FIONBIO, &mode);

		// Listen for incoming connections
		int listenResult = listen(serverSocket, SOMAXCONN);
		assert(listenResult != SOCKET_ERROR);
	}

	coroutine_init();
	while (1) {
		coroutine_sleep_read(serverSocket);
		SOCKET clientSocket = accept(serverSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) {
			fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
			continue;
		}
		coroutine_go(client_coroutine, (void*)(intptr_t)clientSocket);
	}

	closesocket(serverSocket);
	WSACleanup();
	return 0;
}
