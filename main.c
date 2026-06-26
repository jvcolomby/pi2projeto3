#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#include "simulador.h"

#define MIN_W 68
#define MIN_H 24
#define MENU_H 7
#define RES_W painel_largura
#define RES_H painel_altura
#define TOTAL_OPCOES 9

#define COR_BORDA 1
#define COR_TITULO 2
#define COR_SELECAO 3
#define COR_TEXTO 4
#define COR_INFO 5
#define COR_ALERTA 6
#define COR_BOLHA 7
#define COR_MUTED 8
#define COR_SUCESSO 9

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
static int painel_linha = MENU_H;
static int painel_coluna = 0;
static int painel_largura = 0;
static int painel_altura = 0;

static int eh_enter(int tecla) {
    return tecla == '\n' || tecla == '\r' || tecla == KEY_ENTER;
}

static void ativar_tema(void) {
    if (!has_colors()) return;

    start_color();
    int fundo = use_default_colors() == OK ? -1 : COLOR_BLACK;
    init_pair(COR_BORDA, COLOR_BLUE, fundo);
    init_pair(COR_TITULO, COLOR_CYAN, fundo);
    init_pair(COR_SELECAO, COLOR_BLACK, COLOR_CYAN);
    init_pair(COR_TEXTO, COLOR_WHITE, fundo);
    init_pair(COR_INFO, COLOR_GREEN, fundo);
    init_pair(COR_ALERTA, COLOR_YELLOW, fundo);
    init_pair(COR_BOLHA, COLOR_MAGENTA, fundo);
    init_pair(COR_MUTED, COLOR_BLUE, fundo);
    init_pair(COR_SUCESSO, COLOR_BLACK, COLOR_GREEN);
    tema_colorido = 1;

    bkgd(COLOR_PAIR(COR_TEXTO));
}

static int cor(int par) {
    return tema_colorido ? COLOR_PAIR(par) : 0;
}

static void preencher_linha(WINDOW *janela, int y, int x, int largura,
                            chtype caractere) {
    for (int i = 0; i < largura; i++)
        mvwaddch(janela, y, x + i, caractere);
}

static void desenhar_moldura(WINDOW *janela, int largura, const char *titulo) {
    wattron(janela, cor(COR_BORDA) | A_BOLD);
    wborder(janela, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    mvwaddch(janela, 2, 0, ACS_LTEE);
    mvwhline(janela, 2, 1, ACS_HLINE, largura - 2);
    mvwaddch(janela, 2, largura - 1, ACS_RTEE);
    wattroff(janela, cor(COR_BORDA) | A_BOLD);

    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 0, (largura - (int)strlen(titulo) - 2) / 2,
              " %s ", titulo);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
}

static void desenhar_moldura_simples(WINDOW *janela, int largura,
                                     const char *titulo) {
    wattron(janela, cor(COR_BORDA) | A_BOLD);
    wborder(janela, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wattroff(janela, cor(COR_BORDA) | A_BOLD);

    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 0, (largura - (int)strlen(titulo) - 2) / 2,
              " %s ", titulo);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
}

static void desenhar_secao(WINDOW *janela, int y, int x, int largura,
                           const char *titulo) {
    wattron(janela, cor(COR_MUTED));
    preencher_linha(janela, y, x, largura, ACS_HLINE);
    wattroff(janela, cor(COR_MUTED));

    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, y, x + 2, " %s ", titulo);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
}

static void desenhar_rodape(WINDOW *janela, int altura, int largura,
                            const char *texto) {
    wattron(janela, cor(COR_MUTED));
    mvwhline(janela, altura - 3, 1, ACS_HLINE, largura - 2);
    wattroff(janela, cor(COR_MUTED));

    wattron(janela, cor(COR_ALERTA) | A_DIM);
    mvwprintw(janela, altura - 2, 2, "%-*.*s", largura - 4, largura - 4,
              texto);
    wattroff(janela, cor(COR_ALERTA) | A_DIM);
}

static void limpar_painel(WINDOW *janela, const char *titulo) {
    werase(janela);
    desenhar_moldura(janela, RES_W, titulo);
}

static void desenhar_badge(WINDOW *janela, int y, int x, const char *texto,
                           int par) {
    wattron(janela, cor(par) | A_BOLD);
    mvwprintw(janela, y, x, " %s ", texto);
    wattroff(janela, cor(par) | A_BOLD);
}

