#include "aeroporto.h"

// Função genérica para solicitar um recurso usando a fila de prioridade
int solicitar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                     aviao_t* aviao, tipo_recurso tipo, const char* nome_recurso) {
    printf("AVIÃO [%03d] solicitando %s...\n", aviao->ID, nome_recurso);
    
    if (aviao->recursos_realocados && aviao->tipo == DOMESTICO) {
        printf("AVIÃO [%03d] (doméstico realocado) tem prioridade máxima\n", aviao->ID);
    }
    
    if (adicionar_requisicao(fila, aviao, tipo) == -1) {
        return -1;
    }
    
    registrar_requisicao(aviao, tipo);
    adicionar_aviao_warning(aviao);
    
    time_t tempo_inicio_espera = time(NULL);
    
    // Aguarda até ser o próximo na fila de prioridade
    while (1) {
        pthread_mutex_lock(&fila->mutex);
        request_node_t* proximo = fila->head;
        
        if (proximo != NULL && proximo->aviao->ID == aviao->ID) {
            // É a nossa vez - libera o mutex e tenta adquirir o recurso
            pthread_mutex_unlock(&fila->mutex);
            break;
        } else {
            // Não é nossa vez - aguarda sinal de mudança na fila
            request_node_t* meu_node = fila->head;
            while (meu_node != NULL && meu_node->aviao->ID != aviao->ID) {
                meu_node = meu_node->next;
            }
            
            if (meu_node != NULL) {
                // Espera com timeout curto para poder verificar starvation
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 2; // timeout de 2 segundos
                pthread_cond_timedwait(&meu_node->cond_var, &fila->mutex, &ts);
            }
            pthread_mutex_unlock(&fila->mutex);
        }
        
        // Verifica starvation enquanto aguarda na fila
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
            
            printf("\nFALHA OPERACIONAL POR STARVATION\n");
            printf("AVIÃO [%03d] excedeu tempo limite esperando por %s (%ld segundos)\n\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
            return -1;
        }
        
        if (tempo_espera_total >= ALERTA_CRITICO && !aviao->em_alerta) {
            pthread_mutex_lock(&mutex_lista_avioes);
            aviao->em_alerta = true;
            pthread_mutex_unlock(&mutex_lista_avioes);
            
            printf("\nALERTA CRÍTICO\n");
            printf("AVIÃO [%03d] em situação crítica esperando por %s (%ld segundos)\n\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
        }
    }
    
    // Agora é nossa vez na fila - tenta adquirir o recurso
    // Usa sem_timedwait com timeout longo para permitir detecção de deadlock real
    // mas ainda verificar starvation ocasionalmente
    while (1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5; // timeout de 5 segundos
        
        if (sem_timedwait(sem_recurso, &ts) == 0) {
            // Recurso adquirido com sucesso
            remover_requisicao(fila, aviao);
            limpar_requisicao(aviao, tipo);
            registrar_alocacao(aviao, tipo);
            printf("AVIÃO [%03d] conseguiu alocar %s.\n", aviao->ID, nome_recurso);
            return 0;
        }
        
        // Verifica se excedeu tempo para starvation
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
            
            printf("\nFALHA OPERACIONAL POR STARVATION\n");
            printf("AVIÃO [%03d] excedeu tempo limite esperando por %s (%ld segundos)\n\n", 
                   aviao->ID, nome_recurso, tempo_espera_total);
            return -1;
        }
        
        printf("AVIÃO [%03d] ainda aguardando %s (timeout)... %ld segundos\n", 
               aviao->ID, nome_recurso, tempo_espera_total);
    }
}

// Função genérica para liberar um recurso
void liberar_recurso_com_prioridade(fila_prioridade_t* fila, sem_t* sem_recurso, 
                                   aviao_t* aviao, const char* nome_recurso) {
    printf("AVIÃO [%03d] liberando %s.\n", aviao->ID, nome_recurso);
    sem_post(sem_recurso);
    
    pthread_mutex_lock(&fila->mutex);
    if (fila->head != NULL) {
        pthread_cond_signal(&fila->head->cond_var);
    }
    pthread_mutex_unlock(&fila->mutex);
}

// Funções específicas para cada recurso
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

// Funções de operações complexas
int solicitar_pouso(aviao_t *aviao) {
    printf("AVIÃO [%03d] (%s) solicitando recursos para POUSO...\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
    if (aviao->tipo == DOMESTICO) {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_pista(aviao) == -1) { liberar_torre(aviao); return -1; }
    } else {
        if (solicitar_pista(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) { liberar_pista(aviao); return -1; }
    }
    printf("AVIÃO [%03d] obteve todos os recursos para POUSO (Pista + Torre).\n", aviao->ID);
    return 0;
}
void liberar_pouso(aviao_t *aviao) {
    printf("AVIÃO [%03d] liberando recursos do POUSO...\n", aviao->ID);
    liberar_pista(aviao);
    liberar_torre(aviao);
}
int solicitar_desembarque(aviao_t *aviao) {
    printf("AVIÃO [%03d] (%s) solicitando recursos para DESEMBARQUE...\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
    if (aviao->tipo == DOMESTICO) {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_portao(aviao) == -1) { liberar_torre(aviao); return -1; }
    } else {
        if (solicitar_portao(aviao) == -1) return -1;
        if (solicitar_torre(aviao) == -1) { liberar_portao(aviao); return -1; }
    }
    printf("AVIÃO [%03d] obteve todos os recursos para DESEMBARQUE (Portão + Torre).\n", aviao->ID);
    return 0;
}
void liberar_desembarque(aviao_t *aviao) {
    printf("AVIÃO [%03d] liberando recursos do DESEMBARQUE...\n", aviao->ID);
    liberar_torre(aviao);
    usleep(300000);
    liberar_portao(aviao);
}
int solicitar_decolagem(aviao_t *aviao) {
    printf("AVIÃO [%03d] (%s) solicitando recursos para DECOLAGEM...\n", 
           aviao->ID, aviao->tipo == INTERNACIONAL ? "Internacional" : "Doméstico");
    if (aviao->tipo == DOMESTICO) {
        if (solicitar_torre(aviao) == -1) return -1;
        if (solicitar_portao(aviao) == -1) { liberar_torre(aviao); return -1; }
        if (solicitar_pista(aviao) == -1) { liberar_torre(aviao); liberar_portao(aviao); return -1; }
    } else {
        if (solicitar_portao(aviao) == -1) return -1;
        if (solicitar_pista(aviao) == -1) { liberar_portao(aviao); return -1; }
        if (solicitar_torre(aviao) == -1) { liberar_pista(aviao); liberar_portao(aviao); return -1; }
    }
    printf("AVIÃO [%03d] obteve todos os recursos para DECOLAGEM (Portão + Pista + Torre).\n", aviao->ID);
    return 0;
}
void liberar_decolagem(aviao_t *aviao) {
    printf("AVIÃO [%03d] liberando todos os recursos da DECOLAGEM...\n", aviao->ID);
    liberar_portao(aviao);
    liberar_pista(aviao);
    liberar_torre(aviao);
}