/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#include <PrintString.h>
#undef __LDBG_assertf
#if _MSC_VER
#define __LDBG_assertf(cond, fmt, ...)		_ASSERT_EXPR((cond), PrintString(F(fmt), ##__VA_ARGS__).LPWStr());
#else
#define __LDBG_assertf(cond, fmt, ...)		if (!(cond)) { __DBG_printf(fmt, ##__VA_ARGS__); assert(_STRINGIFY(cond) == nullptr); }
#endif
#elif HAVE_KFC_FIRMWARE_VERSION
#include <debug_helper_disable.h>
#else
#undef __LDBG_printf
#undef __LDBG_print
#undef __LDBG_println
#undef __LDBG_assert
#undef __LDBG_assertf
#define __LDBG_printf(...)
#define __LDBG_print(...)
#define __LDBG_println(...)
#define __LDBG_assert(...)
#define __LDBG_assertf(...)
#endif
