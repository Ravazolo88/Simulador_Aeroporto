#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <ncurses.h> // Adicionado para a UI
#include <stdarg.h>  // Adicionado para a função de log

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
int avioes_concluidos = 0;
int avioes_falha = 0;
pthread_mutex_t mutex_contadores;
// ---------------------------------------------

// ------------- THREADS -------------
pthread_t thread_aging;
pthread_t thread_detector_deadlock;
bool sistema_ativo = true;
// -----------------------------------

// ------------- MUTEXES -------------
pthread_mutex_t mutex_lista_avioes;
pthread_mutex_t mutex_ncurses; // Mutex para proteger as chamadas ncurses
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

// ------------- VARIÁVEIS DA UI (NCURSES) -------------
WINDOW *win_header, *win_pistas, *win_portoes, *win_torre, *win_log, *win_sumario;
int total_avioes_criados = 0;
// -----------------------------------------------------


// ------------- DECLARAÇÕES DAS FUNÇÕES -------------
// Funções da UI
void inicializar_ui();
void destruir_ui();
void log_mensagem(const char* tipo, const char* formato, ...);
void atualizar_janelas_status();

// Funções da Simulação (protótipos já existentes)
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

// =================================================================
//                    IMPLEMENTAÇÃO DA UI (NCURSES)
// =================================================================
void inicializar_ui() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    
    // Pares de cores: 1: Info (Verde), 2: Alerta (Amarelo), 3: Falha (Vermelho), 4: Sucesso (Azul)
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);

    int height, width;
    getmaxyx(stdscr, height, width);

    // Header
    win_header = newwin(3, width, 0, 0);
    box(win_header, 0, 0);
    mvwprintw(win_header, 1, (width - 36) / 2, "SIMULAÇÃO DE CONTROLE DE TRÁFEGO AÉREO");
    wrefresh(win_header);

    // Janelas de Status
    int status_width = (width - 4) / 3;
    win_pistas = newwin(6, status_width, 4, 1);
    win_portoes = newwin(6, status_width, 4, status_width + 2);
    win_torre = newwin(6, width - 2 * status_width - 3, 4, 2 * status_width + 3);
    
    // Sumário
    win_sumario = newwin(height - 11, status_width, 11, 1);

    // Log
    win_log = newwin(height - 11, width - status_width - 2, 11, status_width + 2);
    scrollok(win_log, TRUE);

    atualizar_janelas_status();
}

void destruir_ui() {
    endwin();
}

void log_mensagem(const char* tipo, const char* formato, ...) {
    pthread_mutex_lock(&mutex_ncurses);
    
    // Define cor com base no tipo
    if (strcmp(tipo, "ALERTA") == 0) {
        wattron(win_log, COLOR_PAIR(2));
    } else if (strcmp(tipo, "FALHA") == 0) {
        wattron(win_log, COLOR_PAIR(3));
    } else if (strcmp(tipo, "SUCESSO") == 0) {
        wattron(win_log, COLOR_PAIR(4));
    } else {
        wattron(win_log, COLOR_PAIR(1));
    }
    
    va_list args;
    va_start(args, formato);
    vw_printw(win_log, formato, args);
    va_end(args);
    
    wprintw(win_log, "\n");
    
    // Desativa a cor
    if (strcmp(tipo, "ALERTA") == 0) {
        wattroff(win_log, COLOR_PAIR(2));
    } else if (strcmp(tipo, "FALHA") == 0) {
        wattroff(win_log, COLOR_PAIR(3));
    } else if (strcmp(tipo, "SUCESSO") == 0) {
        wattroff(win_log, COLOR_PAIR(4));
    } else {
        wattroff(win_log, COLOR_PAIR(1));
    }

    wrefresh(win_log);
    pthread_mutex_unlock(&mutex_ncurses);
}

