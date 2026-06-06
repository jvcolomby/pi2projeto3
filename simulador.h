#ifndef SIMULADOR_H
#define SIMULADOR_H

// Opcodes
#define OP_TIPO_R  0
#define OP_JUMP    2
#define OP_ADDI    4
#define OP_BEQ     8
#define OP_LW      11
#define OP_SW      15

// Funct tipo R
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

// ─── Registradores de pipeline ────────────────────────────────────────────────

// Entre BI e DI: carrega instrução bruta e PC incrementado
typedef struct {
    int instrucao;
    int PC_mais1;   // PC + 1 (próxima instrução)
    int valido;     // 0 = bolha (NOP)
} Reg_BI_DI;

// Entre DI e EX: campos decodificados + valores lidos do banco
typedef struct {
    Decode c;
    int A;          // registradores[rs]
    int B;          // registradores[rt]
    int PC_mais1;
    int valido;
} Reg_DI_EX;

// Entre EX e MEM: resultado da ULA, dado a escrever, destino
typedef struct {
    int ULAout;
    int B;          // valor de rt (usado pelo SW)
    int zero;       // flag zero da ULA (usado pelo BEQ)
    int rd_dest;    // registrador destino (rd ou rt)
    int opcode;
    int addr;       // endereço do JUMP
    int PC_branch;  // PC alvo do BEQ
    int valido;
} Reg_EX_MEM;

// Entre MEM e ER: dado da memória ou ULA a ser escrito
typedef struct {
    int resultado;  // ULAout ou RDM (dado lido da memória)
    int rd_dest;
    int opcode;
    int valido;
} Reg_MEM_ER;

// ─── Estado completo do pipeline ──────────────────────────────────────────────
typedef struct {
    int memoria[256];
    int registradores[8];
    int PC;
    Reg_BI_DI  bi_di;
    Reg_DI_EX  di_ex;
    Reg_EX_MEM ex_mem;
    Reg_MEM_ER mem_er;
    // Estatísticas
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

// ─── Funções utilitárias ──────────────────────────────────────────────────────
Decode    campos(int instrucao);
int       ULA(int A, int B, int controle, int *flag_zero);
int       controle_ULA(int opcode, int funct);
void      instrucao_para_asm(int instrucao, char *buf);
int       leitura_arquivo_mem(int memoria[], char nome_arquivo[]);
void      inicializar_estado(Estado *e);

// ─── Estágios do pipeline ─────────────────────────────────────────────────────
void estagio_BI (Estado *e);
void estagio_DI (Estado *e);
void estagio_EX (Estado *e);
void estagio_MEM(Estado *e);
void estagio_ER (Estado *e);

// ─── Execução ─────────────────────────────────────────────────────────────────
void ciclo_pipeline(Estado *e);
void run(Estado *e, int num_instrucoes);

// ─── Impressão ────────────────────────────────────────────────────────────────
void imprimir_registradores(Estado *e);
void imprimir_pipeline(Estado *e);
void imprimir_estatisticas(Estado *e);

#endif
