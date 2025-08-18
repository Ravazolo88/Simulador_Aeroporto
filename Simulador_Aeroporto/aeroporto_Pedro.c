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
#define PRIORIDADE_BASE_INTERNACIONAL 10
#define PRIORIDADE_BASE_DOMESTICO 5
#define AGING_INCREMENT 1
#define AGING_INTERVAL 5  // Incrementa prioridade a cada 5 segundos
// ---------------------------------

// -------------- STRUCTS --------------
typedef enum{
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
    int recursos_alocados[3]; // 0: pista, 1: portao, 2: torre
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

// Estrutura para detecção de deadlock
typedef struct {
    int recursos_disponiveis[3]; // pistas, portoes, torres
    int matriz_alocacao[MAX_AVIOES][3];
    int matriz_requisicao[MAX_AVIOES][3];
    pthread_mutex_t mutex;
} detector_deadlock_t;
// --------------------------------------

// ------------- VARIÁVEIS GLOBAIS -------------
detector_deadlock_t detector;
int contador_deadlocks = 0;
int contador_starvation = 0;
pthread_mutex_t mutex_contadores;
// ---------------------------------------------

// ------------- THREADS -------------
pthread_t thread_aging;
pthread_t thread_detector_deadlock;
bool sistema_ativo = true;
// -----------------------------------

// ------------- MUTEXES -------------
pthread_mutex_t mutex_lista_avioes;
// -----------------------------------

// -------------- SEMÁFOROS --------------
sem_t sem_pistas;
sem_t sem_portoes;
sem_t sem_torre_ops;
// ---------------------------------------

// -------------- FILAS DE PRIORIDADE --------------
fila_prioridade_t fila_pistas;
fila_prioridade_t fila_portoes;
fila_prioridade_t fila_torre_ops;
// -------------------------------------------------

// ------------- DECLARAÇÕES DAS FUNÇÕES -------------
void *rotina_aviao(void *arg);
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
void exibir_relatorio_final(aviao_t* avioes[], int total_avioes);
void inicializar_fila(fila_prioridade_t* fila);
void destruir_fila(fila_prioridade_t* fila);
int adicionar_requisicao(fila_prioridade_t* fila, aviao_t* aviao, tipo_recurso recurso);
void remover_requisicao(fila_prioridade_t* fila, aviao_t* aviao);
void* thread_aging_func(void* arg);
void atualizar_prioridades(fila_prioridade_t* fila);
request_node_t* obter_proximo_requisicao(fila_prioridade_t* fila);
int solicitar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                     aviao_t* aviao, tipo_recurso tipo, const char* nome_recurso);
void liberar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                   aviao_t* aviao, const char* nome_recurso);
void inicializar_detector_deadlock();
void* thread_detectar_deadlock(void* arg);
bool detectar_ciclo_deadlock();
void registrar_alocacao(aviao_t* aviao, tipo_recurso recurso);
void registrar_liberacao(aviao_t* aviao, tipo_recurso recurso);
void registrar_requisicao(aviao_t* aviao, tipo_recurso recurso);
void limpar_requisicao(aviao_t* aviao, tipo_recurso recurso);
// --------------------------------------------------

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

    printf("\n========================================\n");
    printf("  SIMULAÇÃO DE CONTROLE DE TRÁFEGO AÉREO\n");
    printf("========================================\n\n");
    printf("Parâmetros da simulação:\n");
    printf("---------------------------------------------\n");
    printf("Torres de Controle: %d\n", NUM_TORRES);
    printf("Pistas: %d\n", NUM_PISTAS);
    printf("Portões: %d\n", NUM_PORTOES);
    printf("Operações simultâneas por Torre: %d\n", NUM_OP_TORRES);
    printf("Tempo total de simulação: %d segundos\n", TEMPO_TOTAL);
    printf("Tempo para alerta crítico: %d segundos\n", ALERTA_CRITICO);
    printf("Tempo para falha: %d segundos\n", FALHA);
    printf("---------------------------------------------\n\n");

    // Inicialização
    printf("Inicializando sistema...\n");
    sem_init(&sem_pistas, 0, NUM_PISTAS);
    sem_init(&sem_portoes, 0, NUM_PORTOES);
    sem_init(&sem_torre_ops, 0, NUM_OP_TORRES);
    pthread_mutex_init(&mutex_lista_avioes, NULL);
    pthread_mutex_init(&mutex_contadores, NULL);

    // Inicializa as filas de prioridade
    inicializar_fila(&fila_pistas);
    inicializar_fila(&fila_portoes);
    inicializar_fila(&fila_torre_ops);
    
    // Inicializa detector de deadlock
    inicializar_detector_deadlock();
    
    // Cria threads auxiliares
    pthread_create(&thread_aging, NULL, thread_aging_func, NULL);
    pthread_create(&thread_detector_deadlock, NULL, thread_detectar_deadlock, NULL);
    
    aviao_t* avioes[MAX_AVIOES];
    int contador_avioes = 0;
    bool limite_atingido = false;

    srand(time(NULL));
    time_t inicio_simulacao = time(NULL);

    printf("\n--- SIMULAÇÃO INICIADA ---\n\n");

    // Loop principal de criação de aviões
    while (time(NULL) - inicio_simulacao < TEMPO_TOTAL && !limite_atingido) {
        if (contador_avioes < MAX_AVIOES) {
            avioes[contador_avioes] = malloc(sizeof(aviao_t));
            if (avioes[contador_avioes] == NULL) {
                perror("Falha ao alocar memória para o avião");
                continue;
            }
            
            avioes[contador_avioes]->ID = contador_avioes + 1;
            avioes[contador_avioes]->tipo = (rand() % 2 == 0) ? DOMESTICO : INTERNACIONAL;
            avioes[contador_avioes]->em_alerta = false;
            avioes[contador_avioes]->tempo_de_criacao = time(NULL);
            avioes[contador_avioes]->estado = VOANDO;
            memset(avioes[contador_avioes]->recursos_alocados, 0, sizeof(avioes[contador_avioes]->recursos_alocados));

            pthread_create(&avioes[contador_avioes]->thread_id, NULL, rotina_aviao, (void *)avioes[contador_avioes]);
            
            printf("✈  AVIÃO [%03d] (%s) criado e se aproximando do aeroporto.\n", 
                   avioes[contador_avioes]->ID, 
                   avioes[contador_avioes]->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
            
            contador_avioes++;

            if (contador_avioes == MAX_AVIOES) {
                limite_atingido = true;
            }
        }
        
        // Espera um tempo aleatório (1 a 3 segundos) para criar o próximo avião
        sleep(rand() % 3 + 1);
    }

    sistema_ativo = false;

    if (!limite_atingido)
        printf("\n⏰ TEMPO ESGOTADO! Não serão criados mais aviões. Aguardando os existentes finalizarem...\n");
    else
        printf("\n📊 LIMITE DE AVIÕES ATINGIDO! Aguardando os existentes finalizarem...\n");

    // Aguarda todas as threads de aviões terminarem
    for (int i = 0; i < contador_avioes; i++) {
        pthread_join(avioes[i]->thread_id, NULL);
    }

    // Finaliza threads auxiliares
    pthread_cancel(thread_aging);
    pthread_cancel(thread_detector_deadlock);
    pthread_join(thread_aging, NULL);
    pthread_join(thread_detector_deadlock, NULL);

    printf("\n✅ SIMULAÇÃO FINALIZADA! Todos os aviões concluíram suas operações.\n");

    // Limpeza
    sem_destroy(&sem_pistas);
    sem_destroy(&sem_portoes);
    sem_destroy(&sem_torre_ops);
    destruir_fila(&fila_pistas);
    destruir_fila(&fila_portoes);
    destruir_fila(&fila_torre_ops);

    exibir_relatorio_final(avioes, contador_avioes);

    for (int i = 0; i < contador_avioes; i++) {
        free(avioes[i]);
    }

    pthread_mutex_destroy(&mutex_lista_avioes);
    pthread_mutex_destroy(&mutex_contadores);
    pthread_mutex_destroy(&detector.mutex);

    return 0;
}

