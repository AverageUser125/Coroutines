#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "coroutine.h"
#include "coroutine.c"

#pragma comment(lib, "ws2_32.lib")

#define PORT 12345
#define BUF_SIZE 1024
#define MAX_CLIENTS 512

static Stack *g_stack = NULL;

/* Poll arrays */
static WSAPOLLFD pollfds[MAX_CLIENTS + 1];
static Coroutine *coros[MAX_CLIENTS + 1];
static int poll_count = 0; /* includes server socket at index 0 */

/* ----------------------------------------------------------- */

static void PrintLastError(const char *msg) {
    DWORD id = GetLastError();
    LPSTR buf = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, id,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buf, 0, NULL
    );
    printf("%s: %s\n", msg, buf);
    LocalFree(buf);
}

/* -----------------------------------------------------------
   CLIENT COROUTINE
   ----------------------------------------------------------- */
static void client_coroutine(Stack *stack, void *arg) {
    SOCKET s = (SOCKET)(intptr_t)arg;
    char buf[BUF_SIZE];

    for (;;) {
        int r = recv(s, buf, sizeof(buf), 0);
        if (r > 0) {
            send(s, buf, r, 0);
        } else if (r == 0) {
            /* disconnect */
            break;
        } else {
            PrintLastError("recv");
            break;
        }

        /* allow scheduler to resume others */
        coroutine_yield(stack);
    }

    closesocket(s);
}
/* -----------------------------------------------------------
   ACCEPT COROUTINE
   ----------------------------------------------------------- */
static void accept_coroutine(Stack *stack, void *arg) {
    SOCKET serv = (SOCKET)(intptr_t)arg;

    for (;;) {
		SOCKET c = accept(serv, NULL, NULL);
		if (c == INVALID_SOCKET) {
			PrintLastError("accept");
		} else {
			u_long mode = 1;
			ioctlsocket(c, FIONBIO, &mode);

			if (poll_count < MAX_CLIENTS + 1) {
				pollfds[poll_count].fd = c;
				pollfds[poll_count].events = POLLRDNORM;
				coros[poll_count++] = coroutine_create(g_stack, client_coroutine, (void*)(intptr_t)c);
			} else {
				closesocket(c);
			} 
        }

        /* yield back to scheduler */
        coroutine_yield(stack);
    }
}
/* -----------------------------------------------------------
   REMOVE CLIENT ENTRY
   ----------------------------------------------------------- */
static void remove_client(int index) {
    /* index is in pollfds/coros */

    /* destroy coroutine */
    coroutine_destroy(coros[index]);

    /* remove socket from poll array */
    pollfds[index] = pollfds[poll_count - 1];
    coros[index]    = coros[poll_count - 1];

    poll_count--;
}

/* -----------------------------------------------------------
   MAIN
   ----------------------------------------------------------- */
int main(void) {
    /* winsock */
    {
        WSADATA w;
        assert(WSAStartup(MAKEWORD(2,2), &w) == 0);
    }

    /* server socket */
    SOCKET serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(serv != INVALID_SOCKET);

    {
        struct sockaddr_in a;
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = INADDR_ANY;
        a.sin_port = htons(PORT);

        assert(bind(serv, (struct sockaddr*)&a, sizeof(a)) != SOCKET_ERROR);
        assert(listen(serv, SOMAXCONN) != SOCKET_ERROR);

        u_long mode = 1;
        ioctlsocket(serv, FIONBIO, &mode);
    }
    /* coroutine stack */
    g_stack = coroutine_init();
    assert(g_stack);

    /* setup poll list */
    pollfds[0].fd = serv;
    pollfds[0].events = POLLRDNORM;
    coros[0] = NULL; /* server has no coroutine */
    poll_count = 0;

	coros[poll_count++] = coroutine_create(g_stack, accept_coroutine, (void*)(intptr_t)serv);
    /* event loop */
    for (;;) {
        int r = WSAPoll(pollfds, poll_count, -1);
        if (r == SOCKET_ERROR) {
            PrintLastError("WSAPoll");
            continue;
        }
        /* CLIENTS READY */
        int i = 0;
        while (i < poll_count) {
            if (pollfds[i].revents & POLLRDNORM) {
                Coroutine *co = coros[i];
                coroutine_resume(g_stack, co);

                if (co->finished) {
                    remove_client(i);
                    continue; /* do not increment i (array shifted) */
                }
            }
            i++;
        }
    }

    return 0;
}
