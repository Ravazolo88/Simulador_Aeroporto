#include <stdio.h>
#include <stdlib.h> // Necessário para pthread_exit
#include <pthread.h>
#include <unistd.h>

#include "airport.h"
#include "logger.h"
#include "config.h" 

// --- Variáveis Globais do Módulo ---

// Ponteiros para as filas de prioridade, recebidos de main.c
static PriorityQueue* g_landing_queue; 
static PriorityQueue* g_disembarking_queue;
static PriorityQueue* g_takeoff_queue;

// Mutex e variável de condição com 'linkage externa' para serem visíveis ao utils.c
pthread_mutex_t airport_mutex;
pthread_cond_t resource_cond;

// Contadores de recursos disponíveis (privados para este arquivo)
static int available_runways;
static int available_gates;
static int tower_capacity_in_use;


// --- Funções do Módulo ---

void airport_init(PriorityQueue* landing_q, PriorityQueue* disembarking_q, PriorityQueue* takeoff_q) {
    g_landing_queue = landing_q;
    g_disembarking_queue = disembarking_q;
    g_takeoff_queue = takeoff_q;

    pthread_mutex_init(&airport_mutex, NULL);
    pthread_cond_init(&resource_cond, NULL);

    available_runways = NUM_RUNWAYS;
    available_gates = NUM_GATES;
    tower_capacity_in_use = 0;

    write_log("INFO: Modulo Aeroporto inicializado.\n");
}

void airport_destroy(void) {
    pthread_mutex_destroy(&airport_mutex);
    pthread_cond_destroy(&resource_cond); 
    write_log("INFO: Modulo Aeroporto finalizado.\n");
}

// Lógica de solicitação para POUSO (Requer: 1 Pista, 1 Torre)
void airport_request_landing_resources(PlaneData* data) {
    pthread_mutex_lock(&airport_mutex);

    data->init_time_wait = time(NULL);
    priority_queue_insert(g_landing_queue, data);

    // ... (snprintf para log da fila) ...

    while (available_runways < 1 || (tower_capacity_in_use >= TOWER_CAPACITY) ||
           (g_landing_queue->size == 0 || g_landing_queue->planes[0]->id != data->id))
    {
        pthread_cond_wait(&resource_cond, &airport_mutex);

        if (data->status == CRASHED) {
            // ... (código para sair da thread) ...
        }
    }

    // --- INÍCIO DA ADIÇÃO: Coleta de estatísticas de tempo de espera ---
    double wait_time = difftime(time(NULL), data->init_time_wait);
    if (wait_time > 0.1) { // Só registra se esperou um tempo mínimo
        stats_add_wait_time(wait_time);
    }
    // --- FIM DA ADIÇÃO ---

    priority_queue_remove(g_landing_queue);

    // ... (resto da função sem alteração) ...
    pthread_mutex_unlock(&airport_mutex);
}

void airport_release_landing_resources() {
    pthread_mutex_lock(&airport_mutex);
    available_runways++;
    tower_capacity_in_use--;
    pthread_cond_broadcast(&resource_cond);
    pthread_mutex_unlock(&airport_mutex);
}


// Lógica de solicitação para DESEMBARQUE (Requer: 1 Portão, 1 Torre)
void airport_request_disembarking_resources(PlaneData* data) {
    pthread_mutex_lock(&airport_mutex);

    data->init_time_wait = time(NULL);
    priority_queue_insert(g_disembarking_queue, data);

    // ... (snprintf para log da fila) ...

    while (available_gates < 1 || (tower_capacity_in_use >= TOWER_CAPACITY) ||
           (g_disembarking_queue->size == 0 || g_disembarking_queue->planes[0]->id != data->id))
    {
        pthread_cond_wait(&resource_cond, &airport_mutex);

        if (data->status == CRASHED) {
            // ... (código para sair da thread) ...
        }
    }

    // --- INÍCIO DA ADIÇÃO: Coleta de estatísticas de tempo de espera ---
    double wait_time = difftime(time(NULL), data->init_time_wait);
    if (wait_time > 0.1) {
        stats_add_wait_time(wait_time);
    }
    // --- FIM DA ADIÇÃO ---

    priority_queue_remove(g_disembarking_queue);
    
    // ... (resto da função sem alteração) ...
    pthread_mutex_unlock(&airport_mutex);
}

void airport_release_disembarking_resources() {
    pthread_mutex_lock(&airport_mutex);
    available_gates++;
    tower_capacity_in_use--;
    pthread_cond_broadcast(&resource_cond);
    pthread_mutex_unlock(&airport_mutex);
}

// Lógica de solicitação para DECOLAGEM (Requer: 1 Pista, 1 Portão, 1 Torre)
void airport_request_takeoff_resources(PlaneData* data) {
    pthread_mutex_lock(&airport_mutex);

    data->init_time_wait = time(NULL);
    priority_queue_insert(g_takeoff_queue, data);

    // ... (snprintf para log da fila) ...

    while (available_runways < 1 || available_gates < 1 || (tower_capacity_in_use >= TOWER_CAPACITY) ||
           (g_takeoff_queue->size == 0 || g_takeoff_queue->planes[0]->id != data->id))
    {
        pthread_cond_wait(&resource_cond, &airport_mutex);
        
        if (data->status == CRASHED) {
            // ... (código para sair da thread) ...
        }
    }

    // --- INÍCIO DA ADIÇÃO: Coleta de estatísticas de tempo de espera ---
    double wait_time = difftime(time(NULL), data->init_time_wait);
    if (wait_time > 0.1) {
        stats_add_wait_time(wait_time);
    }
    // --- FIM DA ADIÇÃO ---

    priority_queue_remove(g_takeoff_queue);

    // ... (resto da função sem alteração) ...
    pthread_mutex_unlock(&airport_mutex);
}

void airport_release_takeoff_resources() {
    pthread_mutex_lock(&airport_mutex);
    available_runways++;
    available_gates++;
    tower_capacity_in_use--;
    pthread_cond_broadcast(&resource_cond);
    pthread_mutex_unlock(&airport_mutex);
}