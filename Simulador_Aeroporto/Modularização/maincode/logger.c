#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

static FILE* log_file = NULL;
static pthread_mutex_t log_mutex;

void log_init(const char* filename) {
    log_file = fopen(filename, "w");
    if (log_file == NULL) {
        perror("Falha ao abrir o arquivo de log");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(&log_mutex, NULL);
    
    fprintf(log_file, "--- Log da Simulação do Aeroporto ---\n");
    fflush(log_file);
}

void log_message(const char* format, ...) {
    pthread_mutex_lock(&log_mutex);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf) - 1, "%Y-%m-%d %H:%M:%S", t);
    va_list args_console;
    va_start(args_console, format);
    vprintf(format, args_console);
    va_end(args_console);
    fflush(stdout);
    if (log_file) {
        fprintf(log_file, "[%s] ", time_buf);
        
        va_list args_file;
        va_start(args_file, format);
        vfprintf(log_file, format, args_file);
        va_end(args_file);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

void log_close() {
    if (log_file) {
        fprintf(log_file, "\n--- Fim do Log ---\n");
        fclose(log_file);
    }
    pthread_mutex_destroy(&log_mutex);
}