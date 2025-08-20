#include "aeroporto.h"

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