void atualizar_janelas_status() {
    pthread_mutex_lock(&mutex_ncurses);

    int val;

    // --- Pistas ---
    wclear(win_pistas);
    box(win_pistas, 0, 0);
    mvwprintw(win_pistas, 1, 2, "Pistas (%d)", NUM_PISTAS);
    sem_getvalue(&sem_pistas, &val);
    mvwprintw(win_pistas, 3, 2, "Livres: %d", val);
    mvwprintw(win_pistas, 4, 2, "Ocupadas: %d", NUM_PISTAS - val);
    wrefresh(win_pistas);

    // --- Portões ---
    wclear(win_portoes);
    box(win_portoes, 0, 0);
    mvwprintw(win_portoes, 1, 2, "Portões (%d)", NUM_PORTOES);
    sem_getvalue(&sem_portoes, &val);
    mvwprintw(win_portoes, 3, 2, "Livres: %d", val);
    mvwprintw(win_portoes, 4, 2, "Ocupados: %d", NUM_PORTOES - val);
    wrefresh(win_portoes);

    // --- Torre ---
    wclear(win_torre);
    box(win_torre, 0, 0);
    mvwprintw(win_torre, 1, 2, "Operações na Torre (%d)", NUM_OP_TORRES);
    sem_getvalue(&sem_torre_ops, &val);
    mvwprintw(win_torre, 3, 2, "Disponíveis: %d", val);
    mvwprintw(win_torre, 4, 2, "Em uso: %d", NUM_OP_TORRES - val);
    wrefresh(win_torre);
    
    // --- Sumário ---
    wclear(win_sumario);
    box(win_sumario, 0, 0);
    mvwprintw(win_sumario, 1, 2, "Sumário da Simulação");
    mvwprintw(win_sumario, 3, 2, "Aviões Criados: %d", total_avioes_criados);
    mvwprintw(win_sumario, 4, 2, "Concluídos: %d", avioes_concluidos);
    mvwprintw(win_sumario, 5, 2, "Falhas: %d", avioes_falha);
    mvwprintw(win_sumario, 7, 2, "Starvation: %d", contador_starvation);
    mvwprintw(win_sumario, 8, 2, "Deadlocks: %d", contador_deadlocks);
    wrefresh(win_sumario);

    // --- Log Title ---
    box(win_log, 0, 0);
    mvwprintw(win_log, 0, 2, " Log de Eventos ");
    wrefresh(win_log);

    pthread_mutex_unlock(&mutex_ncurses);
}

// =================================================================
//                         INÍCIO DA SIMULAÇÃO
// =================================================================

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

    // Inicialização da UI
    inicializar_ui();
    pthread_mutex_init(&mutex_ncurses, NULL);

    // Inicialização dos semáforos e mutexes
    sem_init(&sem_pistas, 0, NUM_PISTAS);
    sem_init(&sem_portoes, 0, NUM_PORTOES);
    sem_init(&sem_torre_ops, 0, NUM_OP_TORRES);
    pthread_mutex_init(&mutex_lista_avioes, NULL);
    pthread_mutex_init(&mutex_contadores, NULL);

    inicializar_fila(&fila_pistas);
    inicializar_fila(&fila_portoes);
    inicializar_fila(&fila_torre_ops);
    
    inicializar_detector_deadlock();
    
    pthread_create(&thread_aging, NULL, thread_aging_func, NULL);
    pthread_create(&thread_detector_deadlock, NULL, thread_detectar_deadlock, NULL);
    
    aviao_t* avioes[MAX_AVIOES];
    int contador_avioes = 0;
    bool limite_atingido = false;

    srand(time(NULL));
    time_t inicio_simulacao = time(NULL);

    log_mensagem("INFO", "--- SIMULAÇÃO INICIADA ---");

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
            
            total_avioes_criados++;
            log_mensagem("INFO", "✈  AVIÃO [%03d] (%s) criado e se aproximando.", 
                   avioes[contador_avioes]->ID, 
                   avioes[contador_avioes]->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
            atualizar_janelas_status();
            
            contador_avioes++;

            if (contador_avioes == MAX_AVIOES) {
                limite_atingido = true;
            }
        }
        
        sleep(rand() % 3 + 1);
    }

    sistema_ativo = false;

    if (!limite_atingido)
        log_mensagem("INFO", "⏰ TEMPO ESGOTADO! Aguardando aviões existentes...");
    else
        log_mensagem("INFO", "📊 LIMITE DE AVIÕES ATINGIDO! Aguardando existentes...");

    for (int i = 0; i < contador_avioes; i++) {
        pthread_join(avioes[i]->thread_id, NULL);
    }

    pthread_cancel(thread_aging);
    pthread_cancel(thread_detector_deadlock);
    pthread_join(thread_aging, NULL);
    pthread_join(thread_detector_deadlock, NULL);

    log_mensagem("SUCESSO", "✅ SIMULAÇÃO FINALIZADA! Pressione qualquer tecla para ver o relatório final.");
    getch(); // Espera o usuário pressionar uma tecla antes de sair

    // Limpeza da UI
    destruir_ui();
    pthread_mutex_destroy(&mutex_ncurses);

    // Exibe relatório final no console padrão
    exibir_relatorio_final(avioes, contador_avioes);

    // Limpeza geral
    sem_destroy(&sem_pistas);
    sem_destroy(&sem_portoes);
    sem_destroy(&sem_torre_ops);
    destruir_fila(&fila_pistas);
    destruir_fila(&fila_portoes);
    destruir_fila(&fila_torre_ops);

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

    log_mensagem("INFO", "🛬 AVIÃO [%03d] iniciando procedimento de POUSO...", aviao->ID);
    if (solicitar_pouso(aviao) == -1) {
        pthread_exit(NULL);
    }
    log_mensagem("INFO", "✅ AVIÃO [%03d] POUSANDO...", aviao->ID);
    sleep(2);
    liberar_pouso(aviao);
    log_mensagem("SUCESSO", "✅ AVIÃO [%03d] POUSO CONCLUÍDO.", aviao->ID);

    // --- 2. DESEMBARQUE ---
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = DESEMBARCANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);
    
    log_mensagem("INFO", "👥 AVIÃO [%03d] iniciando DESEMBARQUE...", aviao->ID);
    if (solicitar_desembarque(aviao) == -1) {
        pthread_exit(NULL);
    }
    log_mensagem("INFO", "✅ AVIÃO [%03d] DESEMBARCANDO PASSAGEIROS...", aviao->ID);
    sleep(3);
    liberar_desembarque(aviao);
    log_mensagem("SUCESSO", "✅ AVIÃO [%03d] DESEMBARQUE CONCLUÍDO.", aviao->ID);

    // --- 3. DECOLAGEM ---
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = DECOLANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);
    
    log_mensagem("INFO", "🛫 AVIÃO [%03d] iniciando procedimento de DECOLAGEM...", aviao->ID);
    if (solicitar_decolagem(aviao) == -1) {
        pthread_exit(NULL);
    }
    log_mensagem("INFO", "✅ AVIÃO [%03d] DECOLANDO...", aviao->ID);
    sleep(2);
    liberar_decolagem(aviao);
    log_mensagem("SUCESSO", "✅ AVIÃO [%03d] DECOLAGEM CONCLUÍDA.", aviao->ID);

    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = CONCLUIDO;
    avioes_concluidos++;
    atualizar_janelas_status();
    pthread_mutex_unlock(&mutex_lista_avioes);

    log_mensagem("SUCESSO", "🎉 AVIÃO [%03d] concluiu todas as operações com sucesso!", aviao->ID);
    
    pthread_exit(NULL);
}

