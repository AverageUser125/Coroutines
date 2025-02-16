#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#include "V2/coroutine.h"
#include "V2/coroutine.c"

#define PORT 12345
#define BUF_SIZE 1024

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
char* GetLastErrorAsString() {
	DWORD errorMessageID = GetLastError();
	if (errorMessageID == 0) {
		return NULL; //No error message has been recorded
	}

	LPSTR messageBuffer = NULL;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size =
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					   NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	return messageBuffer;
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
			char* err = GetLastErrorAsString();
			fprintf(stderr, "Recv failed: %s\n", err);
			LocalFree(err);
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
			char* err = GetLastErrorAsString();
			fprintf(stderr, "Accept failed: %s\n", err);
			LocalFree(err);
			continue;
		}

		u_long mode = 1;
		ioctlsocket(clientSocket, FIONBIO, &mode);

		coroutine_go(client_coroutine, (void*)(intptr_t)clientSocket);
	}

	closesocket(serverSocket);
	WSACleanup();
	return 0;
}
