#include "aeroporto.h"

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
    pthread_mutex_init(&mutex_warnings, NULL);
    memset(avioes_com_warnings, 0, sizeof(avioes_com_warnings));

    inicializar_fila(&fila_pistas);
    inicializar_fila(&fila_portoes);
    inicializar_fila(&fila_torre_ops);
    inicializar_detector_deadlock();

    pthread_t thread_aging;
    pthread_t thread_detector_deadlock;
    pthread_create(&thread_aging, NULL, thread_aging_func, NULL);
    pthread_create(&thread_detector_deadlock, NULL, thread_detectar_deadlock, NULL);

    aviao_t* avioes[MAX_AVIOES];
    int contador_avioes = 0;
    bool limite_atingido = false;

    srand(time(NULL));
    time_t inicio_simulacao = time(NULL);

    printf("\n--- SIMULAÇÃO INICIADA ---\n\n");

    while (time(NULL) - inicio_simulacao < TEMPO_TOTAL && !limite_atingido) {
        if (contador_avioes < MAX_AVIOES) {
            avioes[contador_avioes] = malloc(sizeof(aviao_t));
            if (avioes[contador_avioes] == NULL) {
                perror("Falha ao alocar memória para o avião");
                continue;
            }

            avioes[contador_avioes]->ID = contador_avioes + 1;
            avioes[contador_avioes]->tipo = (rand() % 2 == 0) ? INTERNACIONAL : DOMESTICO;
            avioes[contador_avioes]->em_alerta = false;
            avioes[contador_avioes]->tempo_de_criacao = time(NULL);
            avioes[contador_avioes]->estado = VOANDO;
            avioes[contador_avioes]->deadlock_warnings = 0;
            avioes[contador_avioes]->recursos_realocados = false;
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
        sleep(rand() % 3 + 1);
    }

    sistema_ativo = false;

    if (!limite_atingido)
        printf("\n⏰ TEMPO ESGOTADO! Não serão criados mais aviões. Aguardando os existentes finalizarem...\n");
    else
        printf("\n📊 LIMITE DE AVIÕES ATINGIDO! Aguardando os existentes finalizarem...\n");

    for (int i = 0; i < contador_avioes; i++) {
        pthread_join(avioes[i]->thread_id, NULL);
    }

    pthread_cancel(thread_aging);
    pthread_cancel(thread_detector_deadlock);
    pthread_join(thread_aging, NULL);
    pthread_join(thread_detector_deadlock, NULL);

    printf("\n✅ SIMULAÇÃO FINALIZADA! Todos os aviões concluíram suas operações.\n");

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
    pthread_mutex_destroy(&mutex_warnings);
    pthread_mutex_destroy(&detector.mutex);

    return 0;
}