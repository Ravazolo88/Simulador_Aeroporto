#include "aeroporto.h"

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
        
        if (aviao->tipo == INTERNACIONAL) internacionais++; else domesticos++;
        
        switch (aviao->estado) {
            case CONCLUIDO:
                estado_str = "SUCESSO              ";
                sucessos++;
                if (aviao->tipo == INTERNACIONAL) internacionais_sucesso++; else domesticos_sucesso++;
                break;
            case FALHA_OPERACIONAL:
                estado_str = "FALHA OPERACIONAL    ";
                falhas++;
                if (aviao->tipo == INTERNACIONAL) internacionais_falha++; else domesticos_falha++;
                break;
            default:
                estado_str = "INTERROMPIDO         ";
                falhas++;
                if (aviao->tipo == INTERNACIONAL) internacionais_falha++; else domesticos_falha++;
                break;
        }
        
        time_t tempo_vida = tempo_atual - aviao->tempo_de_criacao;
        
        printf("| %03d | %s | %s | %8ld          | %s         |\n",
               aviao->ID, tipo_str, estado_str, tempo_vida, aviao->em_alerta ? "SIM" : "NÃO");
    }
    
    printf("═══════════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("📈 ESTATÍSTICAS GERAIS:\n");
    printf("Total de Aviões: %d | Sucessos: %d (%.1f%%) | Falhas: %d (%.1f%%)\n\n", total_avioes,
           sucessos, total_avioes > 0 ? (float)sucessos * 100 / total_avioes : 0,
           falhas, total_avioes > 0 ? (float)falhas * 100 / total_avioes : 0);

    printf("✈️  INTERNACIONAIS: Total: %d | Sucessos: %d (%.1f%%) | Falhas: %d (%.1f%%)\n",
           internacionais, internacionais_sucesso, internacionais > 0 ? (float)internacionais_sucesso * 100 / internacionais : 0,
           internacionais_falha, internacionais > 0 ? (float)internacionais_falha * 100 / internacionais : 0);
           
    printf("🏠 DOMÉSTICOS:      Total: %d | Sucessos: %d (%.1f%%) | Falhas: %d (%.1f%%)\n\n",
           domesticos, domesticos_sucesso, domesticos > 0 ? (float)domesticos_sucesso * 100 / domesticos : 0,
           domesticos_falha, domesticos > 0 ? (float)domesticos_falha * 100 / domesticos : 0);

    printf("⚠️  PROBLEMAS: Deadlocks: %d | Starvation: %d | Recursos Realocados: %d\n\n",
           contador_deadlocks, contador_starvation, recursos_realocados);
    
    printf("🔧 CONFIGURAÇÃO: Pistas: %d | Portões: %d | Ops. Torre: %d | Tempo: %ds\n",
           NUM_PISTAS, NUM_PORTOES, NUM_OP_TORRES, TEMPO_TOTAL);

    printf("\n████████████████████████████████████████████████████████████████████████████████\n");
    printf("██                          FIM DA SIMULAÇÃO                               ██\n");
    printf("████████████████████████████████████████████████████████████████████████████████\n");
}
