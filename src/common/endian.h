#ifndef COMMON_ENDIAN_H
#define COMMON_ENDIAN_H

#include <inttypes.h>

#if !defined(L_EDN) && !defined(B_EDN)
    #if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || \
    (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN) || \
    (defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN) || \
    (defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN)
        #define B_EDN
    #else
        #define L_EDN
    #endif
#endif

#define SWAPEDN16(x) ((uint16_t)((((x) << 8) & 0xFF00U) | (((x) >> 8) & 0xFFU)))
#define SWAPEDN32(x) ((uint32_t)((((x) << 24) & 0xFF000000LU) | (((x) << 8) & 0xFF0000LU) | (((x) >> 8) & 0xFF00LU) | (((x) >> 24) & 0xFFLU)))
#define SWAPEDN64(x) ((uint64_t)((((x) << 56) & 0xFF00000000000000LLU) | (((x) << 40) & 0xFF000000000000LLU) | (((x) << 24) & 0xFF0000000000LLU) | (((x) << 8) & 0xFF00000000LLU) |\
                     (((x) >> 8) & 0xFF000000LLU) | (((x) >> 24) & 0xFF0000LLU) | (((x) >> 40) & 0xFF00LLU) | (((x) >> 56) & 0xFFLLU)))

#ifdef L_EDN
    #define host2net16(x) ((uint16_t)(x))
    #define host2net32(x) ((uint32_t)(x))
    #define host2net64(x) ((uint64_t)(x))
    #define host2be16(x) (SWAPEDN16(x))
    #define host2be32(x) (SWAPEDN32(x))
    #define host2be64(x) (SWAPEDN64(x))
    #define host2le16(x) ((uint16_t)(x))
    #define host2le32(x) ((uint32_t)(x))
    #define host2le64(x) ((uint64_t)(x))
    #define net2host16(x) ((uint16_t)(x))
    #define net2host32(x) ((uint32_t)(x))
    #define net2host64(x) ((uint64_t)(x))
    #define be2host16(x) (SWAPEDN16(x))
    #define be2host32(x) (SWAPEDN32(x))
    #define be2host64(x) (SWAPEDN64(x))
    #define le2host16(x) ((uint16_t)(x))
    #define le2host32(x) ((uint32_t)(x))
    #define le2host64(x) ((uint64_t)(x))
#else
    #define host2net16(x) (SWAPEDN16(x))
    #define host2net32(x) (SWAPEDN32(x))
    #define host2net64(x) (SWAPEDN64(x))
    #define host2be16(x) ((uint16_t)(x))
    #define host2be32(x) ((uint32_t)(x))
    #define host2be64(x) ((uint64_t)(x))
    #define host2le16(x) (SWAPEDN16(x))
    #define host2le32(x) (SWAPEDN32(x))
    #define host2le64(x) (SWAPEDN64(x))
    #define net2host16(x) (SWAPEDN16(x))
    #define net2host32(x) (SWAPEDN32(x))
    #define net2host64(x) (SWAPEDN64(x))
    #define be2host16(x) ((uint16_t)(x))
    #define be2host32(x) ((uint32_t)(x))
    #define be2host64(x) ((uint64_t)(x))
    #define le2host16(x) (SWAPEDN16(x))
    #define le2host32(x) (SWAPEDN32(x))
    #define le2host64(x) (SWAPEDN64(x))
#endif

#endif
