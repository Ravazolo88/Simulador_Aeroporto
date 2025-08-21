#include "aeroporto.h"

void *rotina_aviao(void *arg) {
    aviao_t *aviao = (aviao_t *)arg;

    // --------------------------------- POUSO ---------------------------------
    log_message("[AVIAO %03d] Iniciando procedimento de pouso.\n", aviao->ID);
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = POUSANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);

    if (solicitar_pouso(aviao) == -1) {
        log_message("[AVIAO %03d] Falha ao obter recursos para pouso. Abortando.\n", aviao->ID);
        pthread_exit(NULL);
    }
    log_message("[AVIAO %03d] Pouso em andamento (duracao: 2s).\n", aviao->ID);
    sleep(2);
    liberar_pouso(aviao);
    log_message("[AVIAO %03d] Pouso concluido. Recursos liberados.\n", aviao->ID);

    // ------------------------------- DESEMBARQUE -------------------------------
    log_message("[AVIAO %03d] Iniciando procedimento de desembarque.\n", aviao->ID);
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = DESEMBARCANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);
    
    if (solicitar_desembarque(aviao) == -1) {
        log_message("[AVIAO %03d] Falha ao obter recursos para desembarque. Abortando.\n", aviao->ID);
        pthread_exit(NULL);
    }
    log_message("[AVIAO %03d] Desembarque de passageiros em andamento (duracao: 3s).\n", aviao->ID);
    sleep(3);
    liberar_desembarque(aviao);
    log_message("[AVIAO %03d] Desembarque concluido. Recursos liberados.\n", aviao->ID);

    // -------------------------------- DECOLAGEM --------------------------------
    log_message("[AVIAO %03d] Iniciando procedimento de decolagem.\n", aviao->ID);
    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = DECOLANDO;
    pthread_mutex_unlock(&mutex_lista_avioes);
    
    if (solicitar_decolagem(aviao) == -1) {
        log_message("[AVIAO %03d] Falha ao obter recursos para decolagem. Abortando.\n", aviao->ID);
        pthread_exit(NULL);
    }
    log_message("[AVIAO %03d] Decolagem em andamento (duracao: 2s).\n", aviao->ID);
    sleep(2);
    liberar_decolagem(aviao);
    log_message("[AVIAO %03d] Decolagem concluida. Recursos liberados.\n", aviao->ID);

    pthread_mutex_lock(&mutex_lista_avioes);
    aviao->estado = CONCLUIDO;
    pthread_mutex_unlock(&mutex_lista_avioes);

    log_message("[AVIAO %03d] Todas as operacoes foram concluidas com sucesso.\n", aviao->ID);
    
    pthread_exit(NULL);
}