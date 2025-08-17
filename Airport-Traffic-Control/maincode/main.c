#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

#include "config.h"
#include "logger.h"
#include "airport.h"
#include "plane.h"
#include "stats.h"
#include "utils.h"

static volatile bool simulation_is_running = true;

void* simulation_controller_thread(void* arg) {
    int plane_id_counter = 0;

    // --- MODIFICAÇÃO AQUI ---
    // O loop agora também verifica se o contador de aviões atingiu o limite máximo.
    while (simulation_is_running && plane_id_counter < MAX_PLANES) {
        plane_id_counter++;

        PlaneData* plane_data = (PlaneData*) malloc(sizeof(PlaneData));
        if (plane_data == NULL) {
            write_log("ERRO: Falha ao alocar memoria para um novo aviao.\n");
            continue;
        }

        plane_data->id = plane_id_counter;
        plane_data->status = ACTIVE;
        plane_data->type = (rand() % 2 == 0) ? DOMESTIC : INTERNATIONAL;
        
        plane_data->init_time_wait = 0;
        plane_data->on_alert = false;
        
        pthread_t plane_thread_id;
        if (pthread_create(&plane_thread_id, NULL, plane_routine, (void*)plane_data) != 0) {
            char log_buffer[100];
            snprintf(log_buffer, sizeof(log_buffer), "ERRO: Falha ao criar a thread para o Voo %d.\n", plane_id_counter);
            write_log(log_buffer);
            free(plane_data);
            plane_id_counter--; // Decrementa para tentar criar este ID novamente
        } else {
            stats_increment_total_flights();
            pthread_detach(plane_thread_id);
        }
        
        sleep((rand() % 3) + 1);
    }

    if (plane_id_counter >= MAX_PLANES) {
        write_log("CONTROLE: Limite máximo de aviões (MAX_PLANES) atingido. Nenhuma nova thread será criada.\n");
    }

    write_log("CONTROLE: Loop de criacao de avioes finalizado.\n");
    return NULL;
}

int main(void) {
    srand(time(NULL));
    stats_init();

    if (logger_init("simulation.log") != 0) {
        fprintf(stderr, "ERRO CRITICO: Nao foi possivel inicializar o logger. Saindo.\n");
        return 1;
    }
    
    write_log("====================================================\n");
    write_log("INFO: Sistema inicializado. Iniciando simulacao...\n");
    write_log("====================================================\n");

    PriorityQueue landing_queue;
    PriorityQueue disembarking_queue;
    PriorityQueue takeoff_queue;
    priority_queue_init(&landing_queue);
    priority_queue_init(&disembarking_queue);
    priority_queue_init(&takeoff_queue);

    airport_init(&landing_queue, &disembarking_queue, &takeoff_queue);

    MonitorArgs monitor_args;
    monitor_args.landing_queue = &landing_queue;
    monitor_args.disembarking_queue = &disembarking_queue; 
    monitor_args.takeoff_queue = &takeoff_queue;          
    monitor_args.simulation_is_running = &simulation_is_running;

    pthread_t controller_tid, monitor_tid;
    pthread_create(&controller_tid, NULL, simulation_controller_thread, NULL);
    pthread_create(&monitor_tid, NULL, monitor_waiting_planes, &monitor_args);

    char log_buffer[100];
    snprintf(log_buffer, sizeof(log_buffer), "INFO: Simulacao rodando por %d segundos ou ate %d avioes serem criados.\n", TOTAL_SIMULATION_TIME, MAX_PLANES); 
    write_log(log_buffer);
    
    sleep(TOTAL_SIMULATION_TIME); 

    write_log("INFO: Tempo de simulacao encerrado. Finalizando a criacao de novos voos...\n"); 

    simulation_is_running = false;

    pthread_join(controller_tid, NULL);
    pthread_join(monitor_tid, NULL);
    write_log("INFO: Threads de controle e monitor finalizadas.\n");

    write_log("INFO: Aguardando operacoes restantes...\n");
    sleep(15);

    airport_destroy();
    priority_queue_destroy(&landing_queue);
    priority_queue_destroy(&disembarking_queue);
    priority_queue_destroy(&takeoff_queue);
    stats_destroy();

    write_log("INFO: Simulacao finalizada.\n");
    logger_close();

    printf("\nSimulacao finalizada. Verifique o arquivo 'simulation.log' para detalhes.\n");
    stats_print_final_report(); 
    
    return 0;
} 