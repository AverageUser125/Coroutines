#ifndef COROUTINE_PANIM_H_
#define COROUTINE_PANIM_H_

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	void* rsp;
	void* stack_base;
	bool finished;
} Coroutine;

// TODO: make the STACK_CAPACITY customizable by the user
#define STACK_CAPACITY (4 * 1024 * 1024)

typedef struct {
	Coroutine** items;
	size_t count;
	size_t capacity;
} Stack;

Stack* coroutine_init(void);
void coroutine_deinit(Stack* stack);
Coroutine* coroutine_create(Stack* stack, void (*f)(Stack*, void*), void* arg);
void coroutine_destroy(Coroutine* c);
void coroutine_resume(Stack* stack, Coroutine* c);
void coroutine_yield(Stack* stack);

#endif // COROUTINE_PANIM_H_