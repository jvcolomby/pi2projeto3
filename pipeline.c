#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulador.h"

#define MAX_HISTORICO 1000

static Estado historico[MAX_HISTORICO];
static int topo_historico = 0;

Decode campos(int instrucao) {
    Decode c;
    c.opcode = (instrucao >> 12) & 0xF;
    c.rs     = (instrucao >> 9)  & 0x7;
    c.rt     = (instrucao >> 6)  & 0x7;
    c.rd     = (instrucao >> 3)  & 0x7;
    c.funct  =  instrucao        & 0x7;
    c.imm    =  instrucao        & 0x3F;
    c.addr   =  instrucao        & 0xFF;
    if (c.imm >= 32) c.imm -= 64;
    return c;
}

static int opcode_valido(int opcode, int funct) {
    if (opcode == OP_TIPO_R)
        return funct == FUNCT_ADD || funct == FUNCT_SUB ||
               funct == FUNCT_AND || funct == FUNCT_OR;

    return opcode == OP_ADDI || opcode == OP_LW || opcode == OP_SW ||
           opcode == OP_BEQ || opcode == OP_JUMP;
}

static void gerar_controle(Decode c, Controle_EX *ex, Controle_MEM *mem,
                           Controle_ER *er) {
    memset(ex, 0, sizeof(*ex));
    memset(mem, 0, sizeof(*mem));
    memset(er, 0, sizeof(*er));

    ex->reg_dst = c.opcode == OP_TIPO_R;
    ex->ula_op = controle_ULA(c.opcode, c.funct);
    ex->ula_fonte = c.opcode == OP_ADDI || c.opcode == OP_LW ||
                    c.opcode == OP_SW;
    mem->dvc = c.opcode == OP_BEQ;
    mem->dvi = c.opcode == OP_JUMP;
    mem->esc_mem = c.opcode == OP_SW;
    er->esc_reg = c.opcode == OP_TIPO_R || c.opcode == OP_ADDI ||
                  c.opcode == OP_LW;
    er->mem_para_reg = c.opcode != OP_LW;
}

int controle_ULA(int opcode, int funct) {
    switch (opcode) {
        case OP_TIPO_R:
            switch (funct) {
                case FUNCT_ADD: return 0;
                case FUNCT_SUB: return 2;
                case FUNCT_AND: return 4;
                case FUNCT_OR:  return 5;
                default:        return -1;
            }
        case OP_ADDI:
        case OP_LW:
        case OP_SW:  return 0;
        case OP_BEQ: return 2;
        default:     return -1;
    }
}

int ULA(int A, int B, int controle, int *flag_zero) {
    int resultado = 0;
    switch (controle) {
        case 0: resultado = A + B; break;
        case 2: resultado = A - B; break;
        case 4: resultado = A & B; break;
        case 5: resultado = A | B; break;
        default: resultado = 0;
    }
    resultado &= 0xFF;
    if (resultado >= 128)
        resultado -= 256;
    *flag_zero = (resultado == 0);
    return resultado;
}

void instrucao_para_asm(int instrucao, char *buf) {
    Decode c = campos(instrucao);
    switch (c.opcode) {
        case OP_TIPO_R:
            switch (c.funct) {
                case FUNCT_ADD: sprintf(buf, "add $%d, $%d, $%d", c.rd, c.rs, c.rt); return;
                case FUNCT_SUB: sprintf(buf, "sub $%d, $%d, $%d", c.rd, c.rs, c.rt); return;
                case FUNCT_AND: sprintf(buf, "and $%d, $%d, $%d", c.rd, c.rs, c.rt); return;
                case FUNCT_OR:  sprintf(buf, "or $%d, $%d, $%d",  c.rd, c.rs, c.rt); return;
            }
            break;
        case OP_ADDI: sprintf(buf, "addi $%d, $%d, %d",  c.rt, c.rs, c.imm); return;
        case OP_LW:   sprintf(buf, "lw $%d, %d($%d)",    c.rt, c.imm, c.rs); return;
        case OP_SW:   sprintf(buf, "sw $%d, %d($%d)",    c.rt, c.imm, c.rs); return;
        case OP_BEQ:  sprintf(buf, "beq $%d, $%d, %d",   c.rs, c.rt, c.imm); return;
        case OP_JUMP: sprintf(buf, "j %d",                c.addr); return;
    }
    sprintf(buf, "??? (op=%d)", c.opcode);
}

