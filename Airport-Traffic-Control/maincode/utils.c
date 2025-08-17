#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils.h"
#include "config.h" 
#include "logger.h" 
#include "stats.h"

// --- Comunicação com o Módulo airport.c ---
// Declarações 'extern' permitem que este arquivo acesse as variáveis globais
// do aeroporto, que são necessárias para notificar as threads corretamente.
// Isso requer que 'airport_mutex' e 'resource_cond' estejam definidas no
// escopo global do arquivo airport.c (fora de qualquer função).
extern pthread_mutex_t airport_mutex;
extern pthread_cond_t resource_cond;


// --- Funções da Fila de Prioridade (Sem alterações) ---

void priority_queue_init(PriorityQueue* queue) {
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
}

void priority_queue_destroy(PriorityQueue* queue) {
    pthread_mutex_destroy(&queue->mutex);
}

void priority_queue_insert(PriorityQueue* queue, PlaneData* plane) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size >= MAX_WAITING_PLANES) {
        pthread_mutex_unlock(&queue->mutex);
        return;
    }

    plane->priority = calculate_dynamic_priority(plane);

    int i = queue->size;
    while (i > 0 && plane->priority > queue->priorities[i - 1]) {
        queue->planes[i] = queue->planes[i - 1];
        queue->priorities[i] = queue->priorities[i - 1];
        i--;
    }

    queue->planes[i] = plane;
    queue->priorities[i] = plane->priority;
    queue->size++;

    pthread_mutex_unlock(&queue->mutex);
}

PlaneData* priority_queue_remove(PriorityQueue* queue) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    PlaneData* removed_plane = queue->planes[0];

    for (int i = 0; i < queue->size - 1; i++) {
        queue->planes[i] = queue->planes[i + 1];
        queue->priorities[i] = queue->priorities[i + 1];
    }

    queue->size--;

    pthread_mutex_unlock(&queue->mutex);
    return removed_plane;
}


// --- Função de Lógica de Simulação (Sem alterações) ---

int calculate_dynamic_priority(PlaneData* plane) {
    if (plane == NULL) return 0;

    time_t now = time(NULL);
    double wait_time = difftime(now, plane->init_time_wait);

    int base_priority = (plane->type == INTERNATIONAL) ? 100 : 50;
    int aging_bonus = (int)(wait_time) * 5;
    int alert_bonus = 0;
    if (plane->on_alert) {
        alert_bonus = 10000;
    }

    return base_priority + aging_bonus + alert_bonus;
}


// --- Função da Thread do Monitor (MODIFICADA) ---

/**
 * @brief Função auxiliar que processa uma fila de espera para verificar starvation e atualizar prioridades.
 * @param queue A fila a ser processada.
 */
static void check_and_update_queue(PriorityQueue* queue) {
    // Trava a fila específica que estamos verificando
    pthread_mutex_lock(&queue->mutex);

    char log_buffer[256];
    time_t now = time(NULL);
    bool priority_has_changed = false;
    bool plane_crashed = false;

    // Itera de trás para frente para facilitar a remoção
    for (int i = queue->size - 1; i >= 0; i--) {
        PlaneData* plane = queue->planes[i];
        double wait_time = difftime(now, plane->init_time_wait);

        // Verifica se o avião deve "cair" por starvation [cite: 65]
        if (wait_time >= FALL_WAIT_TIME) {
            snprintf(log_buffer, sizeof(log_buffer), "ALERTA: Voo %d 'caiu' por starvation apos %.0f seg.\n", plane->id, wait_time);
            write_log(log_buffer);
            
            plane->status = CRASHED; // Sinaliza para a thread do avião que ela deve terminar
            stats_increment_crashed_flights(plane->type);
            plane_crashed = true;

            // Remove o avião da fila
            for (int j = i; j < queue->size - 1; j++) {
                queue->planes[j] = queue->planes[j + 1];
                queue->priorities[j] = queue->priorities[j + 1];
            }
            queue->size--;
            continue; // Pula para o próximo avião na fila
        }

        // Verifica se o avião deve entrar em estado de alerta [cite: 64]
        if (wait_time >= ALERT_WAIT_TIME && !plane->on_alert) {
            stats_increment_alerts();
            plane->on_alert = true;
            priority_has_changed = true;
            snprintf(log_buffer, sizeof(log_buffer), "ALERTA CRÍTICO: Voo %d esperando ha %.0f segundos!\n", plane->id, wait_time);
            write_log(log_buffer);
        }

        // Atualiza a prioridade do avião (aging)
        int new_priority = calculate_dynamic_priority(plane);
        if (new_priority != plane->priority) {
            plane->priority = new_priority;
            queue->priorities[i] = new_priority;
            priority_has_changed = true;
        }
    }

    // Se alguma prioridade mudou, reordena a fila
    if (priority_has_changed) {
        for (int i = 0; i < queue->size - 1; i++) {
            for (int j = 0; j < queue->size - i - 1; j++) {
                if (queue->priorities[j] < queue->priorities[j + 1]) {
                    PlaneData* temp_plane = queue->planes[j];
                    int temp_prio = queue->priorities[j];

                    queue->planes[j] = queue->planes[j + 1];
                    queue->priorities[j] = queue->priorities[j + 1];

                    queue->planes[j + 1] = temp_plane;
                    queue->priorities[j + 1] = temp_prio;
                }
            }
        }
    }

    pthread_mutex_unlock(&queue->mutex);

    // **NOVO E CRUCIAL**: Se um avião caiu, fazemos um broadcast.
    // Isso acorda a thread "caída" para que ela possa verificar seu status e terminar.
    if (plane_crashed) {
        pthread_mutex_lock(&airport_mutex);
        pthread_cond_broadcast(&resource_cond);
        pthread_mutex_unlock(&airport_mutex);
    }
}

/**
 * @brief Rotina principal da thread do monitor. Agora verifica todas as filas.
 */
void* monitor_waiting_planes(void* arg) {
    MonitorArgs* args = (MonitorArgs*)arg;

    while (*(args->simulation_is_running)) {
        sleep(1); // O monitor executa a cada 1 segundo

        // Chama a função auxiliar para cada uma das filas
        check_and_update_queue(args->landing_queue);
        check_and_update_queue(args->disembarking_queue);
        check_and_update_queue(args->takeoff_queue);
    }
    return NULL;
}