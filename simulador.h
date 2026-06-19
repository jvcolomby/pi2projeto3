#ifndef SIMULADOR_H
#define SIMULADOR_H

#define OP_TIPO_R  0
#define OP_JUMP    2
#define OP_ADDI    4
#define OP_BEQ     8
#define OP_LW      11
#define OP_SW      15

#define FUNCT_ADD  0
#define FUNCT_SUB  2
#define FUNCT_AND  4
#define FUNCT_OR   5

typedef struct {
    int opcode;
    int rs, rt, rd;
    int funct;
    int imm;
    int addr;
} Decode;

typedef struct {
    int instrucao;
    int PC_mais1;
    int valido;
} Reg_BI_DI;

typedef struct {
    Decode c;
    int instrucao_raw;
    int A;
    int B;
    int PC_mais1;
    int valido;
} Reg_DI_EX;

typedef struct {
    int ULAout;
    int B;
    int zero;
    int rd_dest;
    int opcode;
    int addr;
    int PC_branch;
    int valido;
} Reg_EX_MEM;

typedef struct {
    int resultado;
    int rd_dest;
    int opcode;
    int valido;
} Reg_MEM_ER;

typedef struct {
    int mem_instrucoes[256];
    int mem_dados[256];
    int registradores[8];
    int PC;
    Reg_BI_DI  bi_di;
    Reg_DI_EX  di_ex;
    Reg_EX_MEM ex_mem;
    Reg_MEM_ER mem_er;
    int ciclos;
    int instrucoes;
    int qtd_tipo_r;
    int qtd_addi;
    int qtd_beq;
    int qtd_lw;
    int qtd_sw;
    int qtd_jump;
    int bolhas;
} Estado;

Decode  campos(int instrucao);
int     ULA(int A, int B, int controle, int *flag_zero);
int     controle_ULA(int opcode, int funct);
void    instrucao_para_asm(int instrucao, char *buf);
int     leitura_arquivo_mem(int mem_instrucoes[], char nome_arquivo[]);
void    inicializar_estado(Estado *e);

void    estagio_BI (Estado *e);
void    estagio_DI (Estado *e);
void    estagio_EX (Estado *e);
void    estagio_MEM(Estado *e);
void    estagio_ER (Estado *e);

void    ciclo_pipeline(Estado *e);
void    run(Estado *e, int num_instrucoes);

void    imprimir_registradores(Estado *e);
void    imprimir_pipeline(Estado *e);
void    imprimir_estatisticas(Estado *e);

#endif