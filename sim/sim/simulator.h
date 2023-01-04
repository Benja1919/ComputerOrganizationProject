#include <stdint.h>

#define DATA_MEMORY_SIZE 4096
#define MAX_ASSEMBLY_LINES 4096
#define INSTRUCTION_LINE_LEN 20
#define DATA_LINE_LEN 5 
#define INPUT_INSTR_FILE_NAME "memin.txt"
#define INPUT_DATA_FILE_NAME "dmemin.txt" //remove

#define CPU_REGS_NUM 16
#define IO_REGS_NUM 23

#define MONITOR_DIM 256

#define DISK_SECTOR_SIZE            128 //???
#define DISK_BUFFER_SIZE            DISK_SECTOR_SIZE / 4 //???
#define DISK_SECTOR_NUM 128
#define DISK_HANDLING_TIME 1024

#define MAX(X, Y)                   (((X) > (Y)) ? (X) : (Y))
#define False 0
#define True  1

typedef enum {
    ADD,
    SUB,
    MUL,
    AND,
    OR,
    XOR,
    SLL,
    SRA,
    SRL,
    BEQ,
    BNE,
    BLT,
    BGT,
    BLE,
    BGE,
    JAL,
    LW,
    SW,
    RETI,
    IN,
    OUT,
    HALT,
    OPCODES_NUM
} opcode_e;

typedef enum {
    $ZERO,
    $IMM,
    $V0,
    $A0,
    $A1,
    $A2,
    $A3,
    $T0,
    $T1,
    $T2,
    $S0,
    $S1,
    $S2,
    $GP,
    $SP,
    $RA
} cpu_reg_e;

typedef struct {
    opcode_e opcode;
    cpu_reg_e rd;
    cpu_reg_e rs;
    cpu_reg_e rt;
    int imm;
    unsigned long raw_cmd;
} asm_cmd_t;

typedef struct {
    unsigned long int data[DISK_SECTOR_NUM][DISK_SECTOR_SIZE];
    unsigned int time_in_cmd;
} disk_t;

typedef enum {
    irq0enable,
    irq1enable,
    irq2enable,
    irq0status,
    irq1status,
    irq2status,
    irqhandler,
    irqreturn,
    clks,
    leds,
    display7seg,
    timerenable,
    timercurrent,
    timermax,
    diskcmd,
    disksector,
    diskbuffer,
    diskstatus,
    reserved1,
    reserved2,
    monitoraddr,
    monitordata,
    monitorcmd
} io_reg_e;