int leitura_arquivo_mem(int mem_instrucoes[], char nome_arquivo[]) {
    FILE *arquivo = fopen(nome_arquivo, "r");
    if (!arquivo) return 0;

    char linha[200];
    int i = 0;
    while (fgets(linha, sizeof(linha), arquivo) && i < 256) {
        char *com = strstr(linha, "//");
        if (com) *com = '\0';
        linha[strcspn(linha, "\r\n")] = '\0';
        char *tok = strtok(linha, " \t");
        if (!tok || tok[0] == '\0') continue;

        size_t tamanho = strlen(tok);
        if (tamanho != 16 || strspn(tok, "01") != tamanho)
            continue;

        mem_instrucoes[i++] = (int)strtol(tok, NULL, 2);
    }
    fclose(arquivo);
    return i;
}

void inicializar_estado(Estado *e) {
    memset(e, 0, sizeof(Estado));
    e->bi_di.valido  = 0;
    e->di_ex.valido  = 0;
    e->ex_mem.valido = 0;
    e->mem_er.valido = 0;
    limpar_historico_pipeline();
}

static void salvar_estado_pipeline(const Estado *e) {
    if (topo_historico >= MAX_HISTORICO) {
        memmove(historico, historico + 1,
                (MAX_HISTORICO - 1) * sizeof(historico[0]));
        topo_historico = MAX_HISTORICO - 1;
    }

    historico[topo_historico++] = *e;
}

int stepback_pipeline(Estado *e) {
    if (topo_historico == 0)
        return 0;

    *e = historico[--topo_historico];
    return 1;
}

int historico_pipeline_tamanho(void) {
    return topo_historico;
}

void limpar_historico_pipeline(void) {
    topo_historico = 0;
}

void estagio_BI(Estado *e) {
    if (e->PC < 0 || e->PC >= e->num_instrucoes) {
        e->bi_di.valido = 0;
        return;
    }
    e->bi_di.instrucao = e->mem_instrucoes[e->PC];
    e->bi_di.PC_mais1  = e->PC + 1;
    e->bi_di.valido    = 1;
    e->PC++;
}

void estagio_DI(Estado *e) {
    if (!e->bi_di.valido) {
        e->di_ex.valido = 0;
        return;
    }
    Decode c = campos(e->bi_di.instrucao);
    if (!opcode_valido(c.opcode, c.funct)) {
        e->di_ex.valido = 0;
        return;
    }
    e->di_ex.c             = c;
    e->di_ex.instrucao_raw = e->bi_di.instrucao;
    e->di_ex.A             = e->registradores[c.rs];
    e->di_ex.B             = e->registradores[c.rt];
    e->di_ex.PC_mais1      = e->bi_di.PC_mais1;
    gerar_controle(c, &e->di_ex.controle_ex, &e->di_ex.controle_mem,
                   &e->di_ex.controle_er);
    e->di_ex.valido        = 1;
}

