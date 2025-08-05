#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <Windows.h>

#include "coroutine.h"

// TODO: make the STACK_CAPACITY customizable by the user
#define PAGE_SIZE (4096)
#define STACK_CAPACITY (1024 * PAGE_SIZE)

#define UNUSED(x) (void)(x)
#define TODO(message)                                                                                                  \
	do {                                                                                                               \
		fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message);                                             \
		abort();                                                                                                       \
	} while (0)
#define UNREACHABLE(message)                                                                                           \
	do {                                                                                                               \
		fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message);                                      \
		abort();                                                                                                       \
	} while (0)

typedef struct {
	void* rsp;
	void* stack_base;
} Context;

// TODO: coroutines library probably does not work well in multithreaded environment
static size_t current = 0;
static std::vector<size_t> active = {};
static std::vector<size_t> dead = {};
static std::vector<Context> contexts = {};
static std::vector<size_t> asleep = {};
static std::vector<struct pollfd> polls = {};

typedef enum {
	SM_NONE = 0,
	SM_READ,
	SM_WRITE,
} Sleep_Mode;

// Windows x86_64 call convention
// %rcx, %rdx, %r8, %r9

// @arch
#define PUSHALL                                                                                                        \
	"    pushq %rcx\n"                                                                                                 \
	"    pushq %rbp\n"                                                                                                 \
	"    pushq %rdi\n"                                                                                                 \
	"    pushq %rsi\n"                                                                                                 \
	"    pushq %rbx\n"                                                                                                 \
	"    pushq %r12\n"                                                                                                 \
	"    pushq %r13\n"                                                                                                 \
	"    pushq %r14\n"                                                                                                 \
	"    pushq %r15\n"

extern "C" void coroutine_switch_context(void* rsp, Sleep_Mode sm, int fd);

void __attribute__((naked)) coroutine_yield(void) {
	// @arch
	asm(PUSHALL "    movq %rsp, %rcx\n" // rsp
				"    movq $0, %rdx\n"	// sm = SM_NONE
				"    jmp coroutine_switch_context\n");
}

void __attribute__((naked)) coroutine_sleep_read(unsigned int fd) {
	UNUSED(fd);
	// @arch
	asm(PUSHALL "    movq %rcx, %r8\n"	// fd
				"    movq %rsp, %rcx\n" // rsp
				"    movq $1, %rdx\n"	// sm = SM_READ
				"    jmp coroutine_switch_context\n");
}

void __attribute__((naked)) coroutine_sleep_write(unsigned int fd) {
	UNUSED(fd);
	// @arch
	asm(PUSHALL "    movq %rcx, %r8\n"	// fd
				"    movq %rsp, %rcx\n" // rsp
				"    movq $2, %rdx\n"	// sm = SM_WRITE
				"    jmp coroutine_switch_context\n");
}

static void __attribute__((naked)) coroutine_restore_context(void* rsp) {
	// @arch
	UNUSED(rsp);
	asm("    movq %rcx, %rsp\n"
		"    popq %r15\n"
		"    popq %r14\n"
		"    popq %r13\n"
		"    popq %r12\n"
		"    popq %rbx\n"
		"    popq %rsi\n"
		"    popq %rdi\n"
		"    popq %rbp\n"
		"    popq %rcx\n"
		"    ret\n");
}
template <typename T>
void da_remove_unordered(std::vector<T>* da, int i) {
	assert((i) < (da)->size());                                                                                     \
	if ((i) != (da)->size() - 1) {                                                                                  \
		(da)->at((i)) = (da)->at((da)->size() - 1);                                                           \
	}                                                                                                              \
	(da)->pop_back();                                                                                               \
}

