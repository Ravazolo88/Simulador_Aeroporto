#include "aeroporto.h"

int solicitar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, aviao_t* aviao, tipo_recurso tipo, const char* nome_recurso) {
    log_message("[RECURSO] Aviao [%03d] solicitou %s.\n", aviao->ID, nome_recurso);
    
    if (aviao->recursos_realocados && aviao->tipo == DOMESTICO) {
        log_message("[SISTEMA] Aviao [%03d] (domestico realocado) tem prioridade maxima.\n", aviao->ID);
    }
    
    if (adicionar_requisicao(fila, aviao, tipo) == -1) {
        return -1;
    }
    
    registrar_requisicao(aviao, tipo);
    adicionar_aviao_warning(aviao);
    
    time_t tempo_inicio_espera = time(NULL);
    
    while (1) {
        pthread_mutex_lock(&fila->mutex);
        request_node_t* proximo = fila->head;
        
        if (proximo != NULL && proximo->aviao->ID == aviao->ID) {
            pthread_mutex_unlock(&fila->mutex);
            break;
        } else {
            request_node_t* meu_node = fila->head;
            while (meu_node != NULL && meu_node->aviao->ID != aviao->ID) {
                meu_node = meu_node->next;
            }
            
            if (meu_node != NULL) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 2;
                pthread_cond_timedwait(&meu_node->cond_var, &fila->mutex, &ts);
            }
            pthread_mutex_unlock(&fila->mutex);
        }
        
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
            
            log_message("[ALERTA] FALHA OPERACIONAL POR STARVATION: Aviao [%03d] excedeu tempo limite esperando por %s (%lds).\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
            return -1;
        }
        
        if (tempo_espera_total >= ALERTA_CRITICO && !aviao->em_alerta) {
            pthread_mutex_lock(&mutex_lista_avioes);
            aviao->em_alerta = true;
            pthread_mutex_unlock(&mutex_lista_avioes);
            
            log_message("[ALERTA] Aviao [%03d] em situacao critica esperando por %s (tempo: %lds).\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
        }
    }
    
    while (1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5;
        
        if (sem_timedwait(sem_recurso, &ts) == 0) {
            remover_requisicao(fila, aviao);
            limpar_requisicao(aviao, tipo);
            registrar_alocacao(aviao, tipo);
            log_message("[RECURSO] Aviao [%03d] alocou %s com sucesso.\n", aviao->ID, nome_recurso);
            return 0;
        }
        
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
            
            log_message("[ALERTA] FALHA OPERACIONAL POR STARVATION: Aviao [%03d] excedeu tempo limite esperando por %s (%lds).\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
            return -1;
        }
    }
}

void liberar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, aviao_t* aviao, const char* nome_recurso) {
    log_message("[RECURSO] Aviao [%03d] liberou %s.\n", aviao->ID, nome_recurso);
    sem_post(sem_recurso);
    
    pthread_mutex_lock(&fila->mutex);
    if (fila->head != NULL) {
        pthread_cond_signal(&fila->head->cond_var);
    }
    pthread_mutex_unlock(&fila->mutex);
}

int solicitar_pista(aviao_t *aviao) {
    return solicitar_recurso_com_prioridade(&fila_pistas, &sem_pistas, aviao, RECURSO_PISTA, "PISTA");
}
void liberar_pista(aviao_t *aviao) {
    registrar_liberacao(aviao, RECURSO_PISTA);
    liberar_recurso_com_prioridade(&fila_pistas, &sem_pistas, aviao, "PISTA");
}
int solicitar_portao(aviao_t *aviao) {
    return solicitar_recurso_com_prioridade(&fila_portoes, &sem_portoes, aviao, RECURSO_PORTAO, "PORTAO");
}
void liberar_portao(aviao_t *aviao) {
    registrar_liberacao(aviao, RECURSO_PORTAO);
    liberar_recurso_com_prioridade(&fila_portoes, &sem_portoes, aviao, "PORTAO");
}
int solicitar_torre(aviao_t *aviao) {
    return solicitar_recurso_com_prioridade(&fila_torre_ops, &sem_torre_ops, aviao, RECURSO_TORRE, "TORRE DE CONTROLE");
}
void liberar_torre(aviao_t *aviao) {
    registrar_liberacao(aviao, RECURSO_TORRE);
    liberar_recurso_com_prioridade(&fila_torre_ops, &sem_torre_ops, aviao, "TORRE DE CONTROLE");
}

// Funções de operações complexas
int solicitar_pouso(aviao_t *aviao) {
    if (aviao->tipo == DOMESTICO) {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_pista(aviao) == -1) { liberar_torre(aviao); return -1; }
    } else {
        if (solicitar_pista(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) { liberar_pista(aviao); return -1; }
    }
    log_message("[AVIAO %03d] Obteve todos os recursos para POUSO (Pista + Torre).\n", aviao->ID);
    return 0;
}
void liberar_pouso(aviao_t *aviao) {
    liberar_pista(aviao);
    liberar_torre(aviao);
}
int solicitar_desembarque(aviao_t *aviao) {
    if (aviao->tipo == DOMESTICO) {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_portao(aviao) == -1) { liberar_torre(aviao); return -1; }
    } else {
        if (solicitar_portao(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) { liberar_portao(aviao); return -1; }
    }
    log_message("[AVIAO %03d] Obteve todos os recursos para DESEMBARQUE (Portao + Torre).\n", aviao->ID);
    return 0;
}
void liberar_desembarque(aviao_t *aviao) {
    liberar_torre(aviao);
    sleep(2);
    liberar_portao(aviao);
}
int solicitar_decolagem(aviao_t *aviao) {
    if (aviao->tipo == DOMESTICO) {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_portao(aviao) == -1) { liberar_torre(aviao); return -1; }
        if (solicitar_pista(aviao) == -1) { liberar_torre(aviao); liberar_portao(aviao); return -1; }
    } else {
        if (solicitar_portao(aviao) == -1) return -1;
        if (solicitar_pista(aviao) == -1) { liberar_portao(aviao); return -1; }
        if (solicitar_torre(aviao) == -1) { liberar_pista(aviao); liberar_portao(aviao); return -1; }
    }
    log_message("[AVIAO %03d] Obteve todos os recursos para DECOLAGEM (Portao + Pista + Torre).\n", aviao->ID);
    return 0;
}
void liberar_decolagem(aviao_t *aviao) {
    liberar_portao(aviao);
    liberar_pista(aviao);
    liberar_torre(aviao);
}