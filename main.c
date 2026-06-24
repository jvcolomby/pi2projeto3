#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#include "simulador.h"

#define MENU_W 48
#define MENU_H 14
#define RES_W 60
#define RES_H 24
#define TOTAL_OPCOES 8

typedef enum {
    MENU_CARREGAR,
    MENU_REGISTRADORES,
    MENU_ASSEMBLY,
    MENU_RUN,
    MENU_STEP,
    MENU_PIPELINE,
    MENU_ESTATISTICAS,
    MENU_SAIR
} OpcaoMenu;

static int eh_enter(int tecla) {
    return tecla == '\n' || tecla == '\r' || tecla == KEY_ENTER;
}

static WINDOW *criar_janela(int altura, int largura, const char *titulo) {
    int linha = (LINES - altura) / 2;
    int coluna = (COLS - largura) / 2;
    WINDOW *janela = newwin(altura, largura, linha, coluna);

    if (!janela) return NULL;

    keypad(janela, TRUE);
    box(janela, 0, 0);
    mvwprintw(janela, 0, (largura - (int)strlen(titulo) - 2) / 2,
              " %s ", titulo);
    return janela;
}

static void fechar_janela(WINDOW *janela) {
    if (!janela) return;
    werase(janela);
    wrefresh(janela);
    delwin(janela);
    touchwin(stdscr);
    refresh();
}

static void aguardar(WINDOW *janela) {
    wattron(janela, A_DIM);
    mvwprintw(janela, RES_H - 2, 2, "Pressione ENTER para voltar...");
    wattroff(janela, A_DIM);
    wrefresh(janela);

    int tecla;
    do {
        tecla = wgetch(janela);
    } while (!eh_enter(tecla));
}

static void mostrar_aviso(const char *titulo, const char *mensagem) {
    WINDOW *janela = criar_janela(RES_H, RES_W, titulo);
    if (!janela) return;
    mvwprintw(janela, 3, 3, "%s", mensagem);
    aguardar(janela);
    fechar_janela(janela);
}

static int pipeline_vazio(const Estado *estado) {
    return !estado->bi_di.valido && !estado->di_ex.valido &&
           !estado->ex_mem.valido && !estado->mem_er.valido;
}

static int programa_terminou(const Estado *estado) {
    int pc_fora = estado->PC < 0 ||
                  estado->PC >= estado->num_instrucoes;
    return pc_fora && pipeline_vazio(estado);
}

static void desenhar_menu(WINDOW *menu, int selecionada, const Estado *estado) {
    static const char *opcoes[TOTAL_OPCOES] = {
        "Carregar .mem",
        "Registradores",
        "Assembly",
        "Run",
        "Step (1 ciclo)",
        "Estado do pipeline",
        "Estatisticas",
        "Sair"
    };

    werase(menu);
    box(menu, 0, 0);
    mvwprintw(menu, 0, 13, " Mini MIPS Pipeline ");

    for (int i = 0; i < TOTAL_OPCOES; i++) {
        int numero = i == MENU_SAIR ? 0 : i + 1;
        if (i == selecionada) wattron(menu, A_REVERSE);
        mvwprintw(menu, 2 + i, 3, "[%d] %-29s", numero, opcoes[i]);
        if (i == selecionada) wattroff(menu, A_REVERSE);
    }

    if (estado->num_instrucoes > 0)
        mvwprintw(menu, 11, 2, "%d instrucoes carregadas | ciclo %d",
                  estado->num_instrucoes, estado->ciclos);
    else
        mvwprintw(menu, 11, 2, "Nenhum programa carregado");

    wattron(menu, A_DIM);
    mvwprintw(menu, 12, 2, "Numero/setas: selecionar | ENTER: confirmar");
    wattroff(menu, A_DIM);
    wrefresh(menu);
}

static OpcaoMenu ler_opcao(WINDOW *menu, Estado *estado) {
    int selecionada = MENU_CARREGAR;

    while (1) {
        desenhar_menu(menu, selecionada, estado);
        int tecla = wgetch(menu);

        if (tecla == KEY_UP && selecionada > 0)
            selecionada--;
        else if (tecla == KEY_DOWN && selecionada < TOTAL_OPCOES - 1)
            selecionada++;
        else if (tecla >= '1' && tecla <= '7')
            selecionada = tecla - '1';
        else if (tecla == '0')
            selecionada = MENU_SAIR;
        else if (eh_enter(tecla))
            return (OpcaoMenu)selecionada;
    }
}

static void remover_espacos_externos(char *texto) {
    char *inicio = texto;
    while (*inicio == ' ' || *inicio == '\t') inicio++;
    if (inicio != texto) memmove(texto, inicio, strlen(inicio) + 1);

    size_t tamanho = strlen(texto);
    while (tamanho > 0 &&
           (texto[tamanho - 1] == ' ' || texto[tamanho - 1] == '\t'))
        texto[--tamanho] = '\0';
}

