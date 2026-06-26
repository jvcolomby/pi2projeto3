#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#include "simulador.h"

#define MENU_W 48
#define MENU_H 16
#define RES_W 60
#define RES_H 24
#define TOTAL_OPCOES 9

#define COR_BORDA 1
#define COR_TITULO 2
#define COR_SELECAO 3
#define COR_TEXTO 4
#define COR_INFO 5
#define COR_ALERTA 6
#define COR_BOLHA 7

typedef enum {
    MENU_CARREGAR,
    MENU_REGISTRADORES,
    MENU_ASSEMBLY,
    MENU_RUN,
    MENU_STEP,
    MENU_PIPELINE,
    MENU_ESTATISTICAS,
    MENU_MEMORIA,
    MENU_SAIR
} OpcaoMenu;

typedef enum {
    ACAO_STEP,
    ACAO_STEPBACK
} OpcaoStep;

static int tema_colorido = 0;

static int eh_enter(int tecla) {
    return tecla == '\n' || tecla == '\r' || tecla == KEY_ENTER;
}

static void ativar_tema(void) {
    if (!has_colors()) return;

    start_color();
    int fundo = use_default_colors() == OK ? -1 : COLOR_BLACK;
    init_pair(COR_BORDA, COLOR_CYAN, fundo);
    init_pair(COR_TITULO, COLOR_MAGENTA, fundo);
    init_pair(COR_SELECAO, COLOR_BLACK, COLOR_CYAN);
    init_pair(COR_TEXTO, COLOR_WHITE, fundo);
    init_pair(COR_INFO, COLOR_GREEN, fundo);
    init_pair(COR_ALERTA, COLOR_YELLOW, fundo);
    init_pair(COR_BOLHA, COLOR_RED, fundo);
    tema_colorido = 1;

    bkgd(COLOR_PAIR(COR_TEXTO));
}

static int cor(int par) {
    return tema_colorido ? COLOR_PAIR(par) : 0;
}

static void desenhar_moldura(WINDOW *janela, int largura, const char *titulo) {
    wattron(janela, cor(COR_BORDA) | A_BOLD);
    box(janela, 0, 0);
    wattroff(janela, cor(COR_BORDA) | A_BOLD);

    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 0, (largura - (int)strlen(titulo) - 2) / 2,
              " %s ", titulo);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
}

static WINDOW *criar_janela(int altura, int largura, const char *titulo) {
    int linha = (LINES - altura) / 2;
    int coluna = (COLS - largura) / 2;
    WINDOW *janela = newwin(altura, largura, linha, coluna);

    if (!janela) return NULL;

    keypad(janela, TRUE);
    wbkgd(janela, cor(COR_TEXTO));
    desenhar_moldura(janela, largura, titulo);
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
    wattron(janela, cor(COR_ALERTA) | A_DIM);
    mvwprintw(janela, RES_H - 2, 2, "Pressione ENTER para voltar...");
    wattroff(janela, cor(COR_ALERTA) | A_DIM);
    wrefresh(janela);

    int tecla;
    do {
        tecla = wgetch(janela);
    } while (!eh_enter(tecla));
}

