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

    pthread_t threads[MAX_AVIOES];
    int contador_avioes = 0;

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

    return 0;
}

void *rotina_aviao(void *arg) {
    aviao_t *aviao = (aviao_t *)arg;

    printf("AVIAO [%d] (tipo: %s) criado e se aproximando do aeroporto.\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Domestico");

    if (aviao->tipo == INTERNACIONAL) {
        // --- ROTINA DO VOO INTERNACIONAL ---
        printf("AVIAO [%d] INTERNACIONAL iniciando procedimentos.\n", aviao->ID);

        // 1. POUSO (Pista -> Torre)
        printf("AVIAO [%d] solicitando PISTA para pouso.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PISTA]
        printf("AVIAO [%d] PISTA alocada. Solicitando TORRE para pouso.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR TORRE]
        printf("AVIAO [%d] POUSO CONCLUIDO. Liberando recursos de pouso.\n", aviao->ID);
        // [CÓDIGO PARA LIBERAR PISTA E TORRE]
        sleep(2);

        // 2. DESEMBARQUE (Portão -> Torre)
        printf("AVIAO [%d] solicitando PORTAO para desembarque.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PORTAO]
        printf("AVIAO [%d] PORTAO alocado. Solicitando TORRE para desembarque.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR TORRE]
        printf("AVIAO [%d] DESEMBARQUE CONCLUIDO. Liberando recursos de desembarque.\n", aviao->ID);
        // [CÓDIGO PARA LIBERAR PORTAO E TORRE]
        sleep(3);

        // 3. DECOLAGEM (Portão → Pista → Torre)
        printf("AVIAO [%d] solicitando PORTAO para decolagem.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR TORRE]
        printf("AVIAO [%d] PORTAO alocado. Solicitando PISTA para decolagem.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PORTAO]
        printf("AVIAO [%d] PISTA alocada. Solicitando TORRE para decolagem.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PISTA]
        printf("AVIAO [%d] DECOLAGEM CONCLUIDA. Liberando todos os recursos.\n", aviao->ID);
        // [CÓDIGO PARA LIBERAR PISTA, PORTAO E TORRE]
        sleep(2);
    } else { // Voo Doméstico
        // --- ROTINA DO VOO DOMÉSTICO ---
        printf("AVIAO [%d] DOMESTICO iniciando procedimentos.\n", aviao->ID);

        // 1. POUSO (Torre -> Pista)
        printf("AVIAO [%d] solicitando TORRE para pouso.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR TORRE]
        printf("AVIAO [%d] TORRE alocada. Solicitando PISTA para pouso.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PISTA]
        printf("AVIAO [%d] POUSO CONCLUIDO. Liberando recursos de pouso.\n", aviao->ID);
        // [CÓDIGO PARA LIBERAR PISTA E TORRE]
        sleep(2);

        // 2. DESEMBARQUE (Torre -> Portão)
        printf("AVIAO [%d] solicitando TORRE para desembarque.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR TORRE]
        printf("AVIAO [%d] TORRE alocada. Solicitando PORTAO para desembarque.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PORTAO]
        printf("AVIAO [%d] DESEMBARQUE CONCLUIDO. Liberando recursos de desembarque.\n", aviao->ID);
        // [CÓDIGO PARA LIBERAR PORTAO E TORRE]
        sleep(3);

        // 3. DECOLAGEM (Torre → Portão → Pista)
        printf("AVIAO [%d] solicitando TORRE para decolagem.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PORTAO]
        printf("AVIAO [%d] TORRE alocada. Solicitando PORTAO para decolagem.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR PISTA]
        printf("AVIAO [%d] PORTAO alocado. Solicitando PISTA para decolagem.\n", aviao->ID);
        // [CÓDIGO PARA ALOCAR TORRE]
        printf("AVIAO [%d] DECOLAGEM CONCLUIDA. Liberando todos os recursos.\n", aviao->ID);
        // [CÓDIGO PARA LIBERAR PISTA, PORTAO E TORRE]
        sleep(2);
    }

    printf("AVIAO [%d] concluiu todas as operacoes.\n", aviao->ID);

    pthread_exit(NULL);
}