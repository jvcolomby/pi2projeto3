#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include "simulador.h"

// ─── Dimensões ────────────────────────────────────────────────────────────────
#define MENU_W   44
#define MENU_H   13
#define RES_W    60
#define RES_H    24

static const char *nome_estagio[] = { "BI", "DI", "EX", "MEM", "ER" };

// ─── Janela de resultado ──────────────────────────────────────────────────────
static WINDOW *abrir_resultado(const char *titulo) {
    int row = (LINES - RES_H) / 2;
    int col = (COLS  - RES_W) / 2;
    WINDOW *w = newwin(RES_H, RES_W, row, col);
    keypad(w, TRUE);
    box(w, 0, 0);
    mvwprintw(w, 0, (RES_W - strlen(titulo)) / 2, " %s ", titulo);
    return w;
}

static void fechar_resultado(WINDOW *w) {
    werase(w);
    wrefresh(w);
    delwin(w);
}

static void aguardar(WINDOW *w, int linha) {
    wattron(w, A_DIM);
    mvwprintw(w, linha, 2, "Pressione qualquer tecla...");
    wattroff(w, A_DIM);
    wrefresh(w);
    wgetch(w);
}

// ─── Tela de registradores ────────────────────────────────────────────────────
static void tela_registradores(Estado *e) {
    WINDOW *w = abrir_resultado("Registradores");
    for (int i = 0; i < 8; i++)
        mvwprintw(w, 2 + i, 4, "$%d = %d", i, e->registradores[i]);
    mvwprintw(w, 11, 4, "PC = %d", e->PC);
    aguardar(w, RES_H - 2);
    fechar_resultado(w);
}

// ─── Tela de assembly ─────────────────────────────────────────────────────────
static void tela_assembly(Estado *e, int n) {
    WINDOW *w = abrir_resultado("Assembly");
    for (int i = 0; i < n && i < RES_H - 4; i++) {
        char buf[64];
        instrucao_para_asm(e->mem_instrucoes[i], buf);
        mvwprintw(w, 2 + i, 4, "[%2d] %s", i, buf);
    }
    aguardar(w, RES_H - 2);
    fechar_resultado(w);
}

// ─── Tela de pipeline ─────────────────────────────────────────────────────────
static void tela_pipeline(Estado *e) {
    char buf[64];
    WINDOW *w = abrir_resultado("Pipeline");
    mvwprintw(w, 1, 2, "Ciclo: %d", e->ciclos);

    // BI/DI
    mvwprintw(w, 3, 2, "BI/DI  :");
    if (e->bi_di.valido) {
        instrucao_para_asm(e->bi_di.instrucao, buf);
        mvwprintw(w, 3, 11, "%-20s PC+1=%d", buf, e->bi_di.PC_mais1);
    } else mvwprintw(w, 3, 11, "[bolha]");

    // DI/EX
    mvwprintw(w, 5, 2, "DI/EX  :");
    if (e->di_ex.valido) {
        instrucao_para_asm(e->di_ex.instrucao_raw, buf);
        mvwprintw(w, 5, 11, "%-20s", buf);
        int op = e->di_ex.c.opcode;
        if (op == OP_TIPO_R)
            mvwprintw(w, 6, 11, "A=%d  B=%d  rd=$%d", e->di_ex.A, e->di_ex.B, e->di_ex.c.rd);
        else if (op == OP_BEQ)
            mvwprintw(w, 6, 11, "A=%d  B=%d  imm=%d", e->di_ex.A, e->di_ex.B, e->di_ex.c.imm);
        else if (op == OP_SW)
            mvwprintw(w, 6, 11, "A=%d  B=%d  imm=%d", e->di_ex.A, e->di_ex.B, e->di_ex.c.imm);
        else if (op == OP_JUMP)
            mvwprintw(w, 6, 11, "addr=%d", e->di_ex.c.addr);
        else
            mvwprintw(w, 6, 11, "A=%d  imm=%d  rt=$%d", e->di_ex.A, e->di_ex.c.imm, e->di_ex.c.rt);
    } else mvwprintw(w, 5, 11, "[bolha]");

    // EX/MEM
    mvwprintw(w, 8, 2, "EX/MEM :");
    if (e->ex_mem.valido) {
        int op = e->ex_mem.opcode;
        if (op == OP_TIPO_R)
            mvwprintw(w, 8, 11, "tipo R   ULAout=%d  rd=$%d", e->ex_mem.ULAout, e->ex_mem.rd_dest);
        else if (op == OP_ADDI)
            mvwprintw(w, 8, 11, "addi     ULAout=%d  rt=$%d", e->ex_mem.ULAout, e->ex_mem.rd_dest);
        else if (op == OP_LW)
            mvwprintw(w, 8, 11, "lw       end=%d  rt=$%d", e->ex_mem.ULAout, e->ex_mem.rd_dest);
        else if (op == OP_SW)
            mvwprintw(w, 8, 11, "sw       end=%d  dado=%d", e->ex_mem.ULAout, e->ex_mem.B);
        else if (op == OP_BEQ)
            mvwprintw(w, 8, 11, "beq      zero=%d  PC_branch=%d", e->ex_mem.zero, e->ex_mem.PC_branch);
        else if (op == OP_JUMP)
            mvwprintw(w, 8, 11, "jump     addr=%d", e->ex_mem.addr);
    } else mvwprintw(w, 8, 11, "[bolha]");

    // MEM/ER
    mvwprintw(w, 10, 2, "MEM/ER :");
    if (e->mem_er.valido) {
        int op = e->mem_er.opcode;
        if (op == OP_SW || op == OP_BEQ || op == OP_JUMP)
            mvwprintw(w, 10, 11, "%-6s   (sem writeback)",
                op == OP_SW ? "sw" : op == OP_BEQ ? "beq" : "jump");
        else
            mvwprintw(w, 10, 11, "%-6s   resultado=%d  rd=$%d",
                op == OP_TIPO_R ? "tipo R" : op == OP_ADDI ? "addi" : "lw",
                e->mem_er.resultado, e->mem_er.rd_dest);
    } else mvwprintw(w, 10, 11, "[bolha]");

    aguardar(w, RES_H - 2);
    fechar_resultado(w);
}

