// '16/04/28 pxtn.h
// '16/12/03 pxtnRESULT.
// '16/12/15 pxtnRESULT -> pxtnERR/pxtnOK.

#ifndef pxtn_H
#define pxtn_H

//#define pxINCLUDE_OGGVORBIS 1
// $(SolutionDir)libogg\include;$(SolutionDir)libvorbis\include;

#include <stdint.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp32-hal-psram.h>

static inline void* px_platform_malloc(size_t size)
{
    if(psramFound())
    {
        void* p = ps_malloc(size);
        if(p) return p;
    }
    return malloc(size);
}

static inline void* px_platform_calloc(size_t count, size_t size)
{
    if(psramFound())
    {
        void* p = ps_calloc(count, size);
        if(p) return p;
    }
    return calloc(count, size);
}

static inline void* px_platform_realloc(void* ptr, size_t size)
{
    if(psramFound())
    {
        void* p = ps_realloc(ptr, size);
        if(p || size == 0) return p;
    }
    return realloc(ptr, size);
}

#define malloc(size) px_platform_malloc(size)
#define calloc(count, size) px_platform_calloc(count, size)
#define realloc(ptr, size) px_platform_realloc(ptr, size)
#endif

#ifdef pxUseSDL
#include "SDL_endian.h"

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#   define px_BIG_ENDIAN
#endif

#   define px_FORCE_INLINE  SDL_FORCE_INLINE
#else
#if defined(_MSC_VER)
    #define px_FORCE_INLINE __forceinline
#elif (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2))) || defined(__clang__)
    #define px_FORCE_INLINE static __inline __attribute__((always_inline))
#else
    #define px_FORCE_INLINE static inline
#endif
#endif // pxUseSDL

#ifdef px_BIG_ENDIAN
#   define px_IS_BIG_ENDIAN     1
#else
#   define px_IS_BIG_ENDIAN     0
#endif

typedef struct
{
    int32_t x;
    int32_t y;
}
pxtnPOINT;

#include "./pxtnError.h"

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if( p ){ delete( p ); p = NULL; } }
#endif

#endif
