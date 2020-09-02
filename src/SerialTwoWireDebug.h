/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#undef __LDBG_assert
#define __LDBG_assert(condition, fmt, ...) if (!(condition)) { __DBG_printf(fmt, ##__VA_ARGS__); assert(condition); }
#else
#undef __LDBG_printf
#undef __LDBG_print
#undef __LDBG_println
#undef __LDBG_assert
#define __LDBG_printf(...)
#define __LDBG_print(...)
#define __LDBG_println(...)
#define __LDBG_assert(...)
#endif