// ─── Tela de estatísticas ─────────────────────────────────────────────────────
static void tela_estatisticas(Estado *e) {
    WINDOW *w = abrir_resultado("Estatisticas");
    mvwprintw(w,  2, 4, "Ciclos       : %d", e->ciclos);
    mvwprintw(w,  3, 4, "Instrucoes   : %d", e->instrucoes);
    mvwprintw(w,  5, 4, "  Tipo R     : %d", e->qtd_tipo_r);
    mvwprintw(w,  6, 4, "  addi       : %d", e->qtd_addi);
    mvwprintw(w,  7, 4, "  lw         : %d", e->qtd_lw);
    mvwprintw(w,  8, 4, "  sw         : %d", e->qtd_sw);
    mvwprintw(w,  9, 4, "  beq        : %d", e->qtd_beq);
    mvwprintw(w, 10, 4, "  jump       : %d", e->qtd_jump);
    mvwprintw(w, 12, 4, "Bolhas       : %d", e->bolhas);
    if (e->instrucoes > 0)
        mvwprintw(w, 13, 4, "CPI          : %.2f", (float)e->ciclos / e->instrucoes);
    aguardar(w, RES_H - 2);
    fechar_resultado(w);
}

// ─── Tela de carregar arquivo ─────────────────────────────────────────────────
static int tela_carregar(Estado *e) {
    WINDOW *w = abrir_resultado("Carregar .mem");
    mvwprintw(w, 2, 2, "Arquivo: ");
    echo();
    curs_set(1);
    char nome[100];
    mvwgetnstr(w, 2, 11, nome, 80);
    noecho();
    curs_set(0);

    inicializar_estado(e);
    int n = leitura_arquivo_mem(e->mem_instrucoes, nome);

    werase(w);
    box(w, 0, 0);
    mvwprintw(w, 0, (RES_W - strlen("Carregar .mem")) / 2, " Carregar .mem ");
    if (n > 0)
        mvwprintw(w, 2, 2, "OK: %d instrucoes carregadas de '%s'", n, nome);
    else
        mvwprintw(w, 2, 2, "Erro ao abrir '%s'", nome);

    aguardar(w, RES_H - 2);
    fechar_resultado(w);
    return n;
}

// ─── Menu principal ───────────────────────────────────────────────────────────
static void desenhar_menu(WINDOW *w, int highlight, int n) {
    werase(w);
    box(w, 0, 0);

    const char *titulo = "Mini MIPS Pipeline";
    mvwprintw(w, 0, (MENU_W - strlen(titulo)) / 2, " %s ", titulo);

    const char *itens[] = {
        "Carregar .mem",
        "Registradores",
        "Assembly",
        "Run",
        "Step (por estagio)",
        "Estado do pipeline",
        "Estatisticas",
        "Sair",
    };
    int nitens = 8;

    for (int i = 0; i < nitens; i++) {
        if (i == highlight) wattron(w, A_REVERSE);
        mvwprintw(w, 2 + i, 3, "[%d] %s", i + 1 == 8 ? 0 : i + 1, itens[i]);
        if (i == highlight) wattroff(w, A_REVERSE);
    }

    if (n > 0) {
        wattron(w, A_DIM);
        mvwprintw(w, MENU_H - 2, 2, "%d instrucoes carregadas", n);
        wattroff(w, A_DIM);
    }

    mvwprintw(w, MENU_H - 1, 0, " setas / enter ");
    wrefresh(w);
}