static void mostrar_aviso(const char *titulo, const char *mensagem) {
    WINDOW *janela = criar_janela(RES_H, RES_W, titulo);
    if (!janela) return;
    wattron(janela, cor(COR_ALERTA) | A_BOLD);
    mvwprintw(janela, 3, 3, "%s", mensagem);
    wattroff(janela, cor(COR_ALERTA) | A_BOLD);
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
        "Step / Stepback",
        "Estado do pipeline",
        "Estatisticas",
        "Memoria de dados",
        "Sair"
    };

    werase(menu);
    desenhar_moldura(menu, MENU_W, "Mini MIPS Pipeline");

    for (int i = 0; i < TOTAL_OPCOES; i++) {
        int numero = i == MENU_SAIR ? 0 : i + 1;
        if (i == selecionada)
            wattron(menu, cor(COR_SELECAO) | A_BOLD);
        else
            wattron(menu, cor(COR_TEXTO));
        mvwprintw(menu, 2 + i, 3, "[%d] %-29s", numero, opcoes[i]);
        if (i == selecionada)
            wattroff(menu, cor(COR_SELECAO) | A_BOLD);
        else
            wattroff(menu, cor(COR_TEXTO));
    }

    wattron(menu, cor(COR_INFO) | A_BOLD);
    if (estado->num_instrucoes > 0)
        mvwprintw(menu, 12, 2, "%d instrucoes carregadas | ciclo %d",
                  estado->num_instrucoes, estado->ciclos);
    else
        mvwprintw(menu, 12, 2, "Nenhum programa carregado");
    wattroff(menu, cor(COR_INFO) | A_BOLD);

    wattron(menu, cor(COR_ALERTA) | A_DIM);
    mvwprintw(menu, 14, 2, "Numero/setas: selecionar | ENTER: confirmar");
    wattroff(menu, cor(COR_ALERTA) | A_DIM);
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
        else if (tecla >= '1' && tecla <= '8')
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
        desenhar_moldura(janela, RES_W, "Carregar .mem");
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, 2, 3, "Digite o nome do arquivo.");
        wattroff(janela, cor(COR_INFO));
        wattron(janela, cor(COR_TITULO) | A_BOLD);
        mvwprintw(janela, 4, 3, "Arquivo: ");
        wattroff(janela, cor(COR_TITULO) | A_BOLD);
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
    desenhar_moldura(janela, RES_W, "Carregar .mem");

    if (quantidade > 0) {
        novo_estado.num_instrucoes = quantidade;
        *estado = novo_estado;
        wattron(janela, cor(COR_INFO) | A_BOLD);
        mvwprintw(janela, 3, 3, "Arquivo carregado com sucesso.");
        wattroff(janela, cor(COR_INFO) | A_BOLD);
        mvwprintw(janela, 5, 3, "Instrucoes: %d", quantidade);
        mvwprintw(janela, 7, 3, "Arquivo: %.45s", nome);
    } else {
        wattron(janela, cor(COR_ALERTA) | A_BOLD);
        mvwprintw(janela, 3, 3, "Nao foi possivel carregar o arquivo.");
        wattroff(janela, cor(COR_ALERTA) | A_BOLD);
        mvwprintw(janela, 7, 3, "Informado: %.43s", nome);
    }

    aguardar(janela);
    fechar_janela(janela);
}

static void tela_registradores(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Registradores");
    if (!janela) return;

    for (int i = 0; i < 8; i++) {
        wattron(janela, cor(COR_INFO) | A_BOLD);
        mvwprintw(janela, 2 + i, 4, "$%d = %d", i,
                  estado->registradores[i]);
        wattroff(janela, cor(COR_INFO) | A_BOLD);
    }
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 11, 4, "PC = %d", estado->PC);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
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
        desenhar_moldura(janela, RES_W, "Assembly");

        for (int i = 0; i < maximo && i + deslocamento <
             estado->num_instrucoes; i++) {
            char texto[64];
            instrucao_para_asm(estado->mem_instrucoes[i + deslocamento],
                               texto);
            wattron(janela, cor(COR_INFO));
            mvwprintw(janela, 2 + i, 4, "[%2d] %s",
                      i + deslocamento, texto);
            wattroff(janela, cor(COR_INFO));
        }

        wattron(janela, cor(COR_ALERTA) | A_DIM);
        mvwprintw(janela, RES_H - 2, 2,
                  "Setas: rolar | ENTER ou Q: voltar");
        wattroff(janela, cor(COR_ALERTA) | A_DIM);
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
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 1, 2, "Ciclo: %d | PC: %d", estado->ciclos,
              estado->PC);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);

    wattron(janela, cor(COR_BORDA) | A_BOLD);
    mvwprintw(janela, 3, 2, "BI/DI  :");
    wattroff(janela, cor(COR_BORDA) | A_BOLD);
    if (estado->bi_di.valido) {
        instrucao_para_asm(estado->bi_di.instrucao, texto);
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, 3, 11, "%-20s PC+1=%d", texto,
                  estado->bi_di.PC_mais1);
        wattroff(janela, cor(COR_INFO));
    } else {
        wattron(janela, cor(COR_BOLHA) | A_DIM);
        mvwprintw(janela, 3, 11, "[bolha]");
        wattroff(janela, cor(COR_BOLHA) | A_DIM);
    }

    wattron(janela, cor(COR_BORDA) | A_BOLD);
    mvwprintw(janela, 5, 2, "DI/EX  :");
    wattroff(janela, cor(COR_BORDA) | A_BOLD);
    if (estado->di_ex.valido) {
        instrucao_para_asm(estado->di_ex.instrucao_raw, texto);
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, 5, 11, "%s", texto);
        mvwprintw(janela, 6, 11, "A=%d B=%d imm=%d",
                  estado->di_ex.A, estado->di_ex.B,
                  estado->di_ex.c.imm);
        wattroff(janela, cor(COR_INFO));
    } else {
        wattron(janela, cor(COR_BOLHA) | A_DIM);
        mvwprintw(janela, 5, 11, "[bolha]");
        wattroff(janela, cor(COR_BOLHA) | A_DIM);
    }

    wattron(janela, cor(COR_BORDA) | A_BOLD);
    mvwprintw(janela, 8, 2, "EX/MEM :");
    wattroff(janela, cor(COR_BORDA) | A_BOLD);
    if (estado->ex_mem.valido) {
        instrucao_para_asm(estado->ex_mem.instrucao_raw, texto);
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, 8, 11, "%s", texto);
        mvwprintw(janela, 9, 11, "ULA=%d zero=%d destino=$%d",
                  estado->ex_mem.ULAout, estado->ex_mem.zero,
                  estado->ex_mem.rd_dest);
        wattroff(janela, cor(COR_INFO));
    } else {
        wattron(janela, cor(COR_BOLHA) | A_DIM);
        mvwprintw(janela, 8, 11, "[bolha]");
        wattroff(janela, cor(COR_BOLHA) | A_DIM);
    }

    wattron(janela, cor(COR_BORDA) | A_BOLD);
    mvwprintw(janela, 11, 2, "MEM/ER :");
    wattroff(janela, cor(COR_BORDA) | A_BOLD);
    if (estado->mem_er.valido) {
        instrucao_para_asm(estado->mem_er.instrucao_raw, texto);
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, 11, 11, "%s", texto);
        mvwprintw(janela, 12, 11, "resultado=%d destino=$%d",
                  estado->mem_er.resultado, estado->mem_er.rd_dest);
        wattroff(janela, cor(COR_INFO));
    } else {
        wattron(janela, cor(COR_BOLHA) | A_DIM);
        mvwprintw(janela, 11, 11, "[bolha]");
        wattroff(janela, cor(COR_BOLHA) | A_DIM);
    }
}

