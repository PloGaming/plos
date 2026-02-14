#ifndef LOGGING_H
#define LOGGING_H

/**
 * @brief The type of the log
 */
enum logType {
    LOG_DEBUG,
    LOG_WARN,
    LOG_ERROR,
    LOG_SUCCESS
};

/**
 * @brief The type of the output log
 */
enum logOutput {
    LOG_SERIAL, ///< Log onto a file that qemu redirects output to
    LOG_FRAMEBUFFER, ///< Onto the screen usign the framebuffer
    LOG_BOTH ///< Use both
};

void log_init(enum logOutput out);
void log_log_line(enum logType logLevel, char *fmt, ...);

#endif // LOGGING_H