#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "V2/coroutine.h"
#include "V2/coroutine.c"

void f0() {
	printf("0\n");
	coroutine_yield();
}

void f1(void* arg1) {
	printf("1: %zu\n", (intptr_t)arg1);
	coroutine_yield();
}

void f2(void* arg1, void* arg2) {
	printf("2: %zu %zu\n", (intptr_t)arg1, (intptr_t)arg2);
	coroutine_yield();
}

void f3(void* arg1, void* arg2, void* arg3) {
	printf("3: %zu %zu %zu\n", (intptr_t)arg1, (intptr_t)arg2, (intptr_t)arg3);
	coroutine_yield();
}

void f4(void* arg1, void* arg2, void* arg3, void* arg4) {
	printf("4: %zu %zu %zu %zu\n", (intptr_t)arg1, (intptr_t)arg2, (intptr_t)arg3, (intptr_t)arg4);
	coroutine_yield();
}

int main() {
	coroutine_init();
	size_t argv1[] = {1};
	size_t argv2[] = {1, 2};
	size_t argv3[] = {1, 2, 3};
	size_t argv4[] = {1, 2, 3, 4};

	coroutine_goEX(&f0, 0, NULL);
	coroutine_goEX(&f1, 1, (void**)argv1);
	coroutine_goEX(&f2, 2, (void**)argv2);
	coroutine_goEX(&f3, 3, (void**)argv3);
	coroutine_goEX(&f4, 4, (void**)argv4);

	while (coroutine_alive() > 1)
		coroutine_yield();
	coroutine_destroy();

	puts("END");
	return 0;
}
