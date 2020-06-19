#ifndef TURTLE_API_TIFF_H
#define TURTLE_API_TIFF_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#define FIELD_CUSTOM 65

#define TIFFTAG_IMAGEWIDTH              256     /* image width in pixels */
#define TIFFTAG_IMAGELENGTH             257     /* image height in pixels */
#define TIFFTAG_BITSPERSAMPLE           258     /* bits per channel (sample) */
#define TIFFTAG_COMPRESSION             259     /* data compression technique */
#define     COMPRESSION_NONE            1       /* dump mode */
#define TIFFTAG_PHOTOMETRIC             262     /* photometric interpretation */
#define     PHOTOMETRIC_MINISBLACK      1       /* min value is black */
#define TIFFTAG_ORIENTATION             274     /* +image orientation */
#define     ORIENTATION_TOPLEFT         1       /* row 0 top, col 0 lhs */
#define TIFFTAG_SAMPLESPERPIXEL         277     /* samples per pixel */
#define TIFFTAG_ROWSPERSTRIP            278     /* rows per strip of data */
#define TIFFTAG_PLANARCONFIG            284     /* storage organization */
#define     PLANARCONFIG_CONTIG         1       /* single image plane */
#define TIFFTAG_RESOLUTIONUNIT          296     /* units of resolutions */
#define     RESUNIT_NONE                1       /* no meaningful units */

typedef struct tiff TIFF;
typedef void (*TIFFErrorHandler) (const char *, const char *, va_list);
typedef void (*TIFFExtendProc) (TIFF *);

#define TIFF_INT16_T signed short
#define TIFF_INT32_T signed int
#define TIFF_INT64_T signed long
#define TIFF_INT8_T signed char
#define TIFF_UINT16_T unsigned short
#define TIFF_UINT32_T unsigned int
#define TIFF_UINT64_T unsigned long
#define TIFF_UINT8_T unsigned char
#define TIFF_SSIZE_T signed long
#define TIFF_PTRDIFF_T ptrdiff_t

typedef TIFF_INT8_T   int8;
typedef TIFF_UINT8_T  uint8;
typedef TIFF_INT16_T  int16;
typedef TIFF_UINT16_T uint16;
typedef TIFF_INT32_T  int32;
typedef TIFF_UINT32_T uint32;
typedef TIFF_INT64_T  int64;
typedef TIFF_UINT64_T uint64;

typedef TIFF_SSIZE_T tmsize_t;
typedef uint64 toff_t;          /* file offset */

typedef uint32 ttag_t;          /* directory tag */
typedef uint16 tdir_t;          /* directory index */
typedef uint16 tsample_t;       /* sample number */
typedef uint32 tstrile_t;       /* strip or tile number */
typedef tstrile_t tstrip_t;     /* strip number */
typedef tstrile_t ttile_t;      /* tile number */
typedef tmsize_t tsize_t;       /* i/o size in bytes */
typedef void * tdata_t;         /* image data ref */

typedef enum {
        TIFF_NOTYPE = 0,      /* placeholder */
        TIFF_BYTE = 1,        /* 8-bit unsigned integer */
        TIFF_ASCII = 2,       /* 8-bit bytes w/ last byte null */
        TIFF_SHORT = 3,       /* 16-bit unsigned integer */
        TIFF_LONG = 4,        /* 32-bit unsigned integer */
        TIFF_RATIONAL = 5,    /* 64-bit unsigned fraction */
        TIFF_SBYTE = 6,       /* !8-bit signed integer */
        TIFF_UNDEFINED = 7,   /* !8-bit untyped data */
        TIFF_SSHORT = 8,      /* !16-bit signed integer */
        TIFF_SLONG = 9,       /* !32-bit signed integer */
        TIFF_SRATIONAL = 10,  /* !64-bit signed fraction */
        TIFF_FLOAT = 11,      /* !32-bit IEEE floating point */
        TIFF_DOUBLE = 12,     /* !64-bit IEEE floating point */
        TIFF_IFD = 13,        /* %32-bit unsigned integer (offset) */
        TIFF_LONG8 = 16,      /* BigTIFF 64-bit unsigned integer */
        TIFF_SLONG8 = 17,     /* BigTIFF 64-bit signed integer */
        TIFF_IFD8 = 18        /* BigTIFF 64-bit unsigned integer (offset) */
} TIFFDataType;

typedef struct {
        ttag_t  field_tag;              /* field's tag */
        short   field_readcount;        /* read count/TIFF_VARIABLE/TIFF_SPP */
        short   field_writecount;       /* write count/TIFF_VARIABLE */
        TIFFDataType field_type;        /* type of associated data */
        unsigned short field_bit;       /* bit in fieldsset bit vector */
        unsigned char field_oktochange; /* if true, can change while writing */
        unsigned char field_passcount;  /* if true, pass dir count on set */
        char    *field_name;            /* ASCII name */
} TIFFFieldInfo;

#ifdef __cplusplus
}
#endif
#endif
