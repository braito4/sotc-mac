/* Cooperative cancellation for our diagnostic executables. No guest API,
 * external process signals, GPU teardown or game-memory access is involved. */
#ifndef SOTC_DIAGNOSTIC_CONTROL_H
#define SOTC_DIAGNOSTIC_CONTROL_H
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void *sotc_diagnostic_control_wait(void *argument) {
    const int fd = (int)(intptr_t)argument;
    char request;
    ssize_t count;
    do { count = read(fd, &request, 1); } while (count < 0 && errno == EINTR);
    close(fd);
    static const char requested[] = "STOP_DIAGNOSTIC_CONTROL: parent requested termination\n";
    static const char closed[] = "STOP_DIAGNOSTIC_CONTROL: parent channel closed\n";
    static const char failed[] = "STOP_DIAGNOSTIC_CONTROL: channel read failed\n";
    const char *message = count > 0 ? requested : count == 0 ? closed : failed;
    const size_t size = count > 0 ? sizeof(requested) : count == 0 ? sizeof(closed) : sizeof(failed);
    /* A busy renderer can fill the inherited output pipe while its supervisor
     * is stopping it. The stop notice is best effort: it must never delay exit.
     * This flag change is confined to cancellation, immediately before _Exit. */
    const int output_flags = fcntl(STDERR_FILENO, F_GETFL);
    struct stat output_info;
    sigset_t blocked;
    sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
    /* SIGPIPE is blocked only in this terminating worker. A closed output
     * reader must not replace the intended exit status with signal 13. */
    if (!pthread_sigmask(SIG_BLOCK,&blocked,NULL) && output_flags>=0 &&
        !fstat(STDERR_FILENO,&output_info) &&
        (S_ISFIFO(output_info.st_mode) || S_ISSOCK(output_info.st_mode) || isatty(STDERR_FILENO)) &&
        ((output_flags&O_NONBLOCK) || !fcntl(STDERR_FILENO,F_SETFL,output_flags|O_NONBLOCK))) {
        if (write(STDERR_FILENO,message,size-1)==(ssize_t)(size-1)) {
#ifdef SOTC_DIAGNOSTIC_CONTROL_DUMP
            /* Optional bounded atomic snapshot only; caller must not wait,
             * allocate, take locks, read guest memory or access the driver. */
            SOTC_DIAGNOSTIC_CONTROL_DUMP(STDERR_FILENO);
#endif
        }
    }
    _Exit(count < 0 ? 28 : 21);
}

static int sotc_diagnostic_control_start(void) {
    const char *value = getenv("SOTC_DIAGNOSTIC_CONTROL_FD");
    if (!value) return 0;
    unsigned long number = 0;
    if (!*value) goto invalid;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (*p < '0' || *p > '9' || number > (INT_MAX - (*p - '0')) / 10UL)
            goto invalid;
        number = number * 10 + (*p - '0');
    }
    if (number < 3) goto invalid;
    const int fd = (int)number;
    struct stat info;
    const int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
        fstat(fd, &info) || !S_ISFIFO(info.st_mode) || fcntl(fd, F_SETFD, FD_CLOEXEC))
        goto invalid;
#ifdef F_SETNOSIGPIPE
    /* Closed-output stress reproduced SIGPIPE despite the worker mask. The
     * Darwin descriptor policy also protects writes from other live threads.
     * Set it before the ready notice/worker, without changing the process-wide
     * signal disposition. Output remains optional on failure. */
    (void)fcntl(STDERR_FILENO,F_SETNOSIGPIPE,1);
#endif
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes)) goto invalid;
    int error = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    if (!error) error = pthread_create(&thread, &attributes, sotc_diagnostic_control_wait,
                                       (void *)(intptr_t)fd);
    pthread_attr_destroy(&attributes);
    if (error) goto invalid;
    fputs("Diagnostic control: parent cancellation channel ready\n", stderr);
    return 0;
invalid:
    fputs("STOP_DIAGNOSTIC_CONTROL: invalid cancellation pipe or worker unavailable\n", stderr);
    return -1;
}
#endif
