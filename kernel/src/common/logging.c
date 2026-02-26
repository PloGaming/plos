#include <scheduling/lock.h>
#include <common/logging.h>
#include <drivers/serial.h>
#include <drivers/console.h>
#include <stdarg.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <stdbool.h>
#include <stddef.h>

static char log_buffer[LOG_BUFFER_DIM];

char *log_labels[] = {
    "DBG",
    "OK" ,
    "WARN",
    "ERR",
};

char *log_colors[] = {
    "\033[0m", // DEBUG
    "\033[32m", // OK
    "\033[33m", // WARN
    "\033[31m"  // ERROR
};

void log_line(enum logType logLevel, char *fmt, ...)
{
    bool send_to_console = (logLevel != LOG_DEBUG);
    char *ptr = log_buffer;

    ptr[0] = '[';
    ptr++;

    strcpy(ptr, log_colors[logLevel]);
    ptr += strlen(log_colors[logLevel]);
    
    strcpy(ptr, log_labels[logLevel]);
    ptr += strlen(log_labels[logLevel]);

    strcpy(ptr, "\033[0m");
    ptr += 4;

    strcpy(ptr, "]\t");
    ptr += 2;

    va_list args;
    va_start(args, fmt);
    
    size_t remaining = sizeof(log_buffer) - (ptr - log_buffer) - 2; // -2 for newline and NULL
    int written = vsnprintf(ptr, remaining, fmt, args);
    ptr += written;
    va_end(args);

    *ptr++ = '\r';
    *ptr++ = '\n';
    *ptr = '\0';

    serial_write_str(log_buffer, ptr - log_buffer);

    if (send_to_console) {
        console_write_str(log_buffer, ptr - log_buffer);
    }
}