static WINDOW *criar_janela(int altura, int largura, const char *titulo) {
    WINDOW *janela = newwin(altura, largura, painel_linha, painel_coluna);

    if (!janela) return NULL;

    keypad(janela, TRUE);
    wbkgd(janela, cor(COR_TEXTO));
    desenhar_moldura(janela, largura, titulo);
    return janela;
}

static WINDOW *criar_menu(const char *titulo) {
    WINDOW *janela = newwin(MENU_H, painel_largura, 0, painel_coluna);

    if (!janela) return NULL;

    keypad(janela, TRUE);
    wbkgd(janela, cor(COR_TEXTO));
    desenhar_moldura_simples(janela, painel_largura, titulo);
    return janela;
}

static void fechar_janela(WINDOW *janela) {
    if (!janela) return;
    delwin(janela);
}

static void aguardar(WINDOW *janela) {
    desenhar_rodape(janela, RES_H, RES_W, "Pressione ENTER para voltar");
    wrefresh(janela);

    int tecla;
    do {
        tecla = wgetch(janela);
    } while (!eh_enter(tecla));
}

static void mostrar_aviso(const char *titulo, const char *mensagem) {
    WINDOW *janela = criar_janela(RES_H, RES_W, titulo);
    if (!janela) return;
    desenhar_secao(janela, 4, 3, RES_W - 6, "Aviso");
    wattron(janela, cor(COR_ALERTA) | A_BOLD);
    mvwprintw(janela, 7, 5, "%s", mensagem);
    wattroff(janela, cor(COR_ALERTA) | A_BOLD);
    aguardar(janela);
    fechar_janela(janela);
}

static void mostrar_painel_inicial(void) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Painel");
    if (!janela) return;

    desenhar_secao(janela, 4, 3, RES_W - 6, "Pronto");
    wattron(janela, cor(COR_TEXTO) | A_DIM);
    mvwprintw(janela, 6, 5, "Escolha uma opcao no menu superior.");
    mvwprintw(janela, 8, 5, "O resultado aparece aqui, sem abrir outra tela.");
    wattroff(janela, cor(COR_TEXTO) | A_DIM);
    wrefresh(janela);
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
        "Carregar",
        "Regs",
        "Assembly",
        "Run",
        "Step",
        "Pipeline",
        "Stats",
        "Memoria",
        "Sair"
    };

    int largura = getmaxx(menu);

    int colunas = 3;
    int col_w = (largura - 4) / colunas;

    werase(menu);
    desenhar_moldura_simples(menu, largura, "Mini MIPS Pipeline");

    wattron(menu, cor(COR_ALERTA) | A_DIM);
    mvwprintw(menu, 1, 3, "Setas/num + ENTER");
    wattroff(menu, cor(COR_ALERTA) | A_DIM);

    if (estado->num_instrucoes > 0) {
        wattron(menu, cor(COR_INFO) | A_BOLD);
        mvwprintw(menu, 1, largura - 33, "%3d inst | ciclo %d | PC %d",
                  estado->num_instrucoes, estado->ciclos, estado->PC);
        wattroff(menu, cor(COR_INFO) | A_BOLD);
    } else {
        wattron(menu, cor(COR_TEXTO) | A_DIM);
        mvwprintw(menu, 1, largura - 28, "Nenhum programa carregado");
        wattroff(menu, cor(COR_TEXTO) | A_DIM);
    }

    for (int i = 0; i < TOTAL_OPCOES; i++) {
        int numero = i == MENU_SAIR ? 0 : i + 1;
        int linha = 2 + (i / colunas);
        int coluna = 2 + (i % colunas) * col_w;
        int opcao_w = col_w - 2;

        if (i == selecionada) {
            wattron(menu, cor(COR_SELECAO) | A_BOLD);
            mvwprintw(menu, linha, coluna, "%-*.*s", opcao_w, opcao_w, "");
            mvwprintw(menu, linha, coluna + 1, "[%d] %-*.*s", numero,
                      opcao_w - 5, opcao_w - 5, opcoes[i]);
            wattroff(menu, cor(COR_SELECAO) | A_BOLD);
        } else {
            wattron(menu, cor(COR_TEXTO));
            mvwprintw(menu, linha, coluna + 1, "[%d] %-*.*s", numero,
                      opcao_w - 5, opcao_w - 5, opcoes[i]);
            wattroff(menu, cor(COR_TEXTO));
        }
    }
    wrefresh(menu);
}

