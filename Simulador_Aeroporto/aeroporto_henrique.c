#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// ---- DEFINIÇÃO DE TEMPOS -----
int TEMPO_TOTAL    = 300;
int ALERTA_CRITICO = 60;
int FALHA          = 90;
//-------------------------------

// --------- RECURSOS -----------
int NUM_PISTAS    = 3;
int NUM_PORTOES   = 5;
int NUM_TORRES    = 1;
int NUM_OP_TORRES = 2;
// ------------------------------

// ------------ DEFINEs ------------
#define MAX_AVIOES 4
// ---------------------------------

// ------------- DEFINIÇÕES DAS FUNÇÕES -------------
void *rotina_aviao(void *arg);
void solicitar_pista(int id_aviao);
void liberar_pista(int id_aviao);
void solicitar_portao(int id_aviao);
void liberar_portao(int id_aviao);
void solicitar_torre(int id_aviao);
void liberar_torre(int id_aviao);
void solicitar_pouso(aviao_t *voo);
void liberar_pouso(aviao_t *voo);
void solicitar_desembarque(aviao_t *voo);
void liberar_desembarque(aviao_t *voo);
void solicitar_decolagem(aviao_t *voo);
void liberar_decolagem(aviao_t *voo);
// --------------------------------------------------

// ------------- Variáveis globais -------------
int contador_avioes = 0;
bool flag = false;
// ---------------------------------------------

// ------------- THREADS -------------
pthread_t threads[MAX_AVIOES];
// -----------------------------------

// ------------- MUTEXES -------------

// -----------------------------------

// -------------- SEMÁFOROS --------------
sem_t sem_pistas;
sem_t sem_portoes;
sem_t sem_torre_ops;
// ---------------------------------------

// -------------- STRUCTS --------------
typedef enum{
    DOMESTICO,
    INTERNACIONAL
} tipo_de_voo;

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
} aviao_t;
// --------------------------------------


int main(int argc, char* argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Uso: %s <torres> <pistas> <portoes> <op_torres> <tempo_total> <alerta_critico> <falha>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s 1 3 5 2 300 60 90\n", argv[0]);
        return 1;
    }

    NUM_TORRES = atoi(argv[1]);
    NUM_PISTAS = atoi(argv[2]);
    NUM_PORTOES = atoi(argv[3]);
    NUM_OP_TORRES = atoi(argv[4]);
    TEMPO_TOTAL = atoi(argv[5]);
    ALERTA_CRITICO = atoi(argv[6]);
    FALHA = atoi(argv[7]);

    printf("Simulacao iniciada com os seguintes parametros:\n");
    printf("---------------------------------------------\n");
    printf("Torres de Controle: %d\n", NUM_TORRES);
    printf("Pistas: %d\n", NUM_PISTAS);
    printf("Portoes: %d\n", NUM_PORTOES);
    printf("Operacoes simultaneas por Torre: %d\n", NUM_OP_TORRES);
    printf("Tempo total de simulacao: %d segundos\n", TEMPO_TOTAL);
    printf("Tempo para alerta critico: %d segundos\n", ALERTA_CRITICO);
    printf("Tempo para falha: %d segundos\n", FALHA);
    printf("---------------------------------------------\n\n");

    printf("Inicializando semaforos...\n");
    sem_init(&sem_pistas, 0, NUM_PISTAS);
    sem_init(&sem_portoes, 0, NUM_PORTOES);
    sem_init(&sem_torre_ops, 0, NUM_OP_TORRES);
    printf("Semaforos inicializados!\n\n");

    // Inicializa o gerador de números aleatórios
    srand(time(NULL));

    // Pega o tempo de início da simulação
    time_t inicio_simulacao = time(NULL);

    printf("--- SIMULACAO INICIADA ---\n");

    // Loop principal de criação de aviões
    while (time(NULL) - inicio_simulacao < TEMPO_TOTAL) {
        if (contador_avioes < MAX_AVIOES) {
            aviao_t *aviao = malloc(sizeof(aviao_t));
            if (aviao == NULL) {
                perror("Falha ao alocar memoria para o aviao");
                continue; // Pula para a próxima iteração
            }
            
            // Define os atributos do avião
            aviao->ID = contador_avioes + 1;
            aviao->tipo = (rand() % 2 == 0) ? DOMESTICO : INTERNACIONAL;

            // Cria a thread do avião
            pthread_create(&threads[contador_avioes], NULL, rotina_aviao, (void *)aviao);
            
            contador_avioes++;

            if (contador_avioes == MAX_AVIOES){
                flag = true;
                break;
            }
        }
        
        // Espera um tempo aleatório (0 a 3 segundos) para criar o próximo avião
        sleep(rand() % 4);
    }

    if (!flag)
        printf("\n--- TEMPO ESGOTADO! Nao serao criados mais avioes. Aguardando os existentes finalizarem... ---\n");
    else
        printf("\n--- LIMITE DE AVIOES ATIGINDO! Nao serao criados mais avioes. Aguardando os existentes finalizarem... ---\n");

    for (int i = 0; i < contador_avioes; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\n--- SIMULACAO FINALIZADA! Todos os avioes concluiram suas operacoes. ---\n");

    sem_destroy(&sem_pistas);
    sem_destroy(&sem_portoes);
    sem_destroy(&sem_torre_ops);

    return 0;
}

