#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

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
#define MAX_AVIOES 50
// ---------------------------------

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

// ------------- DEFINIÇÕES DAS FUNÇÕES -------------
void *rotina_aviao(void *arg);
int solicitar_pista(aviao_t *aviao);
void liberar_pista(int id_aviao);
int solicitar_portao(aviao_t *aviao);
void liberar_portao(int id_aviao);
int solicitar_torre(aviao_t *aviao);
void liberar_torre(int id_aviao);
int solicitar_pouso(aviao_t *voo);
void liberar_pouso(aviao_t *voo);
int solicitar_desembarque(aviao_t *voo);
void liberar_desembarque(aviao_t *voo);
int solicitar_decolagem(aviao_t *voo);
void liberar_decolagem(aviao_t *voo);
int tentar_solicitar_recurso(sem_t *recurso, aviao_t *aviao, const char* nome_recurso);
void exibir_relatorio_final(aviao_t* avioes[], int total_avioes);
// --------------------------------------------------

// ------------- Variáveis globais -------------

// ---------------------------------------------

// ------------- THREADS -------------

// -----------------------------------

// ------------- MUTEXES -------------

// -----------------------------------

// -------------- SEMÁFOROS --------------
sem_t sem_pistas;
sem_t sem_portoes;
sem_t sem_torre_ops;
// ---------------------------------------

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

    aviao_t* avioes[MAX_AVIOES];
    int contador_avioes = 0;
    bool flag = false;

    // Inicializa o gerador de números aleatórios
    srand(time(NULL));

    // Pega o tempo de início da simulação
    time_t inicio_simulacao = time(NULL);

    printf("--- SIMULACAO INICIADA ---\n");

    // Loop principal de criação de aviões
    while (time(NULL) - inicio_simulacao < TEMPO_TOTAL) {
    if (contador_avioes < MAX_AVIOES) {
        avioes[contador_avioes] = malloc(sizeof(aviao_t));
        if (avioes[contador_avioes] == NULL) {
            perror("Falha ao alocar memoria para o aviao");
            continue; 
        }
        
        avioes[contador_avioes]->ID = contador_avioes + 1;
        avioes[contador_avioes]->tipo = (rand() % 2 == 0) ? DOMESTICO : INTERNACIONAL;
        avioes[contador_avioes]->em_alerta = false;
        avioes[contador_avioes]->tempo_de_criacao = time(NULL);

        // Cria a thread e guarda o ID direto na struct
        pthread_create(&avioes[contador_avioes]->thread_id, NULL, rotina_aviao, (void *)avioes[contador_avioes]);
        
        contador_avioes++;

            if (contador_avioes == MAX_AVIOES){
                flag = true;
                break;
            }
        }
        
        // Espera um tempo aleatório (1 a 3 segundos) para criar o próximo avião
        sleep(rand() % 3 + 1);
    }

    if (!flag)
        printf("\n--- TEMPO ESGOTADO! Nao serao criados mais avioes. Aguardando os existentes finalizarem... ---\n");
    else
        printf("\n--- LIMITE DE AVIOES ATIGINDO! Nao serao criados mais avioes. Aguardando os existentes finalizarem... ---\n");

    for (int i = 0; i < contador_avioes; i++) {
        pthread_join(avioes[i]->thread_id, NULL);
    }

    printf("\n--- SIMULACAO FINALIZADA! Todos os avioes concluiram suas operacoes. ---\n");

    sem_destroy(&sem_pistas);
    sem_destroy(&sem_portoes);
    sem_destroy(&sem_torre_ops);

    exibir_relatorio_final(avioes, contador_avioes);

    for (int i = 0; i < contador_avioes; i++) {
        free(avioes[i]);
    }

    return 0;
}