static void tela_pipeline(const Estado *estado, const char *titulo) {
    WINDOW *janela = criar_janela(RES_H, RES_W, titulo);
    if (!janela) return;
    imprimir_pipeline_janela(janela, estado);
    aguardar(janela);
    fechar_janela(janela);
}

static void tela_step_stepback(Estado *estado) {
    OpcaoStep selecionada = ACAO_STEP;
    WINDOW *janela = criar_janela(RES_H, RES_W, "Step / Stepback");
    if (!janela) return;

    while (1) {
        werase(janela);
        desenhar_moldura(janela, RES_W, "Step / Stepback");
        imprimir_pipeline_janela(janela, estado);

        wattron(janela, cor(COR_TITULO) | A_BOLD);
        mvwprintw(janela, RES_H - 5, 2, "Acao:");
        wattroff(janela, cor(COR_TITULO) | A_BOLD);

        if (selecionada == ACAO_STEP)
            wattron(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattron(janela, cor(COR_TEXTO));
        mvwprintw(janela, RES_H - 5, 9, "[1] Step");
        if (selecionada == ACAO_STEP)
            wattroff(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattroff(janela, cor(COR_TEXTO));

        if (selecionada == ACAO_STEPBACK)
            wattron(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattron(janela, cor(COR_TEXTO));
        mvwprintw(janela, RES_H - 5, 22, "[2] Stepback");
        if (selecionada == ACAO_STEPBACK)
            wattroff(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattroff(janela, cor(COR_TEXTO));

        wattron(janela, cor(COR_ALERTA) | A_DIM);
        if (selecionada == ACAO_STEP && programa_terminou(estado))
            mvwprintw(janela, RES_H - 3, 2,
                      "Step indisponivel: programa terminou.");
        else if (selecionada == ACAO_STEPBACK &&
                 historico_pipeline_tamanho() == 0)
            mvwprintw(janela, RES_H - 3, 2,
                      "Stepback indisponivel: sem historico.");
        else
            mvwprintw(janela, RES_H - 3, 2,
                      "Historico: %d", historico_pipeline_tamanho());

        mvwprintw(janela, RES_H - 2, 2,
                  "Setas/1/2: escolher | ENTER: executar | Q: voltar");
        wattroff(janela, cor(COR_ALERTA) | A_DIM);
        wrefresh(janela);

        int tecla = wgetch(janela);
        if (tecla == 'q' || tecla == 'Q') break;
        if (tecla == KEY_LEFT || tecla == KEY_UP || tecla == '1') {
            selecionada = ACAO_STEP;
            continue;
        }
        if (tecla == KEY_RIGHT || tecla == KEY_DOWN || tecla == '2') {
            selecionada = ACAO_STEPBACK;
            continue;
        }
        if (eh_enter(tecla)) {
            if (selecionada == ACAO_STEP) {
                if (!programa_terminou(estado))
                    ciclo_pipeline(estado);
            } else if (historico_pipeline_tamanho() > 0) {
                stepback_pipeline(estado);
            }
        }
    }

    fechar_janela(janela);
}

static void tela_memoria_dados(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Memoria de dados");
    int deslocamento = 0;
    int linhas_visiveis = RES_H - 4;
    int colunas = 2;
    int por_pagina = linhas_visiveis * colunas;
    if (!janela) return;

    while (1) {
        werase(janela);
        desenhar_moldura(janela, RES_W, "Memoria de dados");

        for (int i = 0; i < por_pagina && i + deslocamento < 256; i++) {
            int idx = i + deslocamento;
            int col = (i / linhas_visiveis) * 28;
            int lin = i % linhas_visiveis;
            int val = estado->mem_dados[idx];

            if (val != 0)
                wattron(janela, cor(COR_INFO) | A_BOLD);
            else
                wattron(janela, cor(COR_TEXTO) | A_DIM);

            mvwprintw(janela, 2 + lin, 3 + col,
                      "mem[%3d] = %4d", idx, val);

            if (val != 0)
                wattroff(janela, cor(COR_INFO) | A_BOLD);
            else
                wattroff(janela, cor(COR_TEXTO) | A_DIM);
        }

        wattron(janela, cor(COR_ALERTA) | A_DIM);
        mvwprintw(janela, RES_H - 2, 2,
                  "Setas: rolar | ENTER ou Q: voltar");
        wattroff(janela, cor(COR_ALERTA) | A_DIM);
        wrefresh(janela);

        int tecla = wgetch(janela);
        if (eh_enter(tecla) || tecla == 'q' || tecla == 'Q') break;
        if (tecla == KEY_DOWN && deslocamento + por_pagina < 256)
            deslocamento += colunas;
        else if (tecla == KEY_UP && deslocamento > 0)
            deslocamento -= colunas;
    }

    fechar_janela(janela);
}

static void tela_run(Estado *estado) {
    if (programa_terminou(estado)) {
        mostrar_aviso("Run", "O programa ja foi executado ate o fim.");
        return;
    }

    run(estado, estado->num_instrucoes);

    WINDOW *janela = criar_janela(RES_H, RES_W, "Execucao concluida");
    if (!janela) return;
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 2, 3, "Ciclos: %d", estado->ciclos);
    mvwprintw(janela, 3, 3, "Instrucoes concluidas: %d",
              estado->instrucoes);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
    for (int i = 0; i < 8; i++) {
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, 5 + i, 5, "$%d = %d", i,
                  estado->registradores[i]);
        wattroff(janela, cor(COR_INFO));
    }
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 14, 3, "PC = %d", estado->PC);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
    aguardar(janela);
    fechar_janela(janela);
}

static void tela_estatisticas(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Estatisticas");
    if (!janela) return;

    wattron(janela, cor(COR_INFO));
    mvwprintw(janela, 2, 4, "Ciclos       : %d", estado->ciclos);
    mvwprintw(janela, 3, 4, "Instrucoes   : %d", estado->instrucoes);
    mvwprintw(janela, 5, 4, "Tipo R       : %d", estado->qtd_tipo_r);
    mvwprintw(janela, 6, 4, "addi         : %d", estado->qtd_addi);
    mvwprintw(janela, 7, 4, "lw           : %d", estado->qtd_lw);
    mvwprintw(janela, 8, 4, "sw           : %d", estado->qtd_sw);
    mvwprintw(janela, 9, 4, "beq          : %d", estado->qtd_beq);
    mvwprintw(janela, 10, 4, "jump         : %d", estado->qtd_jump);
    wattroff(janela, cor(COR_INFO));
    wattron(janela, cor(COR_ALERTA) | A_BOLD);
    mvwprintw(janela, 12, 4, "Bolhas       : %d", estado->bolhas);
    wattroff(janela, cor(COR_ALERTA) | A_BOLD);
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    if (estado->instrucoes > 0)
        mvwprintw(janela, 13, 4, "CPI          : %.2f",
                  (float)estado->ciclos / estado->instrucoes);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
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
    ativar_tema();

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
                tela_step_stepback(&estado);
                break;
            case MENU_PIPELINE:
                tela_pipeline(&estado, "Estado do pipeline");
                break;
            case MENU_ESTATISTICAS:
                tela_estatisticas(&estado);
                break;
            case MENU_MEMORIA:
                tela_memoria_dados(&estado);
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