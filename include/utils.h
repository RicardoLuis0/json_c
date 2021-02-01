#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#define NORETURN [[noreturn]]
extern "C" {
#else
#include <stddef.h>
#include <stdint.h>
#define NORETURN __attribute__((__noreturn__))
#endif // __cplusplus


size_t str_hash(const char * s);

NORETURN void err_exit(const char * fmt,...);

#ifdef __cplusplus
}
#endif // __cplusplus