static OpcaoMenu ler_opcao(WINDOW *menu, Estado *estado) {
    int selecionada = MENU_CARREGAR;

    while (1) {
        desenhar_menu(menu, selecionada, estado);
        int tecla = wgetch(menu);

        if (tecla == KEY_UP && selecionada >= 3)
            selecionada -= 3;
        else if (tecla == KEY_DOWN && selecionada + 3 < TOTAL_OPCOES)
            selecionada += 3;
        else if (tecla == KEY_LEFT && selecionada > 0)
            selecionada--;
        else if (tecla == KEY_RIGHT && selecionada < TOTAL_OPCOES - 1)
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
        limpar_painel(janela, "Carregar .mem");
        desenhar_secao(janela, 4, 3, RES_W - 6, "Arquivo");
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, 6, 5, "Digite o nome do arquivo .mem");
        wattroff(janela, cor(COR_INFO));
        wattron(janela, cor(COR_TITULO) | A_BOLD);
        mvwprintw(janela, 8, 5, "Arquivo: ");
        wattroff(janela, cor(COR_TITULO) | A_BOLD);
        wmove(janela, 8, 14);
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

        wattron(janela, cor(COR_ALERTA) | A_BOLD);
        mvwprintw(janela, 11, 5, "O nome nao pode ficar vazio.");
        wattroff(janela, cor(COR_ALERTA) | A_BOLD);
        mvwprintw(janela, 13, 5, "Digite o nome e pressione ENTER.");
        wrefresh(janela);
    }

    Estado novo_estado;
    inicializar_estado(&novo_estado);
    int quantidade = leitura_arquivo_mem(novo_estado.mem_instrucoes, nome);

    limpar_painel(janela, "Carregar .mem");
    desenhar_secao(janela, 4, 3, RES_W - 6, "Resultado");

    if (quantidade > 0) {
        novo_estado.num_instrucoes = quantidade;
        *estado = novo_estado;
        desenhar_badge(janela, 6, 5, "OK", COR_SUCESSO);
        wattron(janela, cor(COR_INFO) | A_BOLD);
        mvwprintw(janela, 6, 10, "Arquivo carregado com sucesso.");
        wattroff(janela, cor(COR_INFO) | A_BOLD);
        mvwprintw(janela, 9, 5, "Instrucoes: %d", quantidade);
        mvwprintw(janela, 11, 5, "Arquivo: %.52s", nome);
    } else {
        desenhar_badge(janela, 6, 5, "ERRO", COR_ALERTA);
        wattron(janela, cor(COR_ALERTA) | A_BOLD);
        mvwprintw(janela, 6, 12, "Nao foi possivel carregar o arquivo.");
        wattroff(janela, cor(COR_ALERTA) | A_BOLD);
        mvwprintw(janela, 10, 5, "Informado: %.50s", nome);
    }

    aguardar(janela);
    fechar_janela(janela);
}

static void tela_registradores(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Registradores");
    if (!janela) return;

    desenhar_secao(janela, 4, 3, RES_W - 6, "Banco de registradores");
    for (int i = 0; i < 8; i++) {
        int coluna = 6 + (i % 4) * 15;
        int linha = 6 + (i / 4) * 2;
        wattron(janela, cor(COR_INFO) | A_BOLD);
        mvwprintw(janela, linha, coluna, "$%d", i);
        wattroff(janela, cor(COR_INFO) | A_BOLD);
        wattron(janela, cor(COR_TEXTO));
        mvwprintw(janela, linha, coluna + 4, "= %4d",
                  estado->registradores[i]);
        wattroff(janela, cor(COR_TEXTO));
    }

    desenhar_secao(janela, 10, 3, RES_W - 6, "Controle");
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 12, 6, "PC = %d", estado->PC);
    mvwprintw(janela, 12, 22, "Ciclos = %d", estado->ciclos);
    mvwprintw(janela, 12, 42, "Instrucoes = %d", estado->instrucoes);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
    aguardar(janela);
    fechar_janela(janela);
}