// ─── main ─────────────────────────────────────────────────────────────────────
int main() {
    Estado e;
    inicializar_estado(&e);
    int n = 0;
    int estagio_atual = 0;

    initscr();
    curs_set(0);
    noecho();
    cbreak();

    int row = (LINES - MENU_H) / 2;
    int col = (COLS  - MENU_W) / 2;
    WINDOW *menu = newwin(MENU_H, MENU_W, row, col);
    keypad(menu, TRUE);

    int highlight = 0;
    int nitens = 8;
    int ch;

    while (1) {
        desenhar_menu(menu, highlight, n);
        ch = wgetch(menu);

        if (ch == KEY_UP   && highlight > 0)        highlight--;
        if (ch == KEY_DOWN && highlight < nitens-1)  highlight++;

        if (ch == '\n') {
            switch (highlight) {
                case 0: // Carregar
                    n = tela_carregar(&e);
                    estagio_atual = 0;
                    break;
                case 1: // Registradores
                    tela_registradores(&e);
                    break;
                case 2: // Assembly
                    if (n == 0) {
                        WINDOW *w = abrir_resultado("Aviso");
                        mvwprintw(w, 2, 2, "Carregue um arquivo .mem primeiro.");
                        aguardar(w, RES_H - 2);
                        fechar_resultado(w);
                    } else tela_assembly(&e, n);
                    break;
                case 3: // Run
                    if (n == 0) {
                        WINDOW *w = abrir_resultado("Aviso");
                        mvwprintw(w, 2, 2, "Carregue um arquivo .mem primeiro.");
                        aguardar(w, RES_H - 2);
                        fechar_resultado(w);
                    } else {
                        run(&e, n);
                        tela_registradores(&e);
                    }
                    break;
                case 4: { // Step
                    if (n == 0) {
                        WINDOW *w = abrir_resultado("Aviso");
                        mvwprintw(w, 2, 2, "Carregue um arquivo .mem primeiro.");
                        aguardar(w, RES_H - 2);
                        fechar_resultado(w);
                        break;
                    }
                    if (e.PC >= n && !e.bi_di.valido && !e.di_ex.valido &&
                        !e.ex_mem.valido && !e.mem_er.valido) {
                        WINDOW *w = abrir_resultado("Step");
                        mvwprintw(w, 2, 2, "Pipeline vazio.");
                        aguardar(w, RES_H - 2);
                        fechar_resultado(w);
                        break;
                    }
                    WINDOW *w = abrir_resultado("Step");
                    mvwprintw(w, 1, 2, "Estagio: %s  (ciclo %d)",
                        nome_estagio[estagio_atual], e.ciclos);
                    switch (estagio_atual) {
                        case 0: estagio_BI(&e);  break;
                        case 1: estagio_DI(&e);  break;
                        case 2: estagio_EX(&e);  break;
                        case 3: estagio_MEM(&e); break;
                        case 4: estagio_ER(&e); e.ciclos++; break;
                    }
                    // pipeline inline na janela de step
                    char buf[64];
                    mvwprintw(w, 3, 2, "BI/DI :");
                    if (e.bi_di.valido) {
                        instrucao_para_asm(e.bi_di.instrucao, buf);
                        mvwprintw(w, 3, 10, "%-22s PC+1=%d", buf, e.bi_di.PC_mais1);
                    } else mvwprintw(w, 3, 10, "[bolha]");

                    mvwprintw(w, 5, 2, "DI/EX :");
                    if (e.di_ex.valido) {
                        instrucao_para_asm(e.di_ex.instrucao_raw, buf);
                        mvwprintw(w, 5, 10, "%-22s", buf);
                        mvwprintw(w, 6, 10, "A=%d B=%d", e.di_ex.A, e.di_ex.B);
                    } else mvwprintw(w, 5, 10, "[bolha]");

                    mvwprintw(w, 8, 2, "EX/MEM:");
                    if (e.ex_mem.valido)
                        mvwprintw(w, 8, 10, "ULAout=%d zero=%d rd=$%d",
                            e.ex_mem.ULAout, e.ex_mem.zero, e.ex_mem.rd_dest);
                    else mvwprintw(w, 8, 10, "[bolha]");

                    mvwprintw(w, 10, 2, "MEM/ER:");
                    if (e.mem_er.valido)
                        mvwprintw(w, 10, 10, "resultado=%d rd=$%d",
                            e.mem_er.resultado, e.mem_er.rd_dest);
                    else mvwprintw(w, 10, 10, "[bolha]");

                    estagio_atual = (estagio_atual + 1) % 5;
                    if (estagio_atual == 0)
                        mvwprintw(w, 12, 2, ">>> Ciclo %d completo", e.ciclos);

                    aguardar(w, RES_H - 2);
                    fechar_resultado(w);
                    break;
                }
                case 5: // Pipeline
                    tela_pipeline(&e);
                    break;
                case 6: // Estatisticas
                    tela_estatisticas(&e);
                    break;
                case 7: // Sair
                    endwin();
                    return 0;
            }
        }
    }
    endwin();
    return 0;
}