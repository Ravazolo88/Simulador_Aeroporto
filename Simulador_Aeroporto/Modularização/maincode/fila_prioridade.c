#include "aeroporto.h"

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
    
    if (aviao->recursos_realocados && aviao->tipo == DOMESTICO) {
        novo->prioridade_atual = 50;
    } else if (aviao->tipo == DOMESTICO) {
        novo->prioridade_atual = PRIORIDADE_BASE_DOMESTICO;
    } else {
        novo->prioridade_atual = PRIORIDADE_BASE_INTERNACIONAL;
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

void atualizar_prioridades(fila_prioridade_t* fila) {
    pthread_mutex_lock(&fila->mutex);
    
    request_node_t* atual = fila->head;
    time_t agora = time(NULL);
    
    while (atual != NULL) {
        time_t tempo_espera = agora - atual->tempo_chegada;
        
        if (tempo_espera > 10) {
            atual->prioridade_atual += (tempo_espera / 5) * 2;
        }
        
        if (tempo_espera > ALERTA_CRITICO / 2) {
            atual->prioridade_atual += 10;
            log_message("[SISTEMA] Aviao [%03d] teve prioridade aumentada por tempo de espera (%lds).\n", 
                       atual->aviao->ID, tempo_espera);
        }
        
        atual = atual->next;
    }
    
    // Reordena a fila
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
    (void)arg;
    while (sistema_ativo) {
        atualizar_prioridades(&fila_pistas);
        atualizar_prioridades(&fila_portoes);
        atualizar_prioridades(&fila_torre_ops);
        sleep(1);
    }
    return NULL;
}