static void tela_carregar(Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Carregar .mem");
    char nome[256];
    if (!janela) return;

    while (1) {
        werase(janela);
        box(janela, 0, 0);
        mvwprintw(janela, 0, 22, " Carregar .mem ");
        mvwprintw(janela, 2, 3, "Digite o nome do arquivo. Exemplo: prog.mem");
        mvwprintw(janela, 4, 3, "Arquivo: ");
        wmove(janela, 4, 12);
        wrefresh(janela);

        echo();
        curs_set(1);
        int resultado = wgetnstr(janela, nome, (int)sizeof(nome) - 1);
        curs_set(0);
        noecho();

        if (resultado == ERR) {
            fechar_janela(janela);
            return;
        }

        remover_espacos_externos(nome);
        if (nome[0] != '\0') break;

        mvwprintw(janela, 6, 3, "O nome nao pode ficar vazio.");
        mvwprintw(janela, 8, 3, "Digite o nome e pressione ENTER.");
        wrefresh(janela);
    }

    Estado novo_estado;
    inicializar_estado(&novo_estado);
    int quantidade = leitura_arquivo_mem(novo_estado.mem_instrucoes, nome);

    werase(janela);
    box(janela, 0, 0);
    mvwprintw(janela, 0, 22, " Carregar .mem ");

    if (quantidade > 0) {
        novo_estado.num_instrucoes = quantidade;
        *estado = novo_estado;
        mvwprintw(janela, 3, 3, "Arquivo carregado com sucesso.");
        mvwprintw(janela, 5, 3, "Instrucoes: %d", quantidade);
        mvwprintw(janela, 7, 3, "Arquivo: %.45s", nome);
    } else {
        mvwprintw(janela, 3, 3, "Nao foi possivel carregar o arquivo.");
        mvwprintw(janela, 5, 3, "Use prog.mem se ele estiver nesta pasta.");
        mvwprintw(janela, 7, 3, "Informado: %.43s", nome);
    }

    aguardar(janela);
    fechar_janela(janela);
}

static void tela_registradores(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Registradores");
    if (!janela) return;

    for (int i = 0; i < 8; i++)
        mvwprintw(janela, 2 + i, 4, "$%d = %d", i,
                  estado->registradores[i]);
    mvwprintw(janela, 11, 4, "PC = %d", estado->PC);
    aguardar(janela);
    fechar_janela(janela);
}

static void tela_assembly(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Assembly");
    int deslocamento = 0;
    int maximo = RES_H - 4;
    if (!janela) return;

    while (1) {
        werase(janela);
        box(janela, 0, 0);
        mvwprintw(janela, 0, 24, " Assembly ");

        for (int i = 0; i < maximo && i + deslocamento <
             estado->num_instrucoes; i++) {
            char texto[64];
            instrucao_para_asm(estado->mem_instrucoes[i + deslocamento],
                               texto);
            mvwprintw(janela, 2 + i, 4, "[%2d] %s",
                      i + deslocamento, texto);
        }

        wattron(janela, A_DIM);
        mvwprintw(janela, RES_H - 2, 2,
                  "Setas: rolar | ENTER ou Q: voltar");
        wattroff(janela, A_DIM);
        wrefresh(janela);

        int tecla = wgetch(janela);
        if (eh_enter(tecla) || tecla == 'q' || tecla == 'Q') break;
        if (tecla == KEY_DOWN &&
            deslocamento + maximo < estado->num_instrucoes)
            deslocamento++;
        else if (tecla == KEY_UP && deslocamento > 0)
            deslocamento--;
    }

    fechar_janela(janela);
}

static void imprimir_pipeline_janela(WINDOW *janela, const Estado *estado) {
    char texto[64];
    mvwprintw(janela, 1, 2, "Ciclo: %d | PC: %d", estado->ciclos,
              estado->PC);

    mvwprintw(janela, 3, 2, "BI/DI  :");
    if (estado->bi_di.valido) {
        instrucao_para_asm(estado->bi_di.instrucao, texto);
        mvwprintw(janela, 3, 11, "%-20s PC+1=%d", texto,
                  estado->bi_di.PC_mais1);
    } else {
        mvwprintw(janela, 3, 11, "[bolha]");
    }

    mvwprintw(janela, 5, 2, "DI/EX  :");
    if (estado->di_ex.valido) {
        instrucao_para_asm(estado->di_ex.instrucao_raw, texto);
        mvwprintw(janela, 5, 11, "%s", texto);
        mvwprintw(janela, 6, 11, "A=%d B=%d imm=%d",
                  estado->di_ex.A, estado->di_ex.B,
                  estado->di_ex.c.imm);
    } else {
        mvwprintw(janela, 5, 11, "[bolha]");
    }

    mvwprintw(janela, 8, 2, "EX/MEM :");
    if (estado->ex_mem.valido)
        mvwprintw(janela, 8, 11, "ULA=%d zero=%d destino=$%d",
                  estado->ex_mem.ULAout, estado->ex_mem.zero,
                  estado->ex_mem.rd_dest);
    else
        mvwprintw(janela, 8, 11, "[bolha]");

    mvwprintw(janela, 10, 2, "MEM/ER :");
    if (estado->mem_er.valido)
        mvwprintw(janela, 10, 11, "resultado=%d destino=$%d",
                  estado->mem_er.resultado, estado->mem_er.rd_dest);
    else
        mvwprintw(janela, 10, 11, "[bolha]");
}

