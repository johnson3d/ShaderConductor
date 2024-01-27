/*
 * ShaderConductor
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cstdint>

#if defined(__clang__)
#define SC_BUILTIN_UNREACHABLE __builtin_unreachable()
#elif defined(__GNUC__)
#define SC_BUILTIN_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define SC_BUILTIN_UNREACHABLE __assume(false)
#endif

#if !defined(NDEBUG) || !defined(SC_BUILTIN_UNREACHABLE)
[[noreturn]] void UnreachableInternal(const char* msg, const char* file, uint32_t line);

#define SC_UNREACHABLE(msg) UnreachableInternal(msg, __FILE__, __LINE__)
#else
#define SC_UNREACHABLE(msg) SC_BUILTIN_UNREACHABLE
#endif

[[noreturn]] void ThrowFileLine(const char* file, uint32_t line);

// Throw if failed (HRESULT)
#define TIFHR(x)                                                                                                                           \
    {                                                                                                                                      \
        const HRESULT hr = (x);                                                                                                            \
        if (hr < 0)                                                                                                                        \
        {                                                                                                                                  \
            ThrowFileLine(__FILE__, __LINE__);                                                                                             \
        }                                                                                                                                  \
    }

// Throw if false
#define TIFFALSE(p)                                                                                                                        \
    {                                                                                                                                      \
        if (!(p))                                                                                                                          \
        {                                                                                                                                  \
            ThrowFileLine(__FILE__, __LINE__);                                                                                             \
        }                                                                                                                                  \
    }
