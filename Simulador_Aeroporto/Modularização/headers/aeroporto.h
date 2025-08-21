#ifndef AEROPORTO_H
#define AEROPORTO_H

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "logger.h"

// ---- DEFINIÇÃO DE TEMPOS -----
extern int TEMPO_TOTAL;
extern int ALERTA_CRITICO;
extern int FALHA;

// --------- RECURSOS -----------
extern int NUM_PISTAS;
extern int NUM_PORTOES;
extern int NUM_TORRES;
extern int NUM_OP_TORRES;

// ------------ DEFINES ------------
#define MAX_AVIOES 200
#define PRIORIDADE_BASE_DOMESTICO   8
#define PRIORIDADE_BASE_INTERNACIONAL 13
#define AGING_INCREMENT 1
#define AGING_INTERVAL 5
#define MAX_DEADLOCK_WARNINGS 3

// -------------- STRUCTS --------------
typedef enum {
    DOMESTICO,
    INTERNACIONAL
} tipo_de_voo;

typedef enum {
    RECURSO_PISTA,
    RECURSO_PORTAO,
    RECURSO_TORRE
} tipo_recurso;

typedef enum {
    VOANDO,
    POUSANDO,
    DESEMBARCANDO,
    AGUARDANDO_DECOLAGEM,
    DECOLANDO,
    CONCLUIDO,
    FALHA_OPERACIONAL
} estado_aviao;

typedef struct {
    int ID;
    tipo_de_voo tipo;
    pthread_t thread_id;
    bool em_alerta;
    time_t tempo_de_criacao;
    estado_aviao estado;
    int recursos_alocados[3];
    int deadlock_warnings;
    bool recursos_realocados;
} aviao_t;

typedef struct request_node {
    aviao_t* aviao;
    tipo_recurso recurso_desejado;
    time_t tempo_chegada;
    int prioridade_atual;
    pthread_cond_t cond_var;
    bool atendido;
    struct request_node* next;
} request_node_t;

typedef struct {
    request_node_t* head;
    pthread_mutex_t mutex;
    int total_requisicoes;
} fila_prioridade_t;

typedef struct {
    int recursos_disponiveis[3];
    int matriz_alocacao[MAX_AVIOES][3];
    int matriz_requisicao[MAX_AVIOES][3];
    pthread_mutex_t mutex;
} detector_deadlock_t;

// ------------- VARIÁVEIS GLOBAIS -------------
extern detector_deadlock_t detector;
extern int contador_deadlocks;
extern int contador_starvation;
extern int recursos_realocados;
extern pthread_mutex_t mutex_contadores;
extern aviao_t* avioes_com_warnings[MAX_AVIOES];
extern int num_avioes_warnings;
extern pthread_mutex_t mutex_warnings;
extern bool sistema_ativo;
extern pthread_mutex_t mutex_lista_avioes;

// -------------- SEMÁFOROS  --------------
extern sem_t sem_pistas;
extern sem_t sem_portoes;
extern sem_t sem_torre_ops;

// -------------- FILAS DE PRIORIDADE --------------
extern fila_prioridade_t fila_pistas;
extern fila_prioridade_t fila_portoes;
extern fila_prioridade_t fila_torre_ops;


// ------------- PROTÓTIPOS DAS FUNÇÕES -------------
void* rotina_aviao(void* arg);
int solicitar_pista(aviao_t *aviao);
void liberar_pista(aviao_t *aviao);
int solicitar_portao(aviao_t *aviao);
void liberar_portao(aviao_t *aviao);
int solicitar_torre(aviao_t *aviao);
void liberar_torre(aviao_t *aviao);
int solicitar_pouso(aviao_t *voo);
void liberar_pouso(aviao_t *voo);
int solicitar_desembarque(aviao_t *voo);
void liberar_desembarque(aviao_t *voo);
int solicitar_decolagem(aviao_t *voo);
void liberar_decolagem(aviao_t *voo);
int solicitar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, aviao_t* aviao, tipo_recurso tipo, const char* nome_recurso);
void liberar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, aviao_t* aviao, const char* nome_recurso);
void inicializar_fila(fila_prioridade_t* fila);
void destruir_fila(fila_prioridade_t* fila);
int adicionar_requisicao(fila_prioridade_t* fila, aviao_t* aviao, tipo_recurso recurso);
void remover_requisicao(fila_prioridade_t* fila, aviao_t* aviao);
void* thread_aging_func(void* arg);
void atualizar_prioridades(fila_prioridade_t* fila);
void inicializar_detector_deadlock();
void* thread_detectar_deadlock(void* arg);
bool detectar_ciclo_deadlock();
void registrar_alocacao(aviao_t* aviao, tipo_recurso recurso);
void registrar_liberacao(aviao_t* aviao, tipo_recurso recurso);
void registrar_requisicao(aviao_t* aviao, tipo_recurso recurso);
void limpar_requisicao(aviao_t* aviao, tipo_recurso recurso);
void adicionar_aviao_warning(aviao_t* aviao);
void realocar_recursos_avioes_warning();
bool aviao_tem_muitos_warnings(aviao_t* aviao);
void exibir_relatorio_final(aviao_t* avioes[], int total_avioes);

#endif