#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include "plane.h"
#include "stats.h" // Para que o monitor possa chamar as funções de estatísticas

// Define um tamanho máximo para a fila de espera.
#define MAX_WAITING_PLANES 50

// Estrutura para a Fila de Prioridade
typedef struct {
    PlaneData* planes[MAX_WAITING_PLANES]; // Armazena ponteiros para os dados dos aviões
    int priorities[MAX_WAITING_PLANES];    // Armazena a prioridade calculada de cada avião
    int size;                              // Número atual de aviões na fila
    pthread_mutex_t mutex;                 // Mutex para garantir acesso thread-safe à fila
} PriorityQueue;

// Estrutura para passar argumentos para a thread do monitor
typedef struct {
    PriorityQueue* landing_queue;
    PriorityQueue* disembarking_queue; // ADICIONAR
    PriorityQueue* takeoff_queue;      // ADICIONAR
    volatile bool* simulation_is_running;
} MonitorArgs;

// --- Funções da Fila de Prioridade ---

/**
 * @brief Inicializa a fila de prioridade, zerando seu tamanho e inicializando o mutex.
 * @param queue Ponteiro para a PriorityQueue a ser inicializada.
 */
void priority_queue_init(PriorityQueue* queue);

/**
 * @brief Destrói a fila de prioridade, liberando o mutex.
 * @param queue Ponteiro para a PriorityQueue a ser destruída.
 */
void priority_queue_destroy(PriorityQueue* queue);

/**
 * @brief Insere um avião na fila de prioridade, mantendo a ordem (maior prioridade primeiro).
 * @param queue Ponteiro para a PriorityQueue.
 * @param plane Ponteiro para os dados do avião a ser inserido.
 */
void priority_queue_insert(PriorityQueue* queue, PlaneData* plane);

/**
 * @brief Remove e retorna o avião com a maior prioridade da fila (o primeiro elemento).
 * @param queue Ponteiro para a PriorityQueue.
 * @return Ponteiro para os dados do avião removido, ou NULL se a fila estiver vazia.
 */
PlaneData* priority_queue_remove(PriorityQueue* queue);


// --- Funções de Lógica de Simulação ---

/**
 * @brief Calcula a prioridade dinâmica de um avião com base no tempo de espera (aging).
 * @param plane Ponteiro para os dados do avião.
 * @return O valor da prioridade calculada.
 */
int calculate_dynamic_priority(PlaneData* plane);


// --- Função da Thread do Monitor ---

/**
 * @brief Rotina da thread que monitora os aviões na fila de espera.
 * @param arg Ponteiro para a estrutura MonitorArgs contendo os argumentos da thread.
 * @return NULL.
 */
void* monitor_waiting_planes(void* arg);

#endif // UTILS_H