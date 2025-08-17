#ifndef STATS_H
#define STATS_H

#include "config.h" // Para o FlightType

void stats_init(void);
void stats_destroy(void);

// Funções para incrementar contadores
void stats_increment_total_flights(void);
void stats_increment_successful_flights(FlightType type);
void stats_increment_crashed_flights(FlightType type);
void stats_increment_alerts(void);
void stats_increment_deadlocks(void); // Embora provavelmente não será usada

// Função para adicionar dados de tempo de espera
void stats_add_wait_time(double wait_time);

// Função para imprimir o relatório final
void stats_print_final_report(void);

#endif // STATS_H