static void tela_pipeline(const Estado *estado, const char *titulo) {
    WINDOW *janela = criar_janela(RES_H, RES_W, titulo);
    if (!janela) return;
    imprimir_pipeline_janela(janela, estado);
    aguardar(janela);
    fechar_janela(janela);
}

static void tela_step(Estado *estado) {
    if (programa_terminou(estado)) {
        mostrar_aviso("Step", "O programa terminou e o pipeline esta vazio.");
        return;
    }

    ciclo_pipeline(estado);
    tela_pipeline(estado, "Step - ciclo concluido");
}

static void tela_run(Estado *estado) {
    if (programa_terminou(estado)) {
        mostrar_aviso("Run", "O programa ja foi executado ate o fim.");
        return;
    }

    run(estado, estado->num_instrucoes);

    WINDOW *janela = criar_janela(RES_H, RES_W, "Execucao concluida");
    if (!janela) return;
    mvwprintw(janela, 2, 3, "Ciclos: %d", estado->ciclos);
    mvwprintw(janela, 3, 3, "Instrucoes concluidas: %d",
              estado->instrucoes);
    for (int i = 0; i < 8; i++)
        mvwprintw(janela, 5 + i, 5, "$%d = %d", i,
                  estado->registradores[i]);
    mvwprintw(janela, 14, 3, "PC = %d", estado->PC);
    aguardar(janela);
    fechar_janela(janela);
}

static void tela_estatisticas(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Estatisticas");
    if (!janela) return;

    mvwprintw(janela, 2, 4, "Ciclos       : %d", estado->ciclos);
    mvwprintw(janela, 3, 4, "Instrucoes   : %d", estado->instrucoes);
    mvwprintw(janela, 5, 4, "Tipo R       : %d", estado->qtd_tipo_r);
    mvwprintw(janela, 6, 4, "addi         : %d", estado->qtd_addi);
    mvwprintw(janela, 7, 4, "lw           : %d", estado->qtd_lw);
    mvwprintw(janela, 8, 4, "sw           : %d", estado->qtd_sw);
    mvwprintw(janela, 9, 4, "beq          : %d", estado->qtd_beq);
    mvwprintw(janela, 10, 4, "jump         : %d", estado->qtd_jump);
    mvwprintw(janela, 12, 4, "Bolhas       : %d", estado->bolhas);
    if (estado->instrucoes > 0)
        mvwprintw(janela, 13, 4, "CPI          : %.2f",
                  (float)estado->ciclos / estado->instrucoes);
    aguardar(janela);
    fechar_janela(janela);
}

int main(void) {
    Estado estado;
    inicializar_estado(&estado);

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    if (LINES < RES_H || COLS < RES_W) {
        endwin();
        fprintf(stderr, "O terminal precisa ter pelo menos %dx%d.\n",
                RES_W, RES_H);
        return 1;
    }

    WINDOW *menu = criar_janela(MENU_H, MENU_W, "Mini MIPS Pipeline");
    if (!menu) {
        endwin();
        fprintf(stderr, "Nao foi possivel criar o menu.\n");
        return 1;
    }

    while (1) {
        OpcaoMenu opcao = ler_opcao(menu, &estado);

        if (opcao == MENU_SAIR) break;

        if (opcao == MENU_CARREGAR) {
            tela_carregar(&estado);
            continue;
        }

        if (estado.num_instrucoes == 0) {
            mostrar_aviso("Aviso", "Carregue prog.mem antes de continuar.");
            continue;
        }

        switch (opcao) {
            case MENU_REGISTRADORES:
                tela_registradores(&estado);
                break;
            case MENU_ASSEMBLY:
                tela_assembly(&estado);
                break;
            case MENU_RUN:
                tela_run(&estado);
                break;
            case MENU_STEP:
                tela_step(&estado);
                break;
            case MENU_PIPELINE:
                tela_pipeline(&estado, "Estado do pipeline");
                break;
            case MENU_ESTATISTICAS:
                tela_estatisticas(&estado);
                break;
            case MENU_CARREGAR:
            case MENU_SAIR:
                break;
        }
    }

    delwin(menu);
    endwin();
    return 0;
}