void *rotina_aviao(void *arg) {
    aviao_t *aviao = (aviao_t *)arg;
    aviao->estado = VOANDO;

    printf("AVIAO [%d] (tipo: %s) criado e se aproximando do aeroporto.\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Domestico");

    // --- 1. POUSO ---
    aviao->estado = POUSANDO;
    if (solicitar_pouso(aviao) == -1) {
        pthread_exit(NULL); // Termina a thread se o avião "caiu"
    }
    printf("AVIAO [%d] POUSANDO...\n", aviao->ID);
    sleep(2);
    liberar_pouso(aviao);
    printf("AVIAO [%d] POUSO CONCLUIDO.\n\n", aviao->ID);

    // --- 2. DESEMBARQUE ---
    aviao->estado = DESEMBARCANDO;
    if (solicitar_desembarque(aviao) == -1) {
        pthread_exit(NULL);
    }
    printf("AVIAO [%d] DESEMBARCANDO PASSAGEIROS...\n", aviao->ID);
    sleep(3);
    liberar_desembarque(aviao);
    printf("AVIAO [%d] DESEMBARQUE CONCLUIDO.\n\n", aviao->ID);

    // --- 3. DECOLAGEM ---
    aviao->estado = DECOLANDO;
    if (solicitar_decolagem(aviao) == -1) {
        pthread_exit(NULL);
    }
    printf("AVIAO [%d] DECOLANDO...\n", aviao->ID);
    sleep(2);
    liberar_decolagem(aviao);
    printf("AVIAO [%d] DECOLAGEM CONCLUIDA.\n\n", aviao->ID);

    aviao->estado = CONCLUIDO;
    printf("AVIAO [%d] concluiu todas as operacoes com sucesso.\n", aviao->ID);
    
    pthread_exit(NULL);
}


// ------------------- GERENCIAMENTO DE RECURSOS -------------------

int solicitar_pista(aviao_t *aviao) {
    printf("AVIAO [%d] solicitando PISTA...\n", aviao->ID);
    return tentar_solicitar_recurso(&sem_pistas, aviao, "PISTA");
}

void liberar_pista(int id_aviao) {
    printf("AVIAO [%d] liberando PISTA.\n", id_aviao);
    sem_post(&sem_pistas);
}

int solicitar_portao(aviao_t *aviao) {
    printf("AVIAO [%d] solicitando PORTAO...\n", aviao->ID);
    return tentar_solicitar_recurso(&sem_portoes, aviao, "PORTAO");
}

void liberar_portao(int id_aviao) {
    printf("AVIAO [%d] liberando PORTAO.\n", id_aviao);
    sem_post(&sem_portoes);
}

int solicitar_torre(aviao_t *aviao) {
    printf("AVIAO [%d] solicitando operacao da TORRE...\n", aviao->ID);
    return tentar_solicitar_recurso(&sem_torre_ops, aviao, "TORRE");
}

void liberar_torre(int id_aviao) {
    printf("AVIAO [%d] liberando operacao da TORRE.\n", id_aviao);
    sem_post(&sem_torre_ops);
}

