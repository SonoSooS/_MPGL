#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef int8_t s8;
typedef uint8_t u8;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

#define COMPILER_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define COMPILER_UNLIKELY(expr) __builtin_expect(!!(expr), 0)

#define SH_INVALID(uniform) ((uniform)<0)
#define SH_VALID(uniform) (!SH_INVALID(uniform))