void *rotina_aviao(void *arg) {
    aviao_t *aviao = (aviao_t *)arg;

    // --- 1. POUSO ---
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = POUSANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);

    printf("🛬 AVIÃO [%03d] iniciando procedimento de POUSO...\n", aviao->ID);
    if (solicitar_pouso(aviao) == -1) {
        pthread_exit(NULL);
    }
    printf("✅ AVIÃO [%03d] POUSANDO...\n", aviao->ID);
    sleep(2);
    liberar_pouso(aviao);
    printf("✅ AVIÃO [%03d] POUSO CONCLUÍDO.\n\n", aviao->ID);

    // --- 2. DESEMBARQUE ---
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = DESEMBARCANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);
    
    printf("👥 AVIÃO [%03d] iniciando DESEMBARQUE...\n", aviao->ID);
    if (solicitar_desembarque(aviao) == -1) {
        pthread_exit(NULL);
    }
    printf("✅ AVIÃO [%03d] DESEMBARCANDO PASSAGEIROS...\n", aviao->ID);
    sleep(3);
    liberar_desembarque(aviao);
    printf("✅ AVIÃO [%03d] DESEMBARQUE CONCLUÍDO.\n\n", aviao->ID);

    // --- 3. DECOLAGEM ---
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = DECOLANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);
    
    printf("🛫 AVIÃO [%03d] iniciando procedimento de DECOLAGEM...\n", aviao->ID);
    if (solicitar_decolagem(aviao) == -1) {
        pthread_exit(NULL);
    }
    printf("✅ AVIÃO [%03d] DECOLANDO...\n", aviao->ID);
    sleep(2);
    liberar_decolagem(aviao);
    printf("✅ AVIÃO [%03d] DECOLAGEM CONCLUÍDA.\n\n", aviao->ID);

    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = CONCLUIDO;
    pthread_mutex_unlock(&mutex_lista_avioes);

    printf("🎉 AVIÃO [%03d] concluiu todas as operações com sucesso!\n", aviao->ID);
    
    pthread_exit(NULL);
}

