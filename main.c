#include <stdio.h>
#include <string.h>
#include "simulador.h"

static const char *nome_estagio[] = { "BI", "DI", "EX", "MEM", "ER" };

int main() {
    Estado e;
    inicializar_estado(&e);
    int n = 0;
    int opcao;
    int estagio_atual = 0;

    printf("\n");
    printf("         ---------------------------------------\n");
    printf("           SIMULADOR MINI MIPS 8 BITS PIPELINE\n");
    printf("         ---------------------------------------\n");

    do {
        printf("\n");
        printf(" ----------------------------------------------------\n");
        printf(" |           Menu do Simulador MiniMips             |\n");
        printf(" |--------------------------------------------------|\n");
        printf(" |                                                  |\n");
        printf(" |         [1] Carregar memoria (.mem)              |\n");
        printf(" |         [2] Imprimir registradores               |\n");
        printf(" |         [3] Mostrar Assembly                     |\n");
        printf(" |                                                  |\n");
        printf(" |         [4] Executar programa  (Run)             |\n");
        printf(" |         [5] Executar um estagio (Step)           |\n");
        printf(" |                                                  |\n");
        printf(" |         [6] Estado do pipeline                   |\n");
        printf(" |         [7] Estatisticas                         |\n");
        printf(" |         [0] Sair                                 |\n");
        printf(" |                                                  |\n");
        printf(" ----------------------------------------------------\n");
        printf("\n Escolha: ");
        scanf("%d", &opcao);
        getchar();

        switch (opcao) {
            case 1: {
                char nome[100];
                printf("Nome do arquivo .mem: ");
                scanf("%s", nome);
                getchar();
                inicializar_estado(&e);
                estagio_atual = 0;
                n = leitura_arquivo_mem(e.mem_instrucoes, nome);
                if (n > 0)
                    printf("Arquivo carregado: %s (%d instrucoes)\n", nome, n);
                break;
            }
            case 2:
                imprimir_registradores(&e);
                break;
            case 3: {
                if (n == 0) { printf("Carregue um arquivo .mem primeiro.\n"); break; }
                printf("\n=== Assembly ===\n");
                for (int i = 0; i < n; i++) {
                    char buf[64];
                    instrucao_para_asm(e.mem_instrucoes[i], buf);
                    printf("  mem[%d]: %s\n", i, buf);
                }
                break;
            }
            case 4: {
                if (n == 0) { printf("Carregue um arquivo .mem primeiro.\n"); break; }
                printf("\n--- Iniciando Execucao ---\n");
                run(&e, n);
                printf("--- Execucao concluida ---\n");
                imprimir_registradores(&e);
                break;
            }
            case 5: {
                if (n == 0) { printf("Carregue um arquivo .mem primeiro.\n"); break; }
                if (e.PC >= n && !e.bi_di.valido && !e.di_ex.valido &&
                    !e.ex_mem.valido && !e.mem_er.valido) {
                    printf("Pipeline vazio.\n");
                    break;
                }
                printf("\n--- Estagio: %s ---\n", nome_estagio[estagio_atual]);
                switch (estagio_atual) {
                    case 0: estagio_BI(&e);  break;
                    case 1: estagio_DI(&e);  break;
                    case 2: estagio_EX(&e);  break;
                    case 3: estagio_MEM(&e); break;
                    case 4: estagio_ER(&e);
                            e.ciclos++;
                            break;
                }
                imprimir_pipeline(&e);
                estagio_atual = (estagio_atual + 1) % 5;
                if (estagio_atual == 0)
                    printf("\n>>> Ciclo %d completo\n", e.ciclos);
                break;
            }
            case 6:
                imprimir_pipeline(&e);
                break;
            case 7:
                imprimir_estatisticas(&e);
                break;
            case 0:
                printf("Saindo...\n");
                break;
            default:
                printf("Opcao invalida. Tente novamente.\n");
        }
    } while (opcao != 0);

    return 0;
}
