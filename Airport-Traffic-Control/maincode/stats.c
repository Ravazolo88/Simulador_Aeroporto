#include <stdio.h>
#include <pthread.h>
#include "stats.h"

typedef struct {
    // Contadores obrigatórios
    int total_flights_created;
    int deadlocks_detected;

    // Contadores detalhados
    int successful_flights_dom;
    int successful_flights_intl;
    int crashed_flights_dom;
    int crashed_flights_intl;
    int alerts_triggered;
    
    // Métricas de tempo
    double total_wait_time;
    int flights_that_waited;

} SimulationStats;

static SimulationStats stats;
static pthread_mutex_t stats_mutex;

void stats_init(void) {
    stats.total_flights_created = 0;
    stats.deadlocks_detected = 0;
    stats.successful_flights_dom = 0;
    stats.successful_flights_intl = 0;
    stats.crashed_flights_dom = 0;
    stats.crashed_flights_intl = 0;
    stats.alerts_triggered = 0;
    stats.total_wait_time = 0.0;
    stats.flights_that_waited = 0;

    if (pthread_mutex_init(&stats_mutex, NULL) != 0) {
        perror("Falha ao inicializar o mutex das estatísticas");
    }
}

void stats_destroy(void) {
    pthread_mutex_destroy(&stats_mutex);
}

void stats_increment_total_flights(void) {
    pthread_mutex_lock(&stats_mutex);
    stats.total_flights_created++;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_successful_flights(FlightType type) {
    pthread_mutex_lock(&stats_mutex);
    if (type == DOMESTIC) {
        stats.successful_flights_dom++;
    } else {
        stats.successful_flights_intl++;
    }
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_crashed_flights(FlightType type) {
    pthread_mutex_lock(&stats_mutex);
    if (type == DOMESTIC) {
        stats.crashed_flights_dom++;
    } else {
        stats.crashed_flights_intl++;
    }
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_alerts(void) {
    pthread_mutex_lock(&stats_mutex);
    stats.alerts_triggered++;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_deadlocks(void) {
    pthread_mutex_lock(&stats_mutex);
    stats.deadlocks_detected++;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_add_wait_time(double wait_time) {
    pthread_mutex_lock(&stats_mutex);
    stats.total_wait_time += wait_time;
    stats.flights_that_waited++;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_print_final_report(void) {
    pthread_mutex_lock(&stats_mutex);

    // Cálculos finais
    int total_successful = stats.successful_flights_dom + stats.successful_flights_intl;
    int total_crashed = stats.crashed_flights_dom + stats.crashed_flights_intl;
    int active_at_end = stats.total_flights_created - total_successful - total_crashed;
    double avg_wait_time = (stats.flights_that_waited > 0) ? (stats.total_wait_time / stats.flights_that_waited) : 0.0;

    printf("\n");
    printf("========================================================\n");
    printf("---           Relatório Final da Simulação           ---\n");
    printf("========================================================\n");
    printf("\n--- Visão Geral ---\n");
    printf("Total de voos criados: ......................... %d\n", stats.total_flights_created);
    printf("Voos concluídos com sucesso: ................... %d\n", total_successful);
    printf("Voos que 'caíram' por starvation: .............. %d\n", total_crashed);
    printf("Voos ainda ativos no final da simulação: ...... %d\n", active_at_end);
    
    printf("\n--- Detalhes de Desempenho ---\n");
    printf("Voos bem-sucedidos (Domésticos): .............. %d\n", stats.successful_flights_dom);
    printf("Voos bem-sucedidos (Internacionais): .......... %d\n", stats.successful_flights_intl);
    printf("Voos 'caídos' (Domésticos): ................... %d\n", stats.crashed_flights_dom);
    printf("Voos 'caídos' (Internacionais): ............... %d\n", stats.crashed_flights_intl);
    
    printf("\n--- Análise de Contenção ---\n");
    printf("Número de alertas críticos (>60s de espera): ... %d\n", stats.alerts_triggered);
    printf("Tempo médio de espera por recurso: ............. %.2f segundos\n", avg_wait_time);
    printf("Deadlocks detectados: .......................... %d (esperado: 0)\n", stats.deadlocks_detected);
    printf("--------------------------------------------------------\n");
    printf("\n");

    pthread_mutex_unlock(&stats_mutex);
}