// ------------------- FILAS DE PRIORIDADE -------------------

void inicializar_fila(fila_prioridade_t* fila) {
    fila->head = NULL;
    fila->total_requisicoes = 0;
    pthread_mutex_init(&fila->mutex, NULL);
}

void destruir_fila(fila_prioridade_t* fila) {
    pthread_mutex_lock(&fila->mutex);
    request_node_t* atual = fila->head;
    while (atual != NULL) {
        request_node_t* proximo = atual->next;
        pthread_cond_destroy(&atual->cond_var);
        free(atual);
        atual = proximo;
    }
    pthread_mutex_unlock(&fila->mutex);
    pthread_mutex_destroy(&fila->mutex);
}

int adicionar_requisicao(fila_prioridade_t* fila, aviao_t* aviao, tipo_recurso recurso) {
    request_node_t* novo = malloc(sizeof(request_node_t));
    if (novo == NULL) return -1;
    
    novo->aviao = aviao;
    novo->recurso_desejado = recurso;
    novo->tempo_chegada = time(NULL);
    novo->atendido = false;
    novo->next = NULL;
    pthread_cond_init(&novo->cond_var, NULL);
    
    // Define prioridade inicial baseada no tipo de voo
    if (aviao->tipo == INTERNACIONAL) {
        novo->prioridade_atual = PRIORIDADE_BASE_INTERNACIONAL;
    } else {
        novo->prioridade_atual = PRIORIDADE_BASE_DOMESTICO;
    }
    
    pthread_mutex_lock(&fila->mutex);
    
    // Insere ordenado por prioridade
    if (fila->head == NULL || fila->head->prioridade_atual < novo->prioridade_atual) {
        novo->next = fila->head;
        fila->head = novo;
    } else {
        request_node_t* atual = fila->head;
        while (atual->next != NULL && atual->next->prioridade_atual >= novo->prioridade_atual) {
            atual = atual->next;
        }
        novo->next = atual->next;
        atual->next = novo;
    }
    
    fila->total_requisicoes++;
    pthread_mutex_unlock(&fila->mutex);
    
    return 0;
}

void remover_requisicao(fila_prioridade_t* fila, aviao_t* aviao) {
    pthread_mutex_lock(&fila->mutex);
    
    request_node_t* atual = fila->head;
    request_node_t* anterior = NULL;
    
    while (atual != NULL) {
        if (atual->aviao->ID == aviao->ID) {
            if (anterior == NULL) {
                fila->head = atual->next;
            } else {
                anterior->next = atual->next;
            }
            pthread_cond_destroy(&atual->cond_var);
            free(atual);
            fila->total_requisicoes--;
            break;
        }
        anterior = atual;
        atual = atual->next;
    }
    
    pthread_mutex_unlock(&fila->mutex);
}

