//#include <pthread.h>
//#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// ---- DEFINIÇÃO DE TEMPOS (Valores Padrão) -----
int TEMPO_TOTAL    = 300; // 5 minutos
int ALERTA_CRITICO = 60;
int FALHA          = 90;
//------------------------------

// --------- RECURSOS (Valores Padrão) ----------
int NUM_PISTAS    = 3;
int NUM_PORTOES   = 5;
int NUM_TORRES    = 1; // O problema descreve 1 torre
int NUM_OP_TORRES = 2; // ...mas com capacidade para 2 operações
// ------------------------------

// ------------- Variáveis globais -------------
static FILE* arquivo_log = NULL; // Ponteiro para o arquivo de log

// ---------------------------------------------

// ------------- MUTEXES -------------
// static pthread_mutex_t log_mutex; // Descomente quando pthreads for incluído
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
    //pthread_t thread_id;
    bool em_alerta;
    time_t tempo_de_criacao;
    estado_aviao estado;
} aviao_t;
// -------------------------------------

// --- FUNÇÕES DE LOG (REAPROVEITADAS DO logger.c) ---

/**
 * @brief Inicializa o sistema de log, abrindo o arquivo para escrita.
 * @param nome_arquivo O nome do arquivo de log a ser criado (ex: "simulacao.log").
 * @return int Retorna 0 em caso de sucesso, -1 em caso de erro.
 */
int iniciar_log(const char* nome_arquivo) {
    arquivo_log = fopen(nome_arquivo, "w");
    if (arquivo_log == NULL) {
        perror("Erro critico ao abrir o arquivo de log");
        return -1;
    }
    // pthread_mutex_init(&log_mutex, NULL); // Inicializar o mutex
    return 0;
}

/**
 * @brief Escreve uma mensagem formatada com timestamp no arquivo de log.
 * @param mensagem A string de texto a ser escrita no log.
 */
void escrever_log(const char* mensagem) {
    if (arquivo_log == NULL) return;

    time_t agora = time(NULL);
    struct tm* tempo_info = localtime(&agora);
    char buffer_tempo[21]; // Formato: [YYYY-MM-DD HH:MM:SS]

    strftime(buffer_tempo, sizeof(buffer_tempo), "[%Y-%m-%d %H:%M:%S]", tempo_info);

    // pthread_mutex_lock(&log_mutex); // Trava para evitar escrita concorrente
    fprintf(arquivo_log, "%s %s", buffer_tempo, mensagem);
    fflush(arquivo_log); // Garante que a mensagem seja escrita imediatamente
    // pthread_mutex_unlock(&log_mutex); // Libera o mutex
}

/**
 * @brief Fecha o arquivo de log e libera os recursos.
 */
void fechar_log() {
    if (arquivo_log != NULL) {
        escrever_log("====================================================\n");
        escrever_log("INFO: Fim da simulacao. Fechando log.\n");
        escrever_log("====================================================\n");
        fclose(arquivo_log);
        arquivo_log = NULL;
        // pthread_mutex_destroy(&log_mutex);
    }
}


// --- FUNÇÕES DE CONFIGURAÇÃO (REAPROVEITADAS DO config.c) ---

void mostrar_uso(const char* nome_programa) {
    printf("\nUso: %s [opcoes]\n\n", nome_programa);
    printf("Opcoes:\n");
    printf("  --pistas <num>       Define o numero de pistas (Padrao: %d)\n", 3);
    printf("  --portoes <num>      Define o numero de portoes de embarque (Padrao: %d)\n", 5);
    printf("  --torre-ops <num>    Define a capacidade da torre de controle (Padrao: %d)\n", 2);
    printf("  --tempo <seg>        Define o tempo total da simulacao em segundos (Padrao: %d)\n", 300);
    printf("  --alerta <seg>       Define o tempo de espera para alerta critico (Padrao: %d)\n", 60);
    printf("  --falha <seg>        Define o tempo de espera para a falha (aviao 'cai') (Padrao: %d)\n", 90);
    printf("  --ajuda              Mostra esta mensagem de ajuda.\n\n");
    printf("Exemplo: %s --pistas 4 --portoes 6 --tempo 600\n\n", nome_programa);
}

int processar_argumentos(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ajuda") == 0) {
            return 1;
        } else if (strcmp(argv[i], "--pistas") == 0) {
            if (i + 1 < argc) NUM_PISTAS = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--portoes") == 0) {
            if (i + 1 < argc) NUM_PORTOES = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--torre-ops") == 0) {
            if (i + 1 < argc) NUM_OP_TORRES = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tempo") == 0) {
            if (i + 1 < argc) TEMPO_TOTAL = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--alerta") == 0) {
            if (i + 1 < argc) ALERTA_CRITICO = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--falha") == 0) {
            if (i + 1 < argc) FALHA = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Erro: Argumento desconhecido '%s'\n", argv[i]);
            return -1;
        }
    }
    return 0;
}


// --- FUNÇÃO PRINCIPAL ---

int main(int argc, char* argv[]) {
    int resultado_parse = processar_argumentos(argc, argv);
    if (resultado_parse != 0) {
        mostrar_uso(argv[0]);
        return (resultado_parse == 1) ? 0 : 1;
    }

    // Inicializa o sistema de log
    if (iniciar_log("simulacao_mike.log") != 0) {
        return 1; // Termina se não conseguir criar o arquivo de log
    }

    printf("====================================================\n");
    printf("   Simulador de Controle de Trafego Aereo\n");
    printf("====================================================\n");
    printf("Configuracao:\n");
    printf("  Torres: %d | Pistas: %d | Portoes: %d | Operacoes na Torre: %d\n", NUM_TORRES, NUM_PISTAS, NUM_PORTOES, NUM_OP_TORRES);
    printf("  Duracao: %ds | Alerta: %ds | Falha: %ds\n", TEMPO_TOTAL, ALERTA_CRITICO, FALHA);
    printf("====================================================\n\n");

    escrever_log("====================================================\n");
    escrever_log("INFO: Sistema inicializado. Iniciando simulacao...\n");
    escrever_log("====================================================\n");

    printf("Simulacao iniciada... Verifique o arquivo 'simulacao_mike.log' para detalhes.\n");

    // --- LÓGICA PRINCIPAL DA SIMULAÇÃO VIRÁ AQUI ---
    // Exemplo:
    // iniciar_aeroporto();
    // criar_threads_de_avioes();
    sleep(5); // Simula a execução por 5 segundos
    // finalizar_criacao_de_avioes();
    // aguardar_avioes_finalizarem();
    // destruir_aeroporto();
    // ------------------------------------------------

    printf("Simulacao finalizada.\n");

    // Fecha o log antes de terminar
    fechar_log();

    return 0;
}