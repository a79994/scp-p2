#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

ssize_t send_msg(int fd, const broker_msg_t *msg) {
    size_t total = 0;
    const char *buf = (const char *)msg;
    size_t len = sizeof(broker_msg_t);

    while (total < len) {
        ssize_t n = write(fd, buf + total, len - total);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            return -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t recv_msg(int fd, broker_msg_t *msg) {
    size_t total = 0;
    char *buf = (char *)msg;
    size_t len = sizeof(broker_msg_t);

    while (total < len) {
        ssize_t n = read(fd, buf + total, len - total);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            return (n == 0 && total == 0) ? 0 : -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

void get_formatted_time(char *buffer, size_t buf_size) {
    struct timespec ts;
    struct tm tm_info;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_info);

    long centiseconds = ts.tv_nsec / 10000000L;

    snprintf(buffer, buf_size, "%02d:%02d:%02d.%02ld",
             tm_info.tm_hour,
             tm_info.tm_min,
             tm_info.tm_sec,
             centiseconds);
}