static void tela_assembly(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Assembly");
    int deslocamento = 0;
    int maximo = RES_H - 9;
    if (!janela) return;
    if (maximo < 1) maximo = 1;

    while (1) {
        limpar_painel(janela, "Assembly");
        desenhar_secao(janela, 4, 3, RES_W - 6, "Instrucoes");

        for (int i = 0; i < maximo && i + deslocamento <
             estado->num_instrucoes; i++) {
            int indice = i + deslocamento;
            char texto[64];
            instrucao_para_asm(estado->mem_instrucoes[indice], texto);
            if (indice == estado->PC)
                wattron(janela, cor(COR_SELECAO) | A_BOLD);
            else
                wattron(janela, cor(COR_INFO));
            mvwprintw(janela, 6 + i, 5, "%c [%2d] %-45.45s",
                      indice == estado->PC ? '>' : ' ', indice, texto);
            if (indice == estado->PC)
                wattroff(janela, cor(COR_SELECAO) | A_BOLD);
            else
                wattroff(janela, cor(COR_INFO));
        }

        desenhar_rodape(janela, RES_H, RES_W,
                        "Setas: rolar | ENTER ou Q: voltar");
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

static void desenhar_linha_pipeline(WINDOW *janela, int y, const char *rotulo,
                                    int valido, const char *linha1,
                                    const char *linha2) {
    char resumo[160];
    int largura_resumo = RES_W - 31;
    if (largura_resumo < 10) largura_resumo = 10;

    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, y, 5, "%-8s", rotulo);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);

    if (valido) {
        desenhar_badge(janela, y, 15, "ATIVO", COR_SUCESSO);
        snprintf(resumo, sizeof(resumo), "%s | %s", linha1, linha2);
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, y, 26, "%-*.*s", largura_resumo,
                  largura_resumo, resumo);
        wattroff(janela, cor(COR_INFO));
    } else {
        desenhar_badge(janela, y, 15, "BOLHA", COR_BOLHA);
        wattron(janela, cor(COR_TEXTO) | A_DIM);
        mvwprintw(janela, y, 26, "Estagio vazio");
        wattroff(janela, cor(COR_TEXTO) | A_DIM);
    }
}

