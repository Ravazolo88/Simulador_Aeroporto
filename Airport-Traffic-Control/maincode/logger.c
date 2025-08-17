#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "logger.h"

static FILE* log_file = NULL;
static pthread_mutex_t log_mutex;

int logger_init(const char* filename) {
    log_file = fopen(filename, "w");
    if (log_file == NULL) {
        perror("Falha ao abrir o arquivo de log");
        return -1;
    }

    if (pthread_mutex_init(&log_mutex, NULL) != 0) {
        fclose(log_file);
        log_file = NULL;
        perror("Falha ao inicializar o mutex do log");
        return -1;
    }
    return 0;
}

void write_log(const char* message) {
    if (log_file == NULL) {
        return;
    }
    
    pthread_mutex_lock(&log_mutex);
    
    fprintf(log_file, "%s", message);
    
    fflush(log_file);
    
    pthread_mutex_unlock(&log_mutex);
}

void logger_close(void) {
    if (log_file != NULL) {
        char final_message[] = "--- Fim do Log ---\n";
        write_log(final_message);
        
        fclose(log_file);
        pthread_mutex_destroy(&log_mutex);
        log_file = NULL;
    }
}