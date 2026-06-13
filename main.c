#include <stdio.h>
#include <string.h>
#include "simulador.h"

int main() {
    Estado e;
    inicializar_estado(&e);
    int n = 0;
    int opcao;

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
        printf(" |         [5] Executar um ciclo  (Step)            |\n");
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
                n = leitura_arquivo_mem(e.memoria, nome);
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
                    instrucao_para_asm(e.memoria[i], buf);
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
                if (e.PC < n || e.bi_di.valido || e.di_ex.valido ||
                    e.ex_mem.valido || e.mem_er.valido) {
                    ciclo_pipeline(&e);
                    imprimir_pipeline(&e);
                } else {
                    printf("Pipeline vazio.\n");
                }
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