static void imprimir_pipeline_janela(WINDOW *janela, const Estado *estado) {
    char texto[64];
    char linha1[80];
    char linha2[80];
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 1, RES_W - 22, "Ciclo: %d | PC: %d",
              estado->ciclos, estado->PC);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);

    desenhar_secao(janela, 4, 3, RES_W - 6, "Fluxo do pipeline");

    if (estado->bi_di.valido) {
        instrucao_para_asm(estado->bi_di.instrucao, texto);
        snprintf(linha1, sizeof(linha1), "%s", texto);
        snprintf(linha2, sizeof(linha2), "PC+1=%d", estado->bi_di.PC_mais1);
    } else {
        linha1[0] = '\0';
        linha2[0] = '\0';
    }
    desenhar_linha_pipeline(janela, 6, "BI/DI", estado->bi_di.valido,
                            linha1, linha2);

    if (estado->di_ex.valido) {
        instrucao_para_asm(estado->di_ex.instrucao_raw, texto);
        snprintf(linha1, sizeof(linha1), "%s", texto);
        snprintf(linha2, sizeof(linha2), "A=%d B=%d imm=%d",
                 estado->di_ex.A, estado->di_ex.B, estado->di_ex.c.imm);
    } else {
        linha1[0] = '\0';
        linha2[0] = '\0';
    }
    desenhar_linha_pipeline(janela, 8, "DI/EX",
                            estado->di_ex.valido, linha1, linha2);

    if (estado->ex_mem.valido) {
        instrucao_para_asm(estado->ex_mem.instrucao_raw, texto);
        snprintf(linha1, sizeof(linha1), "%s", texto);
        snprintf(linha2, sizeof(linha2), "ULA=%d zero=%d destino=$%d",
                 estado->ex_mem.ULAout, estado->ex_mem.zero,
                 estado->ex_mem.rd_dest);
    } else {
        linha1[0] = '\0';
        linha2[0] = '\0';
    }
    desenhar_linha_pipeline(janela, 10, "EX/MEM",
                            estado->ex_mem.valido, linha1, linha2);

    if (estado->mem_er.valido) {
        instrucao_para_asm(estado->mem_er.instrucao_raw, texto);
        snprintf(linha1, sizeof(linha1), "%s", texto);
        snprintf(linha2, sizeof(linha2), "resultado=%d destino=$%d",
                 estado->mem_er.resultado, estado->mem_er.rd_dest);
    } else {
        linha1[0] = '\0';
        linha2[0] = '\0';
    }
    desenhar_linha_pipeline(janela, 12, "MEM/ER",
                            estado->mem_er.valido, linha1, linha2);
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
    int compacto = RES_H < 22;
    int acao_y = compacto ? RES_H - 4 : RES_H - 5;
    int info_y = compacto ? RES_H - 4 : RES_H - 4;
    if (!janela) return;

    while (1) {
        limpar_painel(janela, "Step / Stepback");
        imprimir_pipeline_janela(janela, estado);

        wattron(janela, cor(COR_TITULO) | A_BOLD);
        mvwprintw(janela, acao_y, 4, "Acao:");
        wattroff(janela, cor(COR_TITULO) | A_BOLD);

        if (selecionada == ACAO_STEP)
            wattron(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattron(janela, cor(COR_TEXTO));
        mvwprintw(janela, acao_y, 12, " [1] Step ");
        if (selecionada == ACAO_STEP)
            wattroff(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattroff(janela, cor(COR_TEXTO));

        if (selecionada == ACAO_STEPBACK)
            wattron(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattron(janela, cor(COR_TEXTO));
        mvwprintw(janela, acao_y, 25, " [2] Stepback ");
        if (selecionada == ACAO_STEPBACK)
            wattroff(janela, cor(COR_SELECAO) | A_BOLD);
        else
            wattroff(janela, cor(COR_TEXTO));

        wattron(janela, cor(COR_ALERTA) | A_DIM);
        if (selecionada == ACAO_STEP && programa_terminou(estado))
            mvwprintw(janela, info_y, compacto ? 42 : 4, "%s",
                      compacto ? "Fim do programa." :
                      "Step indisponivel: programa terminou.");
        else if (selecionada == ACAO_STEPBACK &&
                 historico_pipeline_tamanho() == 0)
            mvwprintw(janela, info_y, compacto ? 42 : 4, "%s",
                      compacto ? "Sem historico." :
                      "Stepback indisponivel: sem historico.");
        else
            mvwprintw(janela, info_y, compacto ? 42 : 4,
                      "Historico: %d", historico_pipeline_tamanho());
        wattroff(janela, cor(COR_ALERTA) | A_DIM);

        desenhar_rodape(janela, RES_H, RES_W,
                        "Setas/1/2: escolher | ENTER: executar | Q: voltar");
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
    int linhas_visiveis = RES_H - 9;
    int colunas = 2;
    int por_pagina = linhas_visiveis * colunas;
    if (!janela) return;

    while (1) {
        limpar_painel(janela, "Memoria de dados");
        desenhar_secao(janela, 4, 3, RES_W - 6, "Enderecos 0-255");

        for (int i = 0; i < por_pagina && i + deslocamento < 256; i++) {
            int idx = i + deslocamento;
            int col = (i / linhas_visiveis) * 31;
            int lin = i % linhas_visiveis;
            int val = estado->mem_dados[idx];

            if (val != 0)
                wattron(janela, cor(COR_INFO) | A_BOLD);
            else
                wattron(janela, cor(COR_TEXTO) | A_DIM);

            mvwprintw(janela, 6 + lin, 5 + col,
                      "mem[%3d] = %4d", idx, val);

            if (val != 0)
                wattroff(janela, cor(COR_INFO) | A_BOLD);
            else
                wattroff(janela, cor(COR_TEXTO) | A_DIM);
        }

        desenhar_rodape(janela, RES_H, RES_W,
                        "Setas: rolar | ENTER ou Q: voltar");
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
    int compacto = RES_H < 22;
    int regs_y = compacto ? 9 : 10;
    int regs_linha = compacto ? 11 : 12;
    if (!janela) return;
    desenhar_secao(janela, 4, 3, RES_W - 6, "Resumo");
    desenhar_badge(janela, 6, 5, "RUN", COR_SUCESSO);
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    mvwprintw(janela, 6, 12, "Ciclos: %d", estado->ciclos);
    mvwprintw(janela, 7, 12, "Instrucoes concluidas: %d",
              estado->instrucoes);
    wattroff(janela, cor(COR_TITULO) | A_BOLD);
    if (compacto) {
        mvwprintw(janela, 7, 36, "PC = %d", estado->PC);
    }

    desenhar_secao(janela, regs_y, 3, RES_W - 6, "Registradores");
    for (int i = 0; i < 8; i++) {
        int coluna = compacto ? 6 + (i % 4) * 15 : (i < 4 ? 6 : 36);
        int linha = compacto ? regs_linha + (i / 4) : regs_linha + (i % 4);
        wattron(janela, cor(COR_INFO));
        mvwprintw(janela, linha, coluna, "$%d = %4d", i,
                  estado->registradores[i]);
        wattroff(janela, cor(COR_INFO));
    }
    if (!compacto) {
        wattron(janela, cor(COR_TITULO) | A_BOLD);
        mvwprintw(janela, 18, 6, "PC = %d", estado->PC);
        wattroff(janela, cor(COR_TITULO) | A_BOLD);
    }
    aguardar(janela);
    fechar_janela(janela);
}

static void tela_estatisticas(const Estado *estado) {
    WINDOW *janela = criar_janela(RES_H, RES_W, "Estatisticas");
    int compacto = RES_H < 22;
    if (!janela) return;

    desenhar_secao(janela, 4, 3, RES_W - 6, "Execucao");
    wattron(janela, cor(COR_INFO));
    mvwprintw(janela, 6, 6, "Ciclos       : %d", estado->ciclos);
    mvwprintw(janela, 7, 6, "Instrucoes   : %d", estado->instrucoes);
    wattroff(janela, cor(COR_INFO));

    desenhar_secao(janela, compacto ? 9 : 10, 3, RES_W - 6, "Instrucoes");
    wattron(janela, cor(COR_INFO));
    if (compacto) {
        mvwprintw(janela, 11, 6, "Tipo R       : %d", estado->qtd_tipo_r);
        mvwprintw(janela, 12, 6, "addi         : %d", estado->qtd_addi);
        mvwprintw(janela, 13, 6, "lw           : %d", estado->qtd_lw);
        mvwprintw(janela, 11, 36, "sw           : %d", estado->qtd_sw);
        mvwprintw(janela, 12, 36, "beq          : %d", estado->qtd_beq);
        mvwprintw(janela, 13, 36, "jump         : %d", estado->qtd_jump);
    } else {
        mvwprintw(janela, 12, 6, "Tipo R       : %d", estado->qtd_tipo_r);
        mvwprintw(janela, 13, 6, "addi         : %d", estado->qtd_addi);
        mvwprintw(janela, 14, 6, "lw           : %d", estado->qtd_lw);
        mvwprintw(janela, 15, 6, "sw           : %d", estado->qtd_sw);
        mvwprintw(janela, 16, 6, "beq          : %d", estado->qtd_beq);
        mvwprintw(janela, 17, 6, "jump         : %d", estado->qtd_jump);
    }
    wattroff(janela, cor(COR_INFO));

    wattron(janela, cor(COR_ALERTA) | A_BOLD);
    mvwprintw(janela, 6, 36, "Bolhas       : %d", estado->bolhas);
    wattroff(janela, cor(COR_ALERTA) | A_BOLD);
    wattron(janela, cor(COR_TITULO) | A_BOLD);
    if (estado->instrucoes > 0)
        mvwprintw(janela, 7, 36, "CPI          : %.2f",
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

    if (LINES < MIN_H || COLS < MIN_W) {
        endwin();
        fprintf(stderr, "O terminal precisa ter pelo menos %dx%d.\n",
                MIN_W, MIN_H);
        return 1;
    }

    painel_largura = COLS > 92 ? 92 : COLS;
    painel_coluna = (COLS - painel_largura) / 2;
    painel_linha = MENU_H;
    painel_altura = LINES - MENU_H;

    WINDOW *menu = criar_menu("Mini MIPS Pipeline");
    if (!menu) {
        endwin();
        fprintf(stderr, "Nao foi possivel criar o menu.\n");
        return 1;
    }

    mostrar_painel_inicial();

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