// ... (O restante do código de filas, gerenciamento de recursos, deadlock, etc., permanece o mesmo) ...

// ---- Modificações nas funções de gerenciamento de recursos para atualizar a UI ----

int solicitar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                     aviao_t* aviao, tipo_recurso tipo, const char* nome_recurso) {
    log_mensagem("INFO", "📋 AVIÃO [%03d] solicitando %s...", aviao->ID, nome_recurso);
    
    if (adicionar_requisicao(fila, aviao, tipo) == -1) {
        return -1;
    }
    
    registrar_requisicao(aviao, tipo);
    
    time_t tempo_inicio_espera = time(NULL);
    
    while (1) {
        pthread_mutex_lock(&fila->mutex);
        request_node_t* proximo = fila->head;
        
        if (proximo != NULL && proximo->aviao->ID == aviao->ID) {
            pthread_mutex_unlock(&fila->mutex);
            
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            
            if (sem_timedwait(sem_recurso, &ts) == 0) {
                remover_requisicao(fila, aviao);
                limpar_requisicao(aviao, tipo);
                registrar_alocacao(aviao, tipo);
                log_mensagem("SUCESSO", "✅ AVIÃO [%03d] conseguiu alocar %s.", aviao->ID, nome_recurso);
                atualizar_janelas_status(); // ATUALIZA UI
                return 0;
            }
        } else {
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
        
        time_t tempo_espera_total = time(NULL) - tempo_inicio_espera;
        
        if (tempo_espera_total >= FALHA) {
            pthread_mutex_lock(&mutex_lista_avioes);
            aviao->estado = FALHA_OPERACIONAL;
            avioes_falha++;
            pthread_mutex_unlock(&mutex_lista_avioes);
            
            pthread_mutex_lock(&mutex_contadores);
            contador_starvation++;
            pthread_mutex_unlock(&mutex_contadores);
            
            remover_requisicao(fila, aviao);
            limpar_requisicao(aviao, tipo);
            
            log_mensagem("FALHA", "❌ FALHA: AVIÃO [%03d] excedeu tempo esperando por %s.", 
                   aviao->ID, nome_recurso);
            atualizar_janelas_status(); // ATUALIZA UI
            return -1;
        }
        
        if (tempo_espera_total >= ALERTA_CRITICO && !aviao->em_alerta) {
            pthread_mutex_lock(&mutex_lista_avioes);
            aviao->em_alerta = true;
            pthread_mutex_unlock(&mutex_lista_avioes);
            
            log_mensagem("ALERTA", "⚠ ALERTA: AVIÃO [%03d] em espera crítica por %s.", 
                   aviao->ID, nome_recurso);
        }
    }
}

void liberar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                   aviao_t* aviao, const char* nome_recurso) {
    log_mensagem("INFO", "🔓 AVIÃO [%03d] liberando %s.", aviao->ID, nome_recurso);
    sem_post(sem_recurso);
    atualizar_janelas_status(); // ATUALIZA UI
    
    pthread_mutex_lock(&fila->mutex);
    if (fila->head != NULL) {
        pthread_cond_signal(&fila->head->cond_var);
    }
    pthread_mutex_unlock(&fila->mutex);
}