request_node_t* obter_proximo_requisicao(fila_prioridade_t* fila) {
    pthread_mutex_lock(&fila->mutex);
    request_node_t* proximo = fila->head;
    pthread_mutex_unlock(&fila->mutex);
    return proximo;
}

void atualizar_prioridades(fila_prioridade_t* fila) {
    pthread_mutex_lock(&fila->mutex);
    
    request_node_t* atual = fila->head;
    time_t agora = time(NULL);
    
    while (atual != NULL) {
        time_t tempo_espera = agora - atual->tempo_chegada;
        if (tempo_espera > 0 && tempo_espera % AGING_INTERVAL == 0) {
            atual->prioridade_atual += AGING_INCREMENT;
        }
        atual = atual->next;
    }
    
    // Reordena a fila após atualizar prioridades
    if (fila->head != NULL && fila->head->next != NULL) {
        request_node_t* sorted = NULL;
        request_node_t* current = fila->head;
        
        while (current != NULL) {
            request_node_t* next = current->next;
            
            if (sorted == NULL || sorted->prioridade_atual < current->prioridade_atual) {
                current->next = sorted;
                sorted = current;
            } else {
                request_node_t* temp = sorted;
                while (temp->next != NULL && temp->next->prioridade_atual >= current->prioridade_atual) {
                    temp = temp->next;
                }
                current->next = temp->next;
                temp->next = current;
            }
            
            current = next;
        }
        
        fila->head = sorted;
    }
    
    pthread_mutex_unlock(&fila->mutex);
}

void* thread_aging_func(void* arg) {
    while (sistema_ativo) {
        atualizar_prioridades(&fila_pistas);
        atualizar_prioridades(&fila_portoes);
        atualizar_prioridades(&fila_torre_ops);
        sleep(1);
    }
    return NULL;
}

// ------------------- GERENCIAMENTO DE RECURSOS COM PRIORIDADE -------------------

int solicitar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                     aviao_t* aviao, tipo_recurso tipo, const char* nome_recurso) {
    printf("📋 AVIÃO [%03d] solicitando %s...\n", aviao->ID, nome_recurso);
    
    // Adiciona à fila de prioridade
    if (adicionar_requisicao(fila, aviao, tipo) == -1) {
        return -1;
    }
    
    // Registra requisição para detecção de deadlock
    registrar_requisicao(aviao, tipo);
    
    time_t tempo_inicio_espera = time(NULL);
    
    while (1) {
        pthread_mutex_lock(&fila->mutex);
        request_node_t* proximo = fila->head;
        
        // Verifica se este avião é o próximo na fila
        if (proximo != NULL && proximo->aviao->ID == aviao->ID) {
            pthread_mutex_unlock(&fila->mutex);
            
            // Tenta adquirir o recurso
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            
            if (sem_timedwait(sem_recurso, &ts) == 0) {
                // Sucesso!
                remover_requisicao(fila, aviao);
                limpar_requisicao(aviao, tipo);
                registrar_alocacao(aviao, tipo);
                printf("✅ AVIÃO [%03d] conseguiu alocar %s.\n", aviao->ID, nome_recurso);
                return 0;
            }
        } else {
            // Espera sua vez
            request_node_t* meu_node = fila->head;
            while (meu_node != NULL && meu_node->aviao->ID != aviao->ID) {
                meu_node = meu_node->next;
            }
            
            if (meu_node != NULL) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 1;
                pthread_cond_timedwait(&meu_node->cond_var, &fila->mutex, &ts);
            }
            pthread_mutex_unlock(&fila->mutex);
        }
        
        // Verifica tempo de espera
        time_t tempo_espera_total = time(NULL) - tempo_inicio_espera;
        
        if (tempo_espera_total >= FALHA) {
            pthread_mutex_lock(&mutex_lista_avioes);
            aviao->estado = FALHA_OPERACIONAL;
            pthread_mutex_unlock(&mutex_lista_avioes);
            
            pthread_mutex_lock(&mutex_contadores);
            contador_starvation++;
            pthread_mutex_unlock(&mutex_contadores);
            
            remover_requisicao(fila, aviao);
            limpar_requisicao(aviao, tipo);
            
            printf("\n❌❌❌ FALHA OPERACIONAL ❌❌❌\n");
            printf("AVIÃO [%03d] excedeu tempo limite esperando por %s (%ld segundos)\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
            printf("❌❌❌❌❌❌❌❌❌❌❌❌❌❌❌\n\n");
            return -1;
        }
        
        if (tempo_espera_total >= ALERTA_CRITICO && !aviao->em_alerta) {
            pthread_mutex_lock(&mutex_lista_avioes);
            aviao->em_alerta = true;
            pthread_mutex_unlock(&mutex_lista_avioes);
            
            printf("\n⚠⚠⚠ ALERTA CRÍTICO ⚠⚠⚠\n");
            printf("AVIÃO [%03d] em situação crítica esperando por %s (%ld segundos)\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
            printf("⚠⚠⚠⚠⚠⚠⚠⚠⚠⚠⚠\n\n");
        }
    }
}

void liberar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                   aviao_t* aviao, const char* nome_recurso) {
    printf("🔓 AVIÃO [%03d] liberando %s.\n", aviao->ID, nome_recurso);
    sem_post(sem_recurso);
    
    // Notifica o próximo na fila
    pthread_mutex_lock(&fila->mutex);
    if (fila->head != NULL) {
        pthread_cond_signal(&fila->head->cond_var);
    }
    pthread_mutex_unlock(&fila->mutex);
}

