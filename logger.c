#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "task-sw.h"

//Global variables
FILE *log_fp = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;;

void log_init(char *log_file_name) {
    if ((log_fp = fopen(log_file_name, "a")) == NULL)
        handle_error("fopen");
    fputc('\n', log_fp);
}

void log_close() {
    if (log_fp != NULL)
        fclose(log_fp);
}

void logger(const char *format, ...) {
    time_t curr_time = time(NULL);
    struct tm tm_time;
    char time_string[80];
    va_list pa;

    memset(&tm_time, '\0', sizeof(tm_time));
    localtime_r(&curr_time, &tm_time);
    sprintf(time_string, "%02d.%02d.%02d %02d.%02d.%02d ", tm_time.tm_mday, tm_time.tm_mon + 1, tm_time.tm_year + 1900, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);

    pthread_mutex_lock(&mutex);

    fprintf(log_fp, "%s", time_string);
    va_start(pa, format);
    if (vfprintf(log_fp, format, pa) < 0)
        handle_error("vfprintf");
    if (fputc('\n', log_fp) == EOF)
        handle_error("fputc");
    va_end(pa);
    fflush(log_fp);

    pthread_mutex_unlock(&mutex);
}