void* thread_detectar_deadlock(void* arg) {
    while (sistema_ativo) {
        if (detectar_ciclo_deadlock()) {
            pthread_mutex_lock(&mutex_contadores);
            contador_deadlocks++;
            pthread_mutex_unlock(&mutex_contadores);
            
            log_mensagem("FALHA", "🔒 DEADLOCK DETECTADO! Sistema pode estar em risco.");
            atualizar_janelas_status(); // ATUALIZA UI
        }
        sleep(5); 
    }
    return NULL;
}


// ------------------- O RESTANTE DAS FUNÇÕES (FILAS, OPERAÇÕES, RELATÓRIO) PERMANECE IDÊNTICO -------------------
// ... Cole o restante do seu código original aqui, sem alterações.
// As funções abaixo não precisam de modificação pois usam as primitivas que já foram alteradas
// ou são chamadas sem a necessidade de logar na UI (como as de inicialização/destruição de filas).

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
    
    if (aviao->tipo == INTERNACIONAL) {
        novo->prioridade_atual = PRIORIDADE_BASE_INTERNACIONAL;
    } else {
        novo->prioridade_atual = PRIORIDADE_BASE_DOMESTICO;
    }
    
    pthread_mutex_lock(&fila->mutex);
    
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
    return solicitar_recurso_com_prioridade(&fila_torre_ops, &sem_torre_ops, aviao, RECURSO_TORRE, "TORRE");
}

void liberar_torre(aviao_t *aviao) {
    registrar_liberacao(aviao, RECURSO_TORRE);
    liberar_recurso_com_prioridade(&fila_torre_ops, &sem_torre_ops, aviao, "TORRE");
}

int solicitar_pouso(aviao_t *aviao) {
    if (aviao->tipo == INTERNACIONAL) {
        if (solicitar_pista(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) {
            liberar_pista(aviao);
            return -1;
        }
    } else {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_pista(aviao) == -1) {
            liberar_torre(aviao);
            return -1;
        }
    }
    return 0;
}

void liberar_pouso(aviao_t *aviao) {
    liberar_pista(aviao);
    liberar_torre(aviao);
}

int solicitar_desembarque(aviao_t *aviao) {
    if (aviao->tipo == INTERNACIONAL) {
        if (solicitar_portao(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) {
            liberar_portao(aviao);
            return -1;
        }
    } else {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_portao(aviao) == -1) {
            liberar_torre(aviao);
            return -1;
        }
    }
    return 0;
}

void liberar_desembarque(aviao_t *aviao) {
    liberar_torre(aviao);
    sleep(1); 
    liberar_portao(aviao);
}

int solicitar_decolagem(aviao_t *aviao) {
    if (aviao->tipo == INTERNACIONAL) {
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
    return 0;
}

void liberar_decolagem(aviao_t *aviao) {
    liberar_portao(aviao);
    liberar_pista(aviao);
    liberar_torre(aviao);
}

void exibir_relatorio_final(aviao_t* avioes[], int total_avioes) {
    printf("\n\n");
    printf("████████████████████████████████████████████████████████████████████████████████\n");
    printf("██                              RELATÓRIO FINAL                              ██\n");
    printf("████████████████████████████████████████████████████████████████████████████████\n\n");
    
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
            default:
                estado_str = "INTERROMPIDO         ";
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

    if (sucessos == total_avioes) {
        printf("🎉 SIMULAÇÃO PERFEITA! Todos os aviões completaram suas operações com sucesso!\n");
    } else {
        printf("   Considere ajustar os parâmetros da simulação.\n");
    }
    
    printf("\n████████████████████████████████████████████████████████████████████████████████\n");
    printf("██                          FIM DA SIMULAÇÃO                               ██\n");
    printf("████████████████████████████████████████████████████████████████████████████████\n");
}