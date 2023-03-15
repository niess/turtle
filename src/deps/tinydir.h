/*
Copyright (c) 2013-2021, tinydir authors:
- Cong Xu
- Lautis Sun
- Baudouin Feildel
- Andargor <andargor@yahoo.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef TINYDIR_H
#define TINYDIR_H

#ifdef __cplusplus
extern "C" {
#endif

#if ((defined _UNICODE) && !(defined UNICODE))
#define UNICODE
#endif

#if ((defined UNICODE) && !(defined _UNICODE))
#define _UNICODE
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <tchar.h>
# pragma warning(push)
# pragma warning (disable : 4996)
#else
# include <dirent.h>
# include <libgen.h>
# include <sys/stat.h>
# include <stddef.h>
#endif
#ifdef __MINGW32__
# include <tchar.h>
#endif


/* types */

/* Windows UNICODE wide character support */
#if defined _MSC_VER || defined __MINGW32__
# define _tinydir_char_t TCHAR
# define TINYDIR_STRING(s) _TEXT(s)
# define _tinydir_strlen _tcslen
# define _tinydir_strcpy _tcscpy
# define _tinydir_strcat _tcscat
# define _tinydir_strcmp _tcscmp
# define _tinydir_strrchr _tcsrchr
# define _tinydir_strncmp _tcsncmp
#else
# define _tinydir_char_t char
# define TINYDIR_STRING(s) s
# define _tinydir_strlen strlen
# define _tinydir_strcpy strcpy
# define _tinydir_strcat strcat
# define _tinydir_strcmp strcmp
# define _tinydir_strrchr strrchr
# define _tinydir_strncmp strncmp
#endif

#if (defined _MSC_VER || defined __MINGW32__)
# include <windows.h>
# define _TINYDIR_PATH_MAX MAX_PATH
#elif defined  __linux__
# include <limits.h>
# ifdef PATH_MAX
#  define _TINYDIR_PATH_MAX PATH_MAX
# endif
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
# include <sys/param.h>
# if defined(BSD)
#  include <limits.h>
#  ifdef PATH_MAX
#   define _TINYDIR_PATH_MAX PATH_MAX
#  endif
# endif
#endif

#ifndef _TINYDIR_PATH_MAX
#define _TINYDIR_PATH_MAX 4096
#endif

#ifdef _MSC_VER
/* extra chars for the "\\*" mask */
# define _TINYDIR_PATH_EXTRA 2
#else
# define _TINYDIR_PATH_EXTRA 0
#endif

#define _TINYDIR_FILENAME_MAX 256

#if (defined _MSC_VER || defined __MINGW32__)
#define _TINYDIR_DRIVE_MAX 3
#endif

/* readdir_r usage; define TINYDIR_USE_READDIR_R to use it (if supported) */
#ifdef TINYDIR_USE_READDIR_R

/* readdir_r is a POSIX-only function, and may not be available under various
 * environments/settings, e.g. MinGW. Use readdir fallback */
#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE ||\
	_POSIX_SOURCE
# define _TINYDIR_HAS_READDIR_R
#endif
#if _POSIX_C_SOURCE >= 200112L
# define _TINYDIR_HAS_FPATHCONF
# include <unistd.h>
#endif
#if _BSD_SOURCE || _SVID_SOURCE || \
	(_POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)
# define _TINYDIR_HAS_DIRFD
# include <sys/types.h>
#endif
#if defined _TINYDIR_HAS_FPATHCONF && defined _TINYDIR_HAS_DIRFD &&\
	defined _PC_NAME_MAX
# define _TINYDIR_USE_FPATHCONF
#endif
#if defined __MINGW32__ || !defined _TINYDIR_HAS_READDIR_R ||\
	!(defined _TINYDIR_USE_FPATHCONF || defined NAME_MAX)
# define _TINYDIR_USE_READDIR
#endif

/* Use readdir by default */
#else
# define _TINYDIR_USE_READDIR
#endif

/* MINGW32 has two versions of dirent, ASCII and UNICODE*/
#ifndef _MSC_VER
#if (defined __MINGW32__) && (defined _UNICODE)
#define _TINYDIR_DIR _WDIR
#define _tinydir_dirent _wdirent
#define _tinydir_opendir _wopendir
#define _tinydir_readdir _wreaddir
#define _tinydir_closedir _wclosedir
#else
#define _TINYDIR_DIR DIR
#define _tinydir_dirent dirent
#define _tinydir_opendir opendir
#define _tinydir_readdir readdir
#define _tinydir_closedir closedir
#endif
#endif

/* Allow user to use a custom allocator by defining _TINYDIR_MALLOC and _TINYDIR_FREE. */
#if    defined(_TINYDIR_MALLOC) &&  defined(_TINYDIR_FREE)
#elif !defined(_TINYDIR_MALLOC) && !defined(_TINYDIR_FREE)
#else
#error "Either define both alloc and free or none of them!"
#endif

#if !defined(_TINYDIR_MALLOC)
	#define _TINYDIR_MALLOC(_size) malloc(_size)
	#define _TINYDIR_FREE(_ptr)    free(_ptr)
#endif /* !defined(_TINYDIR_MALLOC) */

typedef struct tinydir_file
{
	_tinydir_char_t path[_TINYDIR_PATH_MAX];
	_tinydir_char_t name[_TINYDIR_FILENAME_MAX];
	_tinydir_char_t *extension;
	int is_dir;
	int is_reg;

#ifndef _MSC_VER
#ifdef __MINGW32__
	struct _stat _s;
#else
	struct stat _s;
#endif
#endif
} tinydir_file;

typedef struct tinydir_dir
{
	_tinydir_char_t path[_TINYDIR_PATH_MAX];
	int has_next;
	size_t n_files;

	tinydir_file *_files;
#ifdef _MSC_VER
	HANDLE _h;
	WIN32_FIND_DATA _f;
#else
	_TINYDIR_DIR *_d;
	struct _tinydir_dirent *_e;
#ifndef _TINYDIR_USE_READDIR
	struct _tinydir_dirent *_ep;
#endif
#endif
} tinydir_dir;


/* declarations */

int tinydir_open(tinydir_dir *dir, const _tinydir_char_t *path);
int tinydir_open_sorted(tinydir_dir *dir, const _tinydir_char_t *path);
void tinydir_close(tinydir_dir *dir);

int tinydir_next(tinydir_dir *dir);
int tinydir_readfile(const tinydir_dir *dir, tinydir_file *file);
int tinydir_readfile_n(const tinydir_dir *dir, tinydir_file *file, size_t i);
int tinydir_open_subdir_n(tinydir_dir *dir, size_t i);

int tinydir_file_open(tinydir_file *file, const _tinydir_char_t *path);

#ifdef __cplusplus
}
#endif

# if defined (_MSC_VER)
# pragma warning(pop)
# endif

#endif