// ------------------- DETECTOR DE DEADLOCK -------------------

void inicializar_detector_deadlock() {
    pthread_mutex_init(&detector.mutex, NULL);
    detector.recursos_disponiveis[0] = NUM_PISTAS;
    detector.recursos_disponiveis[1] = NUM_PORTOES;
    detector.recursos_disponiveis[2] = NUM_OP_TORRES;
    
    memset(detector.matriz_alocacao, 0, sizeof(detector.matriz_alocacao));
    memset(detector.matriz_requisicao, 0, sizeof(detector.matriz_requisicao));
}

void registrar_alocacao(aviao_t* aviao, tipo_recurso recurso) {
    pthread_mutex_lock(&detector.mutex);
    detector.matriz_alocacao[aviao->ID - 1][recurso] = 1;
    detector.recursos_disponiveis[recurso]--;
    aviao->recursos_alocados[recurso] = 1;
    pthread_mutex_unlock(&detector.mutex);
}

void registrar_liberacao(aviao_t* aviao, tipo_recurso recurso) {
    pthread_mutex_lock(&detector.mutex);
    detector.matriz_alocacao[aviao->ID - 1][recurso] = 0;
    detector.recursos_disponiveis[recurso]++;
    aviao->recursos_alocados[recurso] = 0;
    pthread_mutex_unlock(&detector.mutex);
}

void registrar_requisicao(aviao_t* aviao, tipo_recurso recurso) {
    pthread_mutex_lock(&detector.mutex);
    detector.matriz_requisicao[aviao->ID - 1][recurso] = 1;
    pthread_mutex_unlock(&detector.mutex);
}

void limpar_requisicao(aviao_t* aviao, tipo_recurso recurso) {
    pthread_mutex_lock(&detector.mutex);
    detector.matriz_requisicao[aviao->ID - 1][recurso] = 0;
    pthread_mutex_unlock(&detector.mutex);
}

bool detectar_ciclo_deadlock() {
    pthread_mutex_lock(&detector.mutex);
    
    // Implementação simplificada de detecção de deadlock
    // Verifica se há aviões esperando por recursos que estão alocados para outros aviões
    // que também estão esperando
    
    bool deadlock_detectado = false;
    int avioes_esperando = 0;
    int recursos_bloqueados = 0;
    
    for (int i = 0; i < MAX_AVIOES; i++) {
        bool esperando = false;
        bool tem_recursos = false;
        
        for (int j = 0; j < 3; j++) {
            if (detector.matriz_requisicao[i][j] > 0) esperando = true;
            if (detector.matriz_alocacao[i][j] > 0) tem_recursos = true;
        }
        
        if (esperando && tem_recursos) {
            avioes_esperando++;
        }
    }
    
    // Se há múltiplos aviões esperando e segurando recursos, pode haver deadlock
    if (avioes_esperando >= 2) {
        for (int j = 0; j < 3; j++) {
            if (detector.recursos_disponiveis[j] == 0) {
                recursos_bloqueados++;
            }
        }
        
        if (recursos_bloqueados >= 2) {
            deadlock_detectado = true;
        }
    }
    
    pthread_mutex_unlock(&detector.mutex);
    return deadlock_detectado;
}

