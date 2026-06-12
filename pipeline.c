#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulador.h"

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
    *flag_zero = (resultado == 0);
    if (resultado > 127 || resultado < -128)
        printf("[ULA] Overflow detectado.\n");
    return resultado;
}

void instrucao_para_asm(int instrucao, char *buf) {
    Decode c = campos(instrucao);
    if (instrucao == 0) { sprintf(buf, "nop"); return; }
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

int leitura_arquivo_mem(int memoria[], char nome_arquivo[]) {
    FILE *arquivo = fopen(nome_arquivo, "r");
    if (!arquivo) { printf("Erro ao abrir %s\n", nome_arquivo); return 0; }
    char linha[200];
    int i = 0;
    while (fgets(linha, sizeof(linha), arquivo) && i < 256) {
        char *com = strstr(linha, "//");
        if (com) *com = '\0';
        linha[strcspn(linha, "\r\n")] = '\0';
        char *tok = strtok(linha, " \t");
        if (!tok || tok[0] == '\0') continue;
        memoria[i++] = (int)strtol(tok, NULL, 2);
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
}

void estagio_BI(Estado *e) {
    if (e->PC >= 256) {
        e->bi_di.valido = 0;
        return;
    }
    e->bi_di.instrucao = e->memoria[e->PC];
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
    e->di_ex.c             = c;
    e->di_ex.instrucao_raw = e->bi_di.instrucao;
    e->di_ex.A             = e->registradores[c.rs];
    e->di_ex.B             = e->registradores[c.rt];
    e->di_ex.PC_mais1      = e->bi_di.PC_mais1;
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
    int ctrl   = controle_ULA(c.opcode, c.funct);
    int op2    = (c.opcode == OP_TIPO_R) ? B : c.imm;
    int result = ULA(A, op2, ctrl, &flag);

    e->ex_mem.ULAout    = result;
    e->ex_mem.B         = B;
    e->ex_mem.zero      = flag;
    e->ex_mem.opcode    = c.opcode;
    e->ex_mem.addr      = c.addr;
    e->ex_mem.PC_branch = e->di_ex.PC_mais1 + c.imm;
    e->ex_mem.rd_dest   = (c.opcode == OP_TIPO_R) ? c.rd : c.rt;
    e->ex_mem.valido    = 1;
}

void estagio_MEM(Estado *e) {
    if (!e->ex_mem.valido) {
        e->mem_er.valido = 0;
        return;
    }
    int opcode    = e->ex_mem.opcode;
    int endereco  = e->ex_mem.ULAout;
    int resultado = e->ex_mem.ULAout;

    switch (opcode) {
        case OP_LW:
            if (endereco >= 0 && endereco < 256)
                resultado = e->memoria[endereco];
            else
                printf("[MEM] Erro: endereço LW fora dos limites (%d)\n", endereco);
            break;
        case OP_SW:
            if (endereco >= 0 && endereco < 256)
                e->memoria[endereco] = e->ex_mem.B;
            else
                printf("[MEM] Erro: endereço SW fora dos limites (%d)\n", endereco);
            break;
        case OP_BEQ:
            if (e->ex_mem.zero) {
                e->PC = e->ex_mem.PC_branch;
                e->bi_di.valido = 0;
                e->di_ex.valido = 0;
                e->bolhas += 2;
            }
            break;
        case OP_JUMP:
            e->PC = e->ex_mem.addr;
            e->bi_di.valido = 0;
            e->di_ex.valido = 0;
            e->bolhas += 2;
            break;
    }

    e->mem_er.resultado = resultado;
    e->mem_er.rd_dest   = e->ex_mem.rd_dest;
    e->mem_er.opcode    = opcode;
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

    if (opcode == OP_SW || opcode == OP_BEQ || opcode == OP_JUMP) {
        e->mem_er.valido = 0;
        return;
    }

    if (e->mem_er.rd_dest != 0)
        e->registradores[e->mem_er.rd_dest] = e->mem_er.resultado;

    e->mem_er.valido = 0;
}