void estagio_EX(Estado *e) {
    if (!e->di_ex.valido) {
        e->ex_mem.valido = 0;
        return;
    }
    Decode c   = e->di_ex.c;
    int A      = e->di_ex.A;
    int B      = e->di_ex.B;
    int flag   = 0;
    int op2    = e->di_ex.controle_ex.ula_fonte ? c.imm : B;
    int result = c.opcode == OP_JUMP
        ? 0
        : ULA(A, op2, e->di_ex.controle_ex.ula_op, &flag);

    e->ex_mem.instrucao_raw = e->di_ex.instrucao_raw;
    e->ex_mem.ULAout        = result;
    e->ex_mem.B             = B;
    e->ex_mem.zero          = flag;
    e->ex_mem.opcode        = c.opcode;
    e->ex_mem.addr          = c.addr;
    e->ex_mem.PC_branch     = e->di_ex.PC_mais1 + c.imm;
    e->ex_mem.rd_dest       = e->di_ex.controle_ex.reg_dst ? c.rd : c.rt;
    e->ex_mem.controle_mem = e->di_ex.controle_mem;
    e->ex_mem.controle_er  = e->di_ex.controle_er;
    e->ex_mem.valido    = 1;
}

void estagio_MEM(Estado *e) {
    if (!e->ex_mem.valido) {
        e->mem_er.valido = 0;
        return;
    }
    int opcode    = e->ex_mem.opcode;
    int endereco  = e->ex_mem.ULAout & 0xFF;
    int resultado = e->ex_mem.ULAout;

    if (!e->ex_mem.controle_er.mem_para_reg)
        resultado = e->mem_dados[endereco];

    if (e->ex_mem.controle_mem.esc_mem)
        e->mem_dados[endereco] = e->ex_mem.B;

    if (e->ex_mem.controle_mem.dvc && e->ex_mem.zero) {
        e->PC = e->ex_mem.PC_branch;
        e->bi_di.valido = 0;
        e->di_ex.valido = 0;
        e->bolhas += 2;
    } else if (e->ex_mem.controle_mem.dvi) {
            e->PC = e->ex_mem.addr;
            e->bi_di.valido = 0;
            e->di_ex.valido = 0;
            e->bolhas += 2;
    }

    e->mem_er.instrucao_raw = e->ex_mem.instrucao_raw;
    e->mem_er.resultado     = resultado;
    e->mem_er.rd_dest       = e->ex_mem.rd_dest;
    e->mem_er.opcode        = opcode;
    e->mem_er.controle_er = e->ex_mem.controle_er;
    e->mem_er.valido    = 1;
}

void estagio_ER(Estado *e) {
    if (!e->mem_er.valido) return;

    int opcode = e->mem_er.opcode;

    e->instrucoes++;
    switch (opcode) {
        case OP_TIPO_R: e->qtd_tipo_r++; break;
        case OP_ADDI:   e->qtd_addi++;   break;
        case OP_LW:     e->qtd_lw++;     break;
        case OP_SW:     e->qtd_sw++;     break;
        case OP_BEQ:    e->qtd_beq++;    break;
        case OP_JUMP:   e->qtd_jump++;   break;
    }

    if (!e->mem_er.controle_er.esc_reg) {
        e->mem_er.valido = 0;
        return;
    }

    if (e->mem_er.rd_dest != 0)
        e->registradores[e->mem_er.rd_dest] = e->mem_er.resultado;

    e->mem_er.valido = 0;
}

/* ----------  detecção de hazard load-use  ---------- */
static int detectar_stall_load(const Estado *e) {
    /* LW no estágio DI/EX cujo destino (rt) é lido pela instrução em BI/DI */
    if (!e->di_ex.valido || e->di_ex.c.opcode != OP_LW)
        return 0;

    int lw_rt = e->di_ex.c.rt;          /* registrador destino do LW */
    if (lw_rt == 0) return 0;           /* $0 nunca causa hazard      */
    if (!e->bi_di.valido) return 0;

    Decode prox = campos(e->bi_di.instrucao);

    /* quais registradores a próxima instrução lê? */
    int le_rs = (prox.opcode != OP_JUMP);
    int le_rt = (prox.opcode == OP_TIPO_R || prox.opcode == OP_BEQ ||
                 prox.opcode == OP_SW);

    return (le_rs && prox.rs == lw_rt) || (le_rt && prox.rt == lw_rt);
}