// Retorna 0 para sucesso, -1 para falha (aviao caiu)
int tentar_solicitar_recurso(sem_t *recurso, aviao_t *aviao, const char* nome_recurso) {
    struct timespec ts;
    time_t tempo_inicio_espera = time(NULL);

    while (1) {
        // Define o timeout para 1 segundo no futuro
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        // Tenta pegar o recurso com timeout de 1 segundo
        if (sem_timedwait(recurso, &ts) == 0) {
            // Sucesso! Conseguiu o recurso.
            printf("AVIAO [%d] conseguiu alocar %s.\n", aviao->ID, nome_recurso);
            return 0; 
        }

        if (errno != ETIMEDOUT) {
            perror("sem_timedwait");
            return -1;
        }

        // Se chegou aqui, não conseguiu o recurso. Verifica o tempo total de espera.
        time_t tempo_espera_total = time(NULL) - tempo_inicio_espera;

        // Verifica se o avião "caiu"
        if (tempo_espera_total >= FALHA) {
            aviao->estado = FALHA_OPERACIONAL;
            printf("\n!!! FALHA OPERACIONAL !!! AVIAO [%d] 'caiu' esperando por %s por %ld segundos.\n\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
            return -1; // Sinaliza falha
        }

        // Verifica se entrou em estado de alerta crítico
        if (tempo_espera_total >= ALERTA_CRITICO && !aviao->em_alerta) {
            aviao->em_alerta = true;
            printf("\n!!! ALERTA CRITICO !!! AVIAO [%d] esperando por %s por %ld segundos.\n\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
        }
    }
}

// ----------------------------------------------------------------------

// ------------------- POUSO, DESEMBARQUE E DECOLAGEM -------------------

int solicitar_pouso(aviao_t *voo) {
    if (voo->tipo == INTERNACIONAL) { // Pista -> Torre
        if (solicitar_pista(voo) == -1) return -1;
        if (solicitar_torre(voo) == -1) {
            liberar_pista(voo->ID);
            return -1;
        }
    } else { // Torre -> Pista
        if (solicitar_torre(voo) == -1) return -1;
        if (solicitar_pista(voo) == -1) {
            liberar_torre(voo->ID);
            return -1;
        }
    }
    return 0;
}

void liberar_pouso(aviao_t *voo) {
    liberar_pista(voo->ID);
    liberar_torre(voo->ID);
}

int solicitar_desembarque(aviao_t *voo) {
    if (voo->tipo == INTERNACIONAL) { // Portão -> Torre
        if (solicitar_portao(voo) == -1) return -1;
        if (solicitar_torre(voo) == -1) {
            liberar_portao(voo->ID);
            return -1;
        }
    } else { // Torre -> Portão
        if (solicitar_torre(voo) == -1) return -1;
        if (solicitar_portao(voo) == -1) {
            liberar_torre(voo->ID);
            return -1;
        }
    }
    return 0;
}

void liberar_desembarque(aviao_t *voo) {
    liberar_torre(voo->ID);
    printf("AVIAO [%d] servicos de solo em andamento no portao.\n", voo->ID);
    sleep(3);
    liberar_portao(voo->ID);
}

int solicitar_decolagem(aviao_t *voo) {
    if (voo->tipo == INTERNACIONAL) { // Portão -> Pista -> Torre
        if (solicitar_portao(voo) == -1) return -1;
        if (solicitar_pista(voo) == -1) {
            liberar_portao(voo->ID);
            return -1;
        }
        if (solicitar_torre(voo) == -1) {
            liberar_portao(voo->ID);
            liberar_pista(voo->ID);
            return -1;
        }
    } else { // Torre -> Portão -> Pista
        if (solicitar_torre(voo) == -1) return -1;
        if (solicitar_portao(voo) == -1) {
            liberar_torre(voo->ID);
            return -1;
        }
        if (solicitar_pista(voo) == -1) {
            liberar_torre(voo->ID);
            liberar_portao(voo->ID);
            return -1;
        }
    }
    return 0;
}

void liberar_decolagem(aviao_t *voo) {
    liberar_torre(voo->ID);
    liberar_portao(voo->ID);
    liberar_pista(voo->ID);
}

// ------------------- RELATÓRIO FINAL -------------------

void exibir_relatorio_final(aviao_t* avioes[], int total_avioes) {
    printf("\n\n--- RELATORIO FINAL DA SIMULACAO ---\n");
    printf("--------------------------------------\n");

    int sucessos = 0, falhas_starvation = 0, domesticos = 0, internacionais = 0;
    const char* estado_str[] = { "VOANDO", "POUSANDO", "DESEMBARCANDO", "AGUARDANDO DECOLAGEM", "DECOLANDO", "CONCLUIDO", "FALHA OPERACIONAL" };

    printf("Estado final de cada aviao:\n");
    for (int i = 0; i < total_avioes; i++) {
        printf(" - Aviao ID: %-3d | Tipo: %-13s | Estado Final: %s\n",
               avioes[i]->ID,
               avioes[i]->tipo == DOMESTICO ? "Domestico" : "Internacional",
               estado_str[avioes[i]->estado]);
        
        if (avioes[i]->estado == CONCLUIDO) sucessos++;
        else if (avioes[i]->estado == FALHA_OPERACIONAL) falhas_starvation++;
        if (avioes[i]->tipo == DOMESTICO) domesticos++;
        else internacionais++;
    }

    printf("\nResumo das Metricas:\n");
    printf(" - Total de avioes criados: %d\n", total_avioes);
    printf("   - Voos Domesticos: %d\n", domesticos);
    printf("   - Voos Internacionais: %d\n", internacionais);
    printf(" - Operacoes concluidas com sucesso: %d\n", sucessos);
    printf(" - Falhas operacionais (starvation): %d\n", falhas_starvation);
    printf("--------------------------------------\n");
}