static void coroutine_update_sleeping() {
	// @speed to activate a sleeping coroutine is linear
	if (polls.empty()) {
		return;
	}
	int timeout = active.empty() ? -1 : 0;
	int result = WSAPoll(polls.data(), polls.size(), timeout);
	if (result < 0)
		TODO("poll");

	if (polls.empty()) {
		return;
	}
	for (int i = polls.size() - 1; i >= 0; --i) {
		if (polls[i].revents) {
			size_t id = asleep[i];
			da_remove_unordered(&polls, i);
			da_remove_unordered(&asleep, i);
			active.emplace_back(id);
	}
	}
}
extern "C" __attribute__((unused)) __attribute__ ((noinline)) void coroutine_switch_context(void* rsp, Sleep_Mode sm, int fd) {
	contexts[active[current]].rsp = rsp;

	switch (sm) {
	case SM_NONE:
		current += 1;
		break;
	case SM_READ: {
		assert(fd != SOCKET_ERROR);
		asleep.emplace_back(active[current]);
		struct pollfd pfd = {
			.fd = (SOCKET)fd,
			.events = POLLRDNORM,
		};
		polls.emplace_back(pfd);
		da_remove_unordered(&active, current);
	} break;

	case SM_WRITE: {
		assert(fd != SOCKET_ERROR);
		asleep.emplace_back(active[current]);
		struct pollfd pfd = {
			.fd = (SOCKET)fd,
			.events = POLLWRNORM,
		};
		polls.emplace_back(pfd);
		da_remove_unordered(&active, current);
	} break;

	default:
		UNREACHABLE("coroutine_switch_context");
	}

	coroutine_update_sleeping();

	assert(!active.empty());
	current %= active.size();
	coroutine_restore_context(contexts[active[current]].rsp);
}

void coroutine_init(void) {
	if (!contexts.empty())
		return;
	contexts.emplace_back((Context){0});
	active.emplace_back(0);
}

static void coroutine__finish_current(void) {
	if (active[current] == 0) {
		UNREACHABLE("Main Coroutine with id == 0 should never reach this place");
	}
	dead.emplace_back(active[current]);
	da_remove_unordered(&active, current);

	coroutine_update_sleeping();

	assert(!active.empty());
	current %= active.size();
	coroutine_restore_context(contexts[active[current]].rsp);
}

void coroutine_go(void (*f)(void*), void* arg) {
	size_t id;
	if (!dead.empty()) {
		id = dead.back();
		dead.pop_back();
	} else {
		Context& ctx = contexts.emplace_back((Context){0});
		id = contexts.size() - 1;
		ctx.stack_base =
			(void*)(VirtualAlloc(NULL, STACK_CAPACITY + PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		assert(ctx.stack_base != NULL);
	}

	void** rsp = (void**)((char*)contexts[id].stack_base + STACK_CAPACITY);

	// @arch
	*(--rsp) = (void*)coroutine__finish_current;
	*(--rsp) = (void*)f;
	*(--rsp) = arg; // RCX
	*(--rsp) = 0;	// RBP
	*(--rsp) = 0;	// RDI
	*(--rsp) = 0;	// RSI
	*(--rsp) = 0;	// RBX
	*(--rsp) = 0;	// R12
	*(--rsp) = 0;	// R13
	*(--rsp) = 0;	// R14
	*(--rsp) = 0;	// R15

	contexts[id].rsp = rsp;
	active.emplace_back(id);
}

size_t coroutine_id(void) {
	return active[current];
}

size_t coroutine_alive(void) {
	return active.size();
}

void coroutine_wake_up(size_t id) {
	// @speed coroutine_wake_up is linear
	for (size_t i = 0; i < asleep.size(); ++i) {
		if (asleep[i] == id) {
			da_remove_unordered(&asleep, i);
			da_remove_unordered(&polls, i);
			active.emplace_back(id);
			return;
		}
	}
}

void coroutine_destroy() {
	if (active[current] != 0) {
		UNREACHABLE("Must be called from main routine");
	}
	active.~vector();
	dead.~vector();
	asleep.~vector();
	polls.~vector();

	for (size_t i = 1; i < contexts.size(); i++) {
		BOOLEAN result = VirtualFree(contexts[i].stack_base, 0, MEM_RELEASE);
		assert(result != 0);
	}
	contexts.~vector();
}