/* ----------  forwarding  ---------- */
static void aplicar_forwarding(Estado *e,
                               int fwd_exmem_ok, int fwd_exmem_rd, int fwd_exmem_val,
                               int fwd_memer_ok, int fwd_memer_rd, int fwd_memer_val) {
    if (!e->di_ex.valido) return;

    int rs = e->di_ex.c.rs;
    int rt = e->di_ex.c.rt;

    /* ---- Forwarding para A (valor de rs) ---- */
    /* EX/MEM tem prioridade (instrução mais recente) */
    if (fwd_exmem_ok && fwd_exmem_rd == rs)
        e->di_ex.A = fwd_exmem_val;
    else if (fwd_memer_ok && fwd_memer_rd == rs)
        e->di_ex.A = fwd_memer_val;

    /* ---- Forwarding para B (valor de rt) ---- */
    if (fwd_exmem_ok && fwd_exmem_rd == rt)
        e->di_ex.B = fwd_exmem_val;
    else if (fwd_memer_ok && fwd_memer_rd == rt)
        e->di_ex.B = fwd_memer_val;
}

void ciclo_pipeline(Estado *e) {
    salvar_estado_pipeline(e);

    /* ---- 1. detectar stall load-use ANTES de avançar estágios ---- */
    int stall = detectar_stall_load(e);

    /* ---- 2. salvar fontes de forwarding (estado pré-ciclo) ---- */
    /* EX/MEM: forward ULAout, mas NÃO para LW (dado ainda não lido) */
    int fwd_exmem_ok  = e->ex_mem.valido &&
                        e->ex_mem.controle_er.esc_reg &&
                        e->ex_mem.rd_dest != 0 &&
                        e->ex_mem.opcode != OP_LW;
    int fwd_exmem_rd  = e->ex_mem.rd_dest;
    int fwd_exmem_val = e->ex_mem.ULAout;

    /* MEM/ER: forward resultado (já inclui dado da memória p/ LW) */
    int fwd_memer_ok  = e->mem_er.valido &&
                        e->mem_er.controle_er.esc_reg &&
                        e->mem_er.rd_dest != 0;
    int fwd_memer_rd  = e->mem_er.rd_dest;
    int fwd_memer_val = e->mem_er.resultado;

    /* ---- 3. executar estágios de trás para frente ---- */
    estagio_ER(e);
    estagio_MEM(e);

    /* aplicar forwarding nos operandos de DI/EX antes de EX consumir */
    aplicar_forwarding(e,
                       fwd_exmem_ok, fwd_exmem_rd, fwd_exmem_val,
                       fwd_memer_ok, fwd_memer_rd, fwd_memer_val);

    estagio_EX(e);

    if (stall) {
        /* bolha: DI/EX vira NOP, BI/DI e PC ficam congelados */
        e->di_ex.valido = 0;
        e->bolhas++;
        /* NÃO executa estagio_DI nem estagio_BI → PC não avança */
    } else {
        estagio_DI(e);
        estagio_BI(e);
    }

    e->ciclos++;
}

void run(Estado *e, int num_instrucoes) {
    e->num_instrucoes = num_instrucoes;
    while ((e->PC >= 0 && e->PC < num_instrucoes) ||
        e->bi_di.valido        ||
        e->di_ex.valido        ||
        e->ex_mem.valido       ||
        e->mem_er.valido) {
        
        ciclo_pipeline(e);
    }
}

void imprimir_registradores(Estado *e) {
    printf("\n=== Registradores ===\n");
    for (int i = 0; i < 8; i++)
        printf("  $%d = %d\n", i, e->registradores[i]);
    printf("  PC = %d\n", e->PC);
}

