#ifndef TURTLE_API_PNG_H
#define TURTLE_API_PNG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>

#define PNG_COLOR_TYPE_GRAY 0
#define PNG_COMPRESSION_TYPE_BASE 0
#define PNG_FILTER_TYPE_BASE 0
#define PNG_INTERLACE_NONE 0
#define PNG_TEXT_COMPRESSION_NONE -1

#if CHAR_BIT == 8 && UCHAR_MAX == 255
   typedef unsigned char png_byte;
#else
#  error "libpng requires 8-bit bytes"
#endif

#if INT_MIN == -32768 && INT_MAX == 32767
   typedef int png_int_16;
#elif SHRT_MIN == -32768 && SHRT_MAX == 32767
   typedef short png_int_16;
#else
#  error "libpng requires a signed 16-bit type"
#endif

#if UINT_MAX == 65535
   typedef unsigned int png_uint_16;
#elif USHRT_MAX == 65535
   typedef unsigned short png_uint_16;
#else
#  error "libpng requires an unsigned 16-bit type"
#endif

#if INT_MIN < -2147483646 && INT_MAX > 2147483646
   typedef int png_int_32;
#elif LONG_MIN < -2147483646 && LONG_MAX > 2147483646
   typedef long int png_int_32;
#else
#  error "libpng requires a signed 32-bit (or more) type"
#endif

#if UINT_MAX > 4294967294U
   typedef unsigned int png_uint_32;
#elif ULONG_MAX > 4294967294U
   typedef unsigned long int png_uint_32;
#else
#  error "libpng requires an unsigned 32-bit (or more) type"
#endif

typedef size_t png_size_t;

typedef void                  * png_voidp;
typedef const void            * png_const_voidp;
typedef png_byte              * png_bytep;
typedef const png_byte        * png_const_bytep;
typedef png_uint_32           * png_uint_32p;
typedef const png_uint_32     * png_const_uint_32p;
typedef png_int_32            * png_int_32p;
typedef const png_int_32      * png_const_int_32p;
typedef png_uint_16           * png_uint_16p;
typedef const png_uint_16     * png_const_uint_16p;
typedef png_int_16            * png_int_16p;
typedef const png_int_16      * png_const_int_16p;
typedef char                  * png_charp;
typedef const char            * png_const_charp;
typedef png_size_t            * png_size_tp;
typedef const png_size_t      * png_const_size_tp;

typedef png_byte        * * png_bytepp;
typedef png_uint_32     * * png_uint_32pp;
typedef png_int_32      * * png_int_32pp;
typedef png_uint_16     * * png_uint_16pp;
typedef png_int_16      * * png_int_16pp;
typedef const char      * * png_const_charpp;
typedef char            * * png_charpp;

typedef char            * * * png_charppp;

typedef FILE * png_FILE_p;

typedef struct png_struct_def png_struct;
typedef const png_struct * png_const_structp;
typedef png_struct * png_structp;
typedef png_struct * * png_structpp;

typedef struct png_info_def png_info;
typedef png_info * png_infop;
typedef const png_info * png_const_infop;
typedef png_info * * png_infopp;

typedef struct png_text_struct
{
   int  compression;       /* compression value:
                             -1: tEXt, none
                              0: zTXt, deflate
                              1: iTXt, none
                              2: iTXt, deflate  */
   png_charp key;          /* keyword, 1-79 character description of "text" */
   png_charp text;         /* comment, may be an empty string (ie "")
                              or a NULL pointer */
   png_size_t text_length; /* length of the text string */
   png_size_t itxt_length; /* length of the itxt string */
   png_charp lang;         /* language code, 0-79 characters
                              or a NULL pointer */
   png_charp lang_key;     /* keyword translated UTF-8 string, 0 or more
                              chars or a NULL pointer */
} png_text;
typedef png_text * png_textp;
typedef const png_text * png_const_textp;
typedef png_text * * png_textpp;

typedef void (*png_longjmp_ptr) (jmp_buf, int);
typedef void (*png_error_ptr) (png_structp, png_const_charp);

#ifdef __cplusplus
}
#endif
#endif