void* thread_detectar_deadlock(void* arg) {
    while (sistema_ativo) {
        if (detectar_ciclo_deadlock()) {
            pthread_mutex_lock(&mutex_contadores);
            contador_deadlocks++;
            pthread_mutex_unlock(&mutex_contadores);
            
            printf("\n🔒🔒🔒 POSSÍVEL DEADLOCK DETECTADO 🔒🔒🔒\n");
            printf("Sistema pode estar em deadlock. Verificar logs.\n");
            printf("🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒\n\n");
        }
        sleep(5); // Verifica a cada 5 segundos
    }
    return NULL;
}

// ------------------- FUNÇÕES DE RECURSOS INDIVIDUAIS -------------------

int solicitar_pista(aviao_t *aviao) {
    return solicitar_recurso_com_prioridade(&fila_pistas, &sem_pistas, aviao, RECURSO_PISTA, "PISTA");
}

void liberar_pista(aviao_t *aviao) {
    registrar_liberacao(aviao, RECURSO_PISTA);
    liberar_recurso_com_prioridade(&fila_pistas, &sem_pistas, aviao, "PISTA");
}

int solicitar_portao(aviao_t *aviao) {
    return solicitar_recurso_com_prioridade(&fila_portoes, &sem_portoes, aviao, RECURSO_PORTAO, "PORTÃO");
}

void liberar_portao(aviao_t *aviao) {
    registrar_liberacao(aviao, RECURSO_PORTAO);
    liberar_recurso_com_prioridade(&fila_portoes, &sem_portoes, aviao, "PORTÃO");
}

int solicitar_torre(aviao_t *aviao) {
    return solicitar_recurso_com_prioridade(&fila_torre_ops, &sem_torre_ops, aviao, RECURSO_TORRE, "TORRE DE CONTROLE");
}

void liberar_torre(aviao_t *aviao) {
    registrar_liberacao(aviao, RECURSO_TORRE);
    liberar_recurso_com_prioridade(&fila_torre_ops, &sem_torre_ops, aviao, "TORRE DE CONTROLE");
}

// ------------------- OPERAÇÕES COMPLEXAS (POUSO, DESEMBARQUE, DECOLAGEM) -------------------

