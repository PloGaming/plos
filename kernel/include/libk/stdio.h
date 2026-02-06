#ifndef LIBK_STDIO_H
#define LIBK_STDIO_H

#include <stdarg.h>

int printf(char *fmt, ...);
int vprintf(char *fmt, va_list ap);
int sprintf(char *buf, char *fmt, ...);

#endif // LIBK_STDIO_H