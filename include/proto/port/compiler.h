/* Copyright (c) 2022-2026, HelloAlpha */
#ifndef PROTO_PORT_COMPILER_H
#define PROTO_PORT_COMPILER_H

#ifndef PROTO_API
#  if defined(__GNUC__) || defined(__clang__)
#    define PROTO_API __attribute__((visibility("default")))
#  else
#    define PROTO_API
#  endif
#endif

#ifndef PROTO_INTERNAL
#  if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#    define PROTO_INTERNAL __attribute__((visibility("hidden")))
#  else
#    define PROTO_INTERNAL
#  endif
#endif

#if defined(__cplusplus)
#  define PROTO_EXTERN_C_BEGIN extern "C" {
#  define PROTO_EXTERN_C_END   }
#else
#  define PROTO_EXTERN_C_BEGIN
#  define PROTO_EXTERN_C_END
#endif

#if defined(__cplusplus)
#  if defined(_MSC_VER) || (__cplusplus >= 201103L)
#    define PROTO_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#  else
#    error "C++11 or later is required for the public header"
#  endif
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define PROTO_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#else
#  error "C11 or later is required to build the library"
#endif

#endif
