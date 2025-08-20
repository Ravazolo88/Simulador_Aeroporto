#include "aeroporto.h"

// ---- DEFINIÇÃO DE TEMPOS -----
int TEMPO_TOTAL;
int ALERTA_CRITICO;
int FALHA;

// --------- RECURSOS -----------
int NUM_PISTAS;
int NUM_PORTOES;
int NUM_TORRES;
int NUM_OP_TORRES;

// ------------- VARIÁVEIS GLOBAIS -------------
detector_deadlock_t detector;
int contador_deadlocks = 0;
int contador_starvation = 0;
int recursos_realocados = 0;
pthread_mutex_t mutex_contadores;
aviao_t* avioes_com_warnings[MAX_AVIOES];
int num_avioes_warnings = 0;
pthread_mutex_t mutex_warnings;
bool sistema_ativo = true;
pthread_mutex_t mutex_lista_avioes;

// -------------- SEMÁFOROS --------------
sem_t sem_pistas;
sem_t sem_portoes;
sem_t sem_torre_ops;

// -------------- FILAS DE PRIORIDADE --------------
fila_prioridade_t fila_pistas;
fila_prioridade_t fila_portoes;
fila_prioridade_t fila_torre_ops;
