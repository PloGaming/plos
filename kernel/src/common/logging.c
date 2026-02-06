#include <common/logging.h>
#include <stdarg.h>
#include <libk/stdio.h>

char *logStrings[] = {
    "DEBUG",
    "WARN",
    "ERROR",
    "SUCCESS"      
};

static enum logOutput outputType;

void log_init(enum logOutput out)
{
    if(out == LOG_SERIAL)
    {
        printf("\n");
    }
    outputType = out;
}

void log_logLine(enum logType logLevel, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    switch(outputType)
    {
        case LOG_SERIAL:
            printf("[%s] ", logStrings[logLevel]);
            vprintf(fmt, args);
            printf("\n");
        break;
        
        // TODO: Implement a framebuffer "text mode"
        case LOG_FRAMEBUFFER:
        break;
    }

    va_end(args);
}