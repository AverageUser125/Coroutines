#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// Initial capacity of a dynamic array
#define DA_INIT_CAP 16
// Append an item to a dynamic array
#define da_append(da, item)                                                                                            \
	do {                                                                                                               \
		if ((da)->count >= (da)->capacity) {                                                                           \
			(da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity * 2;                                   \
			(da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items));                                 \
			assert((da)->items != NULL && "Buy more RAM lol");                                                         \
		}                                                                                                              \
                                                                                                                       \
		(da)->items[(da)->count++] = (item);                                                                           \
	} while (0)

#define da_destroy(da)                                                                                                 \
	do {                                                                                                               \
		free((da)->items);                                                                                             \
		(da)->items = NULL;                                                                                            \
		(da)->count = 0;                                                                                               \
		(da)->capacity = 0;                                                                                            \
	} while (0)

#define PAGE_SIZE (4096)
#define STACK_CAPACITY (20 * PAGE_SIZE)

#define UNUSED(x) (void)(x)
#define UNREACHABLE(message)                                                                                           \
	do {                                                                                                               \
		fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message);                                      \
		abort();                                                                                                       \
	} while (0)

typedef struct {
	void* rsp;
	void* stack_base;
} Context;

typedef struct {
	Context* items;
	size_t count;
	size_t capacity;
	size_t current;
} Contexts;

Contexts contexts = {0};

void coroutine_switch_context(void* rsp);

void __attribute__((naked)) coroutine_yield(void) {
	// @arch
	asm("    pushq %rcx\n"
		"    pushq %rbp\n"
		"    pushq %rdi\n"
		"    pushq %rsi\n"
		"    pushq %rbx\n"
		"    pushq %r12\n"
		"    pushq %r13\n"
		"    pushq %r14\n"
		"    pushq %r15\n"
		"    movq %rsp, %rcx\n"				   // Move the stack pointer (rsp) to RCX
		"    jmp coroutine_switch_context\n"); // Pass RCX (rsp) to coroutine_switch_context
}

void __attribute__((naked)) coroutine_restore_context(void* rsp) {
	UNUSED(rsp);
	// @arch
	asm("    movq %rcx, %rsp\n" // Restore the stack pointer from RCX (not RDI)
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

// must exist even if GCC doesn't know it is used in coroutine_yield
void __attribute__((used)) coroutine_switch_context(void* rsp) {
	contexts.items[contexts.current].rsp = rsp;
	contexts.current = (contexts.current + 1) % contexts.count;
	coroutine_restore_context(contexts.items[contexts.current].rsp);
}

void coroutine_init(void) {
	da_append(&contexts, (Context){0});
}

static void coroutine__finish_current(void) {
	if (contexts.current == 0) {
		UNREACHABLE("Main Coroutine with id == 0 should never reach this place");
	}
	// remove by swaping with the end and then reducing the size by 1,
	Context t = contexts.items[contexts.current];
	contexts.items[contexts.current] = contexts.items[contexts.count - 1];
	contexts.items[contexts.count - 1] = t;
	contexts.count -= 1;
	contexts.current %= contexts.count;
	coroutine_restore_context(contexts.items[contexts.current].rsp);
}

void coroutine_go(void (*f)(void*), void* arg) {
	void* stack_base =
		(void*)(VirtualAlloc(NULL, STACK_CAPACITY + PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	assert(stack_base != NULL && "Buy more RAM lol");
	void** rsp = (void**)((char*)stack_base + STACK_CAPACITY);

	// @arch
	*(--rsp) = coroutine__finish_current; // Return address for when coroutine finishes
	*(--rsp) = f;						  // Function to call
	*(--rsp) = arg;						  // RCX
	*(--rsp) = 0;						  // RBP
	*(--rsp) = 0;						  // RDI
	*(--rsp) = 0;						  // RSI
	*(--rsp) = 0;						  // RBX
	*(--rsp) = 0;						  // R12
	*(--rsp) = 0;						  // R13
	*(--rsp) = 0;						  // R14
	*(--rsp) = 0;						  // R15

	da_append(&contexts, ((Context){
							 .rsp = rsp,
							 .stack_base = stack_base,
						 }));
}

size_t coroutine_id(void) {
	return contexts.current;
}

size_t coroutine_alive(void) {
	return contexts.count;
}

void coroutine_destroy() {
	if (contexts.current != 0) {
		UNREACHABLE("Must be called from main routine");
	}

	for (size_t i = 1; i < contexts.count; i++) {
		BOOLEAN result = VirtualFree(contexts.items[i].stack_base, 0, MEM_RELEASE);
		assert(result != 0);
	}

	da_destroy(&contexts);
}
