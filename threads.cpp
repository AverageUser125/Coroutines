#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "V2/coroutine.h"
#include <thread>

void counter(void* arg) {
	long int n = (long int)(size_t)arg;
	for (int i = 1; i <= n; ++i) {
		size_t id = coroutine_id();
		printf("[%zu] %d\n", id, i);
		coroutine_yield();
	}
}

void runner(size_t value) {
	coroutine_init();
	coroutine_go(&counter, (void*)value);
	coroutine_go(&counter, (void*)(value * 2));
	while (coroutine_alive() > 1)
		coroutine_yield();
}

int main() {
	std::thread t1(runner, 5);
	std::thread t2(runner, 3);

	t1.join();
	t2.join();
	return 0;
}
