#include "aeroporto.h"

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

void* thread_detectar_deadlock(void* arg) {
    while (sistema_ativo) {
        if (detectar_ciclo_deadlock()) {
            pthread_mutex_lock(&mutex_contadores);
            contador_deadlocks++;
            pthread_mutex_unlock(&mutex_contadores);
            
            log_message("[DEADLOCK] Possivel deadlock detectado. Iniciando verificacao.\n");
            
            pthread_mutex_lock(&detector.mutex);
            for (int i = 0; i < MAX_AVIOES; i++) {
                bool tem_recursos = false, quer_recursos = false;
                for (int j = 0; j < 3; j++) {
                    if (detector.matriz_alocacao[i][j] > 0) tem_recursos = true;
                    if (detector.matriz_requisicao[i][j] > 0) quer_recursos = true;
                }
                if (tem_recursos && quer_recursos) {
                    pthread_mutex_lock(&mutex_warnings);
                    for (int k = 0; k < num_avioes_warnings; k++) {
                        if (avioes_com_warnings[k] && avioes_com_warnings[k]->ID == i + 1) {
                            avioes_com_warnings[k]->deadlock_warnings++;
                            if (avioes_com_warnings[k]->deadlock_warnings >= MAX_DEADLOCK_WARNINGS) {
                                log_message("[DEADLOCK] Aviao [%03d] atingiu o limite de %d avisos.\n", 
                                       avioes_com_warnings[k]->ID, MAX_DEADLOCK_WARNINGS);
                            }
                            break;
                        }
                    }
                    pthread_mutex_unlock(&mutex_warnings);
                }
            }
            pthread_mutex_unlock(&detector.mutex);
            
            realocar_recursos_avioes_warning();
        }
        sleep(5);
    }
    return NULL;
}

void adicionar_aviao_warning(aviao_t* aviao) {
    pthread_mutex_lock(&mutex_warnings);
    bool ja_existe = false;
    for (int i = 0; i < num_avioes_warnings; i++) {
        if (avioes_com_warnings[i] && avioes_com_warnings[i]->ID == aviao->ID) {
            ja_existe = true;
            break;
        }
    }
    if (!ja_existe && num_avioes_warnings < MAX_AVIOES) {
        avioes_com_warnings[num_avioes_warnings++] = aviao;
    }
    pthread_mutex_unlock(&mutex_warnings);
}

bool aviao_tem_muitos_warnings(aviao_t* aviao) {
    return aviao->deadlock_warnings >= MAX_DEADLOCK_WARNINGS;
}

void realocar_recursos_avioes_warning() {
    pthread_mutex_lock(&mutex_warnings);
    
    for (int i = 0; i < num_avioes_warnings; i++) {
        aviao_t* aviao = avioes_com_warnings[i];
        if (aviao && aviao_tem_muitos_warnings(aviao) && !aviao->recursos_realocados) {
            log_message("[DEADLOCK] Realocando recursos do Aviao [%03d] para resolver o impasse.\n", aviao->ID);
            
            pthread_mutex_lock(&detector.mutex);
            for (int j = 0; j < 3; j++) {
                if (detector.matriz_alocacao[aviao->ID - 1][j] > 0) {
                    detector.matriz_alocacao[aviao->ID - 1][j] = 0;
                    detector.recursos_disponiveis[j]++;
                    aviao->recursos_alocados[j] = 0;
                    
                    if (j == 0) sem_post(&sem_pistas);
                    else if (j == 1) sem_post(&sem_portoes);
                    else if (j == 2) sem_post(&sem_torre_ops);
                }
            }
            pthread_mutex_unlock(&detector.mutex);
            
            aviao->recursos_realocados = true;
            pthread_mutex_lock(&mutex_contadores);
            recursos_realocados++;
            pthread_mutex_unlock(&mutex_contadores);
            
            avioes_com_warnings[i] = NULL;
        }
    }
    
    int nova_pos = 0;
    for (int i = 0; i < num_avioes_warnings; i++) {
        if (avioes_com_warnings[i] != NULL) {
            avioes_com_warnings[nova_pos++] = avioes_com_warnings[i];
        }
    }
    num_avioes_warnings = nova_pos;
    
    pthread_mutex_unlock(&mutex_warnings);
}