void *rotina_aviao(void *arg) {
    aviao_t *aviao = (aviao_t *)arg;

    printf("AVIAO [%d] (tipo: %s) criado e se aproximando do aeroporto.\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Domestico");

    // --- 1. POUSO ---
    solicitar_pouso(aviao);
    printf("AVIAO [%d] POUSANDO...\n", aviao->ID);
    sleep(2);
    liberar_pouso(aviao);
    printf("AVIAO [%d] POUSO CONCLUIDO.\n\n", aviao->ID);

    // --- 2. DESEMBARQUE ---
    solicitar_desembarque(aviao);
    printf("AVIAO [%d] DESEMBARCANDO PASSAGEIROS...\n", aviao->ID);
    sleep(3);
    liberar_desembarque(aviao);
    printf("AVIAO [%d] DESEMBARQUE CONCLUIDO.\n\n", aviao->ID);

    // --- 3. DECOLAGEM ---
    solicitar_decolagem(aviao);
    printf("AVIAO [%d] DECOLANDO...\n", aviao->ID);
    sleep(2);
    liberar_decolagem(aviao);
    printf("AVIAO [%d] DECOLAGEM CONCLUIDA.\n\n", aviao->ID);


    printf("AVIAO [%d] concluiu todas as operacoes com sucesso.\n", aviao->ID);
    
    pthread_exit(NULL);
}


// ------------------- GERENCIAMENTO DE RECURSOS -------------------

void solicitar_pista(int id_aviao) {
    printf("AVIAO [%d] solicitando PISTA...\n", id_aviao);
    sem_wait(&sem_pistas);
    printf("AVIAO [%d] PISTA alocada.\n", id_aviao);
}

void liberar_pista(int id_aviao) {
    printf("AVIAO [%d] liberando PISTA.\n", id_aviao);
    sem_post(&sem_pistas);
}

void solicitar_portao(int id_aviao) {
    printf("AVIAO [%d] solicitando PORTAO...\n", id_aviao);
    sem_wait(&sem_portoes);
    printf("AVIAO [%d] PORTAO alocado.\n", id_aviao);
}

void liberar_portao(int id_aviao) {
    printf("AVIAO [%d] liberando PORTAO.\n", id_aviao);
    sem_post(&sem_portoes);
}

void solicitar_torre(int id_aviao) {
    printf("AVIAO [%d] solicitando operacao da TORRE...\n", id_aviao);
    sem_wait(&sem_torre_ops);
    printf("AVIAO [%d] operacao da TORRE alocada.\n", id_aviao);
}

void liberar_torre(int id_aviao) {
    printf("AVIAO [%d] liberando operacao da TORRE.\n", id_aviao);
    sem_post(&sem_torre_ops);
}

// ----------------------------------------------------------------------

// ------------------- POUSO, DESEMBARQUE E DECOLAGEM -------------------

void solicitar_pouso(aviao_t *voo) {
    if (voo->tipo == INTERNACIONAL) {
        solicitar_pista(voo->ID);
        solicitar_torre(voo->ID);
    } else { // DOMESTICO
        solicitar_torre(voo->ID);
        solicitar_pista(voo->ID);
    }
}

void liberar_pouso(aviao_t *voo) {
    liberar_pista(voo->ID);
    liberar_torre(voo->ID);
}

void solicitar_desembarque(aviao_t *voo) {
    if (voo->tipo == INTERNACIONAL) {
        solicitar_portao(voo->ID);
        solicitar_torre(voo->ID);
    } else { // DOMESTICO
        solicitar_torre(voo->ID);
        solicitar_portao(voo->ID);
    }
}

void liberar_desembarque(aviao_t *voo) {
    liberar_torre(voo->ID);
    printf("AVIAO [%d] servicos de solo em andamento no portao.\n", voo->ID);
    sleep(5);
    liberar_portao(voo->ID);
}

void solicitar_decolagem(aviao_t *voo) {
    if (voo->tipo == INTERNACIONAL) {
        solicitar_portao(voo->ID);
        solicitar_pista(voo->ID);
        solicitar_torre(voo->ID);
    } else { // DOMESTICO
        solicitar_torre(voo->ID);
        solicitar_portao(voo->ID);
        solicitar_pista(voo->ID);
    }
}

void liberar_decolagem(aviao_t *voo) {
    liberar_torre(voo->ID);
    liberar_portao(voo->ID);
    liberar_pista(voo->ID);
}