void imprimir_pipeline(Estado *e) {
    char buf[64];
    printf("\n=== Pipeline (ciclo %d) ===\n", e->ciclos);

    printf("\n  BI/DI  : ");
    if (e->bi_di.valido) {
        instrucao_para_asm(e->bi_di.instrucao, buf);
        printf("%s  |  PC+1=%d\n", buf, e->bi_di.PC_mais1);
    } else {
        printf("[bolha]\n");
    }

    printf("  DI/EX  : ");
    if (e->di_ex.valido) {
        instrucao_para_asm(e->di_ex.instrucao_raw, buf);
        int op = e->di_ex.c.opcode;
        if (op == OP_TIPO_R)
            printf("%s  |  A=%d  B=%d  rd=$%d\n", buf, e->di_ex.A, e->di_ex.B, e->di_ex.c.rd);
        else if (op == OP_BEQ)
            printf("%s  |  A=%d  B=%d  imm=%d  PC+1=%d\n", buf, e->di_ex.A, e->di_ex.B, e->di_ex.c.imm, e->di_ex.PC_mais1);
        else if (op == OP_JUMP)
            printf("%s  |  addr=%d\n", buf, e->di_ex.c.addr);
        else if (op == OP_SW)
            printf("%s  |  A=%d  B=%d  imm=%d\n", buf, e->di_ex.A, e->di_ex.B, e->di_ex.c.imm);
        else
            printf("%s  |  A=%d  imm=%d  rt=$%d\n", buf, e->di_ex.A, e->di_ex.c.imm, e->di_ex.c.rt);
    } else {
        printf("[bolha]\n");
    }

    printf("  EX/MEM : ");
    if (e->ex_mem.valido) {
        instrucao_para_asm(e->ex_mem.instrucao_raw, buf);
        int op = e->ex_mem.opcode;
        if (op == OP_TIPO_R)
            printf("%s  |  ULAout=%d  rd=$%d\n", buf, e->ex_mem.ULAout, e->ex_mem.rd_dest);
        else if (op == OP_ADDI)
            printf("%s  |  ULAout=%d  rt=$%d\n", buf, e->ex_mem.ULAout, e->ex_mem.rd_dest);
        else if (op == OP_LW)
            printf("%s  |  end=%d  rt=$%d\n", buf, e->ex_mem.ULAout, e->ex_mem.rd_dest);
        else if (op == OP_SW)
            printf("%s  |  end=%d  dado=%d\n", buf, e->ex_mem.ULAout, e->ex_mem.B);
        else if (op == OP_BEQ)
            printf("%s  |  zero=%d  PC_branch=%d\n", buf, e->ex_mem.zero, e->ex_mem.PC_branch);
        else if (op == OP_JUMP)
            printf("%s  |  addr=%d\n", buf, e->ex_mem.addr);
        else
            printf("op=%d  |  ULAout=%d\n", op, e->ex_mem.ULAout);
    } else {
        printf("[bolha]\n");
    }

    printf("  MEM/ER : ");
    if (e->mem_er.valido) {
        instrucao_para_asm(e->mem_er.instrucao_raw, buf);
        int op = e->mem_er.opcode;
        if (op == OP_SW || op == OP_BEQ || op == OP_JUMP)
            printf("%s  |  (sem writeback)\n", buf);
        else
            printf("%s  |  resultado=%d  rd=$%d\n", buf,
                   e->mem_er.resultado, e->mem_er.rd_dest);
    } else {
        printf("[bolha]\n");
    }
    printf("\n");
}



void imprimir_estatisticas(Estado *e) {
    printf("\n=== Estatísticas ===\n");
    printf("  Ciclos       : %d\n", e->ciclos);
    printf("  Instruções   : %d\n", e->instrucoes);
    printf("    Tipo R     : %d\n", e->qtd_tipo_r);
    printf("    addi       : %d\n", e->qtd_addi);
    printf("    lw         : %d\n", e->qtd_lw);
    printf("    sw         : %d\n", e->qtd_sw);
    printf("    beq        : %d\n", e->qtd_beq);
    printf("    jump       : %d\n", e->qtd_jump);
    printf("  Bolhas       : %d\n", e->bolhas);
    if (e->instrucoes > 0)
        printf("  CPI          : %.2f\n", (float)e->ciclos / e->instrucoes);
}
