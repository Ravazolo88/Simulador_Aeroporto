
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "plane.h"
#include "airport.h"
#include "logger.h"
#include "config.h"
#include "stats.h"

void* plane_routine(void* arg) {
    PlaneData* data = (PlaneData*) arg;

    char log_buffer[256];
    const char* type_str = (data->type == INTERNATIONAL) ? "INTL" : "DOM";

    snprintf(log_buffer, sizeof(log_buffer), "INFO: Voo %s %d criado e iniciando procedimentos (Prio inicial: %d).\n", type_str, data->id, data->priority);
    write_log(log_buffer);

    // --- POUSO ---
    snprintf(log_buffer, sizeof(log_buffer), "POUSO: Voo %s %d aguardando recursos para pousar.\n", type_str, data->id);
    write_log(log_buffer);

    airport_request_landing_resources(data);
    
    snprintf(log_buffer, sizeof(log_buffer), "POUSO: Voo %s %d obteve recursos. Pousando agora...\n", type_str, data->id);
    write_log(log_buffer);
    sleep(2);
    airport_release_landing_resources();

    snprintf(log_buffer, sizeof(log_buffer), "POUSO: Voo %s %d pousou e liberou a pista/torre.\n", type_str, data->id);
    write_log(log_buffer);


    // --- DESEMBARQUE ---
    snprintf(log_buffer, sizeof(log_buffer), "DESEMBARQUE: Voo %s %d aguardando recursos para desembarcar.\n", type_str, data->id);
    write_log(log_buffer);
    airport_request_disembarking_resources(data);
    snprintf(log_buffer, sizeof(log_buffer), "DESEMBARQUE: Voo %s %d atracou no portao. Desembarcando...\n", type_str, data->id);
    write_log(log_buffer);
    sleep(3);
    airport_release_disembarking_resources();
    snprintf(log_buffer, sizeof(log_buffer), "DESEMBARQUE: Voo %s %d finalizou o desembarque e liberou o portao/torre.\n", type_str, data->id);
    write_log(log_buffer);

    // --- DECOLAGEM ---
    snprintf(log_buffer, sizeof(log_buffer), "DECOLAGEM: Voo %s %d aguardando recursos para decolar.\n", type_str, data->id);
    write_log(log_buffer);

    airport_request_takeoff_resources(data);

    snprintf(log_buffer, sizeof(log_buffer), "DECOLAGEM: Voo %s %d obteve recursos. Decolando agora...\n", type_str, data->id);
    write_log(log_buffer);
    sleep(2);
    airport_release_takeoff_resources();

    snprintf(log_buffer, sizeof(log_buffer), "SUCESSO: Voo %s %d decolou com sucesso e finalizou o ciclo completo!\n", type_str, data->id);
    write_log(log_buffer);
    
    // IMPORTANTE: Incrementa os voos bem-sucedidos APÓS completar todo o ciclo
    stats_increment_successful_flights(data->type);
    
    free(data);
    
    return NULL;
}