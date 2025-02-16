#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "V1/coroutine.h"
#include "V1/coroutine.c"

void counter(void* arg) {
	long int n = (long int)(size_t)arg;
	for (int i = 1; i <= n; ++i) {
		size_t id = coroutine_id();
		printf("[%zu] %d\n", id, i);
		coroutine_yield();
	}
}

int main() {
	coroutine_init();
	coroutine_go(&counter, (void*)5);
	coroutine_go(&counter, (void*)10);
	while (coroutine_alive() > 1)
		coroutine_yield();
	coroutine_finish();
	return 0;
}