int solicitar_pouso(aviao_t *aviao) {
    printf("🛬 AVIÃO [%03d] (%s) solicitando recursos para POUSO...\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
    
    if (aviao->tipo == INTERNACIONAL) {
        // Internacional: Pista → Torre
        if (solicitar_pista(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) {
            liberar_pista(aviao);
            return -1;
        }
    } else {
        // Doméstico: Torre → Pista
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_pista(aviao) == -1) {
            liberar_torre(aviao);
            return -1;
        }
    }
    
    printf("✅ AVIÃO [%03d] obteve todos os recursos para POUSO (Pista + Torre).\n", aviao->ID);
    return 0;
}

void liberar_pouso(aviao_t *aviao) {
    printf("🔓 AVIÃO [%03d] liberando recursos do POUSO...\n", aviao->ID);
    liberar_pista(aviao);
    liberar_torre(aviao);
}

int solicitar_desembarque(aviao_t *aviao) {
    printf("👥 AVIÃO [%03d] (%s) solicitando recursos para DESEMBARQUE...\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
    
    if (aviao->tipo == INTERNACIONAL) {
        // Internacional: Portão → Torre
        if (solicitar_portao(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) {
            liberar_portao(aviao);
            return -1;
        }
    } else {
        // Doméstico: Torre → Portão
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_portao(aviao) == -1) {
            liberar_torre(aviao);
            return -1;
        }
    }
    
    printf("✅ AVIÃO [%03d] obteve todos os recursos para DESEMBARQUE (Portão + Torre).\n", aviao->ID);
    return 0;
}

void liberar_desembarque(aviao_t *aviao) {
    printf("🔓 AVIÃO [%03d] liberando recursos do DESEMBARQUE...\n", aviao->ID);
    liberar_torre(aviao);
    sleep(1); // Torre liberada primeiro, portão fica um tempo
    liberar_portao(aviao);
}

int solicitar_decolagem(aviao_t *aviao) {
    printf("🛫 AVIÃO [%03d] (%s) solicitando recursos para DECOLAGEM...\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
    
    if (aviao->tipo == INTERNACIONAL) {
        // Internacional: Portão → Pista → Torre
        if (solicitar_portao(aviao) == -1) return -1;
        if (solicitar_pista(aviao) == -1) {
            liberar_portao(aviao);
            return -1;
        }
        if (solicitar_torre(aviao) == -1) {
            liberar_pista(aviao);
            liberar_portao(aviao);
            return -1;
        }
    } else {
        // Doméstico: Torre → Portão → Pista
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_portao(aviao) == -1) {
            liberar_torre(aviao);
            return -1;
        }
        if (solicitar_pista(aviao) == -1) {
            liberar_torre(aviao);
            liberar_portao(aviao);
            return -1;
        }
    }
    
    printf("✅ AVIÃO [%03d] obteve todos os recursos para DECOLAGEM (Portão + Pista + Torre).\n", aviao->ID);
    return 0;
}

void liberar_decolagem(aviao_t *aviao) {
    printf("🔓 AVIÃO [%03d] liberando todos os recursos da DECOLAGEM...\n", aviao->ID);
    liberar_portao(aviao);
    liberar_pista(aviao);
    liberar_torre(aviao);
}

// ------------------- RELATÓRIO FINAL -------------------

void exibir_relatorio_final(aviao_t* avioes[], int total_avioes) {
    printf("\n\n");
    printf("████████████████████████████████████████████████████████████████████████████████\n");
    printf("██                              RELATÓRIO FINAL                              ██\n");
    printf("████████████████████████████████████████████████████████████████████████████████\n\n");
    
    // Contadores para o relatório
    int sucessos = 0, falhas = 0;
    int internacionais = 0, domesticos = 0;
    int internacionais_sucesso = 0, domesticos_sucesso = 0;
    int internacionais_falha = 0, domesticos_falha = 0;
    
    printf("📊 RESUMO DOS AVIÕES:\n");
    printf("═══════════════════════════════════════════════════════════════════════════════════\n");
    printf("| ID  | Tipo          | Estado Final           | Tempo de Vida (s) | Alertas     |\n");
    printf("═══════════════════════════════════════════════════════════════════════════════════\n");
    
    time_t tempo_atual = time(NULL);
    
    for (int i = 0; i < total_avioes; i++) {
        aviao_t* aviao = avioes[i];
        
        const char* tipo_str = (aviao->tipo == INTERNACIONAL) ? "Internacional" : "Doméstico    ";
        const char* estado_str;
        const char* alerta_str = aviao->em_alerta ? "SIM" : "NÃO";
        
        // Conta tipos
        if (aviao->tipo == INTERNACIONAL) {
            internacionais++;
        } else {
            domesticos++;
        }
        
        switch (aviao->estado) {
            case CONCLUIDO:
                estado_str = "SUCESSO              ";
                sucessos++;
                if (aviao->tipo == INTERNACIONAL) internacionais_sucesso++;
                else domesticos_sucesso++;
                break;
            case FALHA_OPERACIONAL:
                estado_str = "FALHA OPERACIONAL    ";
                falhas++;
                if (aviao->tipo == INTERNACIONAL) internacionais_falha++;
                else domesticos_falha++;
                break;
            case POUSANDO:
                estado_str = "POUSANDO (INTERROMP.)";
                falhas++;
                if (aviao->tipo == INTERNACIONAL) internacionais_falha++;
                else domesticos_falha++;
                break;
            case DESEMBARCANDO:
                estado_str = "DESEMBARC.(INTERROMP)";
                falhas++;
                if (aviao->tipo == INTERNACIONAL) internacionais_falha++;
                else domesticos_falha++;
                break;
            case DECOLANDO:
                estado_str = "DECOLANDO (INTERROMP)";
                falhas++;
                if (aviao->tipo == INTERNACIONAL) internacionais_falha++;
                else domesticos_falha++;
                break;
            default:
                estado_str = "ESTADO DESCONHECIDO  ";
                falhas++;
                if (aviao->tipo == INTERNACIONAL) internacionais_falha++;
                else domesticos_falha++;
                break;
        }
        
        time_t tempo_vida = tempo_atual - aviao->tempo_de_criacao;
        
        printf("| %03d | %s | %s | %8ld          | %s         |\n",
               aviao->ID, tipo_str, estado_str, tempo_vida, alerta_str);
    }
    
    printf("═══════════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("📈 ESTATÍSTICAS GERAIS:\n");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("Total de Aviões Criados:        %d\n", total_avioes);
    printf("Operações Bem-Sucedidas:        %d (%.1f%%)\n", sucessos, 
           total_avioes > 0 ? (float)sucessos * 100 / total_avioes : 0);
    printf("Operações com Falha:            %d (%.1f%%)\n", falhas,
           total_avioes > 0 ? (float)falhas * 100 / total_avioes : 0);
    printf("─────────────────────────────────────────────────────────────────────────\n\n");
    
    printf("✈️  AVIÕES INTERNACIONAIS:\n");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("Total:                          %d\n", internacionais);
    printf("Sucessos:                       %d (%.1f%%)\n", internacionais_sucesso,
           internacionais > 0 ? (float)internacionais_sucesso * 100 / internacionais : 0);
    printf("Falhas:                         %d (%.1f%%)\n", internacionais_falha,
           internacionais > 0 ? (float)internacionais_falha * 100 / internacionais : 0);
    printf("─────────────────────────────────────────────────────────────────────────\n\n");
    
    printf("🏠 AVIÕES DOMÉSTICOS:\n");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("Total:                          %d\n", domesticos);
    printf("Sucessos:                       %d (%.1f%%)\n", domesticos_sucesso,
           domesticos > 0 ? (float)domesticos_sucesso * 100 / domesticos : 0);
    printf("Falhas:                         %d (%.1f%%)\n", domesticos_falha,
           domesticos > 0 ? (float)domesticos_falha * 100 / domesticos : 0);
    printf("─────────────────────────────────────────────────────────────────────────\n\n");
    
    printf("⚠️  PROBLEMAS DETECTADOS:\n");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("Possíveis Deadlocks Detectados: %d\n", contador_deadlocks);
    printf("Casos de Starvation:            %d\n", contador_starvation);
    printf("─────────────────────────────────────────────────────────────────────────\n\n");
    
    printf("🔧 CONFIGURAÇÃO DA SIMULAÇÃO:\n");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("Pistas Disponíveis:             %d\n", NUM_PISTAS);
    printf("Portões Disponíveis:            %d\n", NUM_PORTOES);
    printf("Torres de Controle:             %d\n", NUM_TORRES);
    printf("Operações Simultâneas/Torre:    %d\n", NUM_OP_TORRES);
    printf("Tempo Total de Simulação:       %d segundos\n", TEMPO_TOTAL);
    printf("Tempo para Alerta Crítico:      %d segundos\n", ALERTA_CRITICO);
    printf("Tempo para Falha:               %d segundos\n", FALHA);
    printf("─────────────────────────────────────────────────────────────────────────\n\n");
    
    // Análise de desempenho
    if (contador_deadlocks > 0) {
        printf("🔒 ANÁLISE DE DEADLOCKS:\n");
        printf("   Foram detectados possíveis deadlocks durante a simulação.\n");
        printf("   Isso pode indicar que a ordem de alocação de recursos entre\n");
        printf("   voos domésticos e internacionais está causando bloqueios mútuos.\n\n");
    }
    
    if (contador_starvation > 0) {
        printf("⏰ ANÁLISE DE STARVATION:\n");
        printf("   %d aviões sofreram starvation (tempo limite excedido).\n", contador_starvation);
        printf("   Isso pode indicar que a priorização de voos internacionais\n");
        printf("   está impedindo voos domésticos de obterem recursos.\n\n");
    }
    
    if (sucessos == total_avioes) {
        printf("🎉 SIMULAÇÃO PERFEITA! Todos os aviões completaram suas operações com sucesso!\n");
    } else if ((float)sucessos / total_avioes >= 0.8) {
        printf("✅ BOM DESEMPENHO! %.1f%% dos aviões completaram suas operações.\n", 
               (float)sucessos * 100 / total_avioes);
    } else if ((float)sucessos / total_avioes >= 0.5) {
        printf("⚠️  DESEMPENHO MODERADO. %.1f%% dos aviões completaram suas operações.\n", 
               (float)sucessos * 100 / total_avioes);
    } else {
        printf("❌ DESEMPENHO RUIM. Apenas %.1f%% dos aviões completaram suas operações.\n", 
               (float)sucessos * 100 / total_avioes);
        printf("   Considere ajustar os parâmetros da simulação.\n");
    }
    
    printf("\n████████████████████████████████████████████████████████████████████████████████\n");
    printf("██                          FIM DA SIMULAÇÃO                               ██\n");
    printf("████████████████████████████████████████████████████████████████████████████████\n");
}
