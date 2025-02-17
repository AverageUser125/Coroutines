#ifndef COROUTINE_H_
#define COROUTINE_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void coroutine_init(void);
void coroutine_destroy();
void coroutine_yield(void);
void coroutine_go(void (*f)(void*), void* arg);
void coroutine_goEX(void (*f)(), int argc, void** argv);
size_t coroutine_id(void);
size_t coroutine_alive(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // COROUTINE_H_
