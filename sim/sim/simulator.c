/*********************************Some Explainations about the code***************************************/
/*
                                     id 208685784, 315416057
                In this file we implemented a
                                                                                                        */
                                                                                                        /********************************************************************************************************/

                                                                                                        /*********************************************Includes****************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "simulator.h"
/*********************************************************************************************************/

/*****************************************Consts, Statics & Arrays****************************************/
static int cpu_registers[CPU_REGS_NUM]; /* CPU registers (32 bits each) */
static int io_registers[IO_REGS_NUM]; /* IO registers */
asm_cmd_t commands_array[MAX_ASSEMBLY_LINES]; /* Commands array */
static int g_in_handler = False; /* Flag indicating running in interrupt handler */
static int reti_imm = False; /* Flag indicating running in interrupt handler */
static unsigned char g_monitor[MONITOR_DIM * MONITOR_DIM]; /* Global monitor buffer */
static int memory_array[DATA_MEMORY_SIZE]; /* Data memory */
static int g_pc = 0; /* Program counter */
static int g_is_running; /* Flag indicating program is running */
static int g_next_irq2 = -1; /* Next Irq2 cycle number */
static disk_t g_disk; /* Disk object */
static FILE* g_io_reg_trace_file; /* IO reg trace file */
static FILE* g_leds_file; /* leds file */
static FILE* g_7segment_file; /* 7 segment file */
static FILE* g_irq2in_file; /* Irq2 file */
int g_max_memory_index; /* Holds the max non empty index in data mem array */
static unsigned long g_cycles = 0; /* Usngined 32 bit for clock cycles counter */

static const char* io_registers_arr[] = { "irq0enable","irq1enable","irq2enable","irq0status","irq1status","irq2status","irqhandler",
                                      "irqreturn","clks","leds","display7seg","timerenable","timercurrent","timermax","diskcmd",
                                      "disksector","diskbuffer","diskstatus","reserved1","reserved2","monitoraddr","monitordata",
                                      "monitorcmd" };

/*********************************************************************************************************/

/*****************************************Functions declarations******************************************/
static void add_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void sub_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void mul_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void and_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void or_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void xor_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void sll_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void sra_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void srl_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void beq_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void bne_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void blt_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void bgt_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void ble_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void bge_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void jal_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void lw_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void sw_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void reti_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void in_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void out_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);
static void halt_cmd(cpu_reg_e, cpu_reg_e, cpu_reg_e);

static void update_hw_reg_trace_file(char* type, int io_reg_index, int data);
static void update_leds_file();
static void update_7segment_file();
static int immediate_sign_extension(int imm);
static void update_immediates(asm_cmd_t* cmd);
static int is_jump_or_branch(opcode_e opcode);
static int is_irq();
static void update_irq2();
static void update_clocks_before(asm_cmd_t* curr_cmd);
static void update_clocks_after(asm_cmd_t* curr_cmd);
static int registers_and_opcode_validation(asm_cmd_t* cmd);
static void command_execution(asm_cmd_t* cmd);
static void load_instruction_file(FILE* instr_file);
static void update_trace_file(FILE* output_trace_file, asm_cmd_t* curr_cmd);
static void update_timer();
static void update_monitor();
static void update_disk();
static void load_memory(FILE* data_input_file);
static void load_disk_file(char const* file_name);
static void execute_instructions(FILE* output_trace_file);
static void write_memory_file(char const* file_name);
static void write_regs_file(char const* file_name);
static void write_cycles_file(char const* file_name);
static void write_disk_file(char const* file_name);
static void write_monitor_files(char const* file_txt_name, char const* file_yuv_name);

/*********************************************************************************************************/


static void update_hw_reg_trace_file(char* type, int io_reg_index, int data) {
    fprintf(g_io_reg_trace_file, "%ld %s %s %08X\n", g_cycles - 1, type, io_registers_arr[io_reg_index], data);
}

static void update_leds_file() {
    fprintf(g_leds_file, "%ld %08X\n", g_cycles - 1, io_registers[leds]);
}

static void update_7segment_file() {
    fprintf(g_7segment_file, "%ld %08X\n", g_cycles - 1, io_registers[display7seg]);
}

/* Array of function pointers used to call the right operation
cmds_ptr_arr[opcode] is a function pointer to a function performing the
opcode */
static void (*cmds_ptr_arr[])(cpu_reg_e, cpu_reg_e, cpu_reg_e) = {
    add_cmd,sub_cmd,mul_cmd,and_cmd,or_cmd,xor_cmd,sll_cmd,sra_cmd,srl_cmd,beq_cmd,bne_cmd,blt_cmd,bgt_cmd,
    ble_cmd,bge_cmd,jal_cmd,lw_cmd,sw_cmd,reti_cmd,in_cmd,out_cmd,halt_cmd };

/* Command function each function corresponds to an opcode*/
static void add_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO)
    {
        return;
    }
    cpu_registers[rd] = cpu_registers[rs] + cpu_registers[rt];
}

static void sub_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO)
    {
        return;
    }
    cpu_registers[rd] = cpu_registers[rs] - cpu_registers[rt];
}

static void mul_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    cpu_registers[rd] = cpu_registers[rs] * cpu_registers[rt];
}

static void and_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    cpu_registers[rd] = cpu_registers[rs] & cpu_registers[rt];
}

static void or_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    cpu_registers[rd] = cpu_registers[rs] | cpu_registers[rt];
}

static void xor_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    cpu_registers[rd] = cpu_registers[rs] ^ cpu_registers[rt];
}

static void sll_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Shift amount must be non negative */
    cpu_registers[rd] = cpu_registers[rs] << abs(cpu_registers[rt]);
}

static void sra_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Arithmetic shift with sign extension is done sutomaticly by C */
    cpu_registers[rd] = cpu_registers[rs] >> abs(cpu_registers[rt]);
}

static void srl_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Casting as unsigned should perform logical shift
    (adding zeros in MSB's) */
    cpu_registers[rd] = (unsigned)cpu_registers[rs] >> abs(cpu_registers[rt]);
}

static void beq_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (cpu_registers[rs] == cpu_registers[rt]) {
        /* Branch if equal - set global pc to 12 LSB's of rm */
        g_pc = cpu_registers[rd] & 0x00000FFF;
        //g_pc--;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void bne_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (cpu_registers[rs] != cpu_registers[rt]) {
        /* Branch if not equal - set global pc to 12 LSB's of rm */
        g_pc = cpu_registers[rd] & 0x00000FFF;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void blt_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (cpu_registers[rs] < cpu_registers[rt]) {
        /* Branch if less than- set global pc to 12 LSB's of rm */
        g_pc = cpu_registers[rd] & 0x00000FFF;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void bgt_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (cpu_registers[rs] > cpu_registers[rt]) {
        /* Branch if greater than- set global pc to 12 LSB's of rm */
        g_pc = cpu_registers[rd] & 0x00000FFF;
        g_pc--;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void ble_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (cpu_registers[rs] <= cpu_registers[rt]) {
        /* Branch if less or equal than- set global pc to 12 LSB's of rm */
        g_pc = cpu_registers[rd] & 0x00000FFF;
        g_pc--;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void bge_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (cpu_registers[rs] >= cpu_registers[rt]) {
        /* Branch if greater or equal than- set global pc to 12 LSB's of rm */
        g_pc = cpu_registers[rd] & 0x00000FFF;
        g_pc--;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void jal_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Save next command address */
    cpu_registers[rd] = g_pc + 2;
    /* Jump */
    g_pc = cpu_registers[rs] & 0x00000FFF;
    g_pc--;
}

static void lw_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    cpu_registers[rd] = memory_array[(cpu_registers[rs] + cpu_registers[rt]) % DATA_MEMORY_SIZE];
}

static void sw_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    int memory_address;
    memory_array[(cpu_registers[rs] + cpu_registers[rt]) % DATA_MEMORY_SIZE] = cpu_registers[rd];
    if (cpu_registers[rd] != 0) {
        memory_address = cpu_registers[rs] + cpu_registers[rt];
        g_max_memory_index = MAX(g_max_memory_index, memory_address);
    }
}

static void reti_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    g_in_handler = False;
    g_pc = io_registers[irqreturn] + 1;
    if (reti_imm) {//with imm
        g_pc++;
    }
    reti_imm = False;
}

static void in_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    if (io_registers[cpu_registers[rs] + cpu_registers[rt]] == monitorcmd) {
        cpu_registers[rd] = 0;
    }
    cpu_registers[rd] = io_registers[cpu_registers[rs] + cpu_registers[rt]];
    update_hw_reg_trace_file("READ", cpu_registers[rs] + cpu_registers[rt], cpu_registers[rd]);
}

static void out_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    io_registers[cpu_registers[rs] + cpu_registers[rt]] = cpu_registers[rd];
    update_hw_reg_trace_file("WRITE", cpu_registers[rs] + cpu_registers[rt], cpu_registers[rd]);
    if (cpu_registers[rs] + cpu_registers[rt] == leds) {
        update_leds_file();
    }
    if (cpu_registers[rs] + cpu_registers[rt] == display7seg) {
        update_7segment_file();
    }
}

static void halt_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt)
{
    g_is_running = False;
}

/* File opening and validation used for all files */
static FILE* file_validation(char const* file_name, char const* perms) {
    FILE* file;
    fopen_s(&file, file_name, perms);
    if (file == NULL) {
        printf("Error opening file %s\n", file_name);
        exit(0);
    }
    else {
        return file;
    }
}

static int immediate_sign_extension(int imm) {
    /* Any immediate is stored in 20 bits in SIMP instruction
    This function assumes num is the shepe of 0x00000###
    Check if the 19th bit is on and extend accordingly */
    if (imm > 0x80000) { // 0x80000 is 1000 0000 0000 0000 0000 in binary
        return imm | 0xFFFFF000;
    }
    return imm;
}

static void update_immediates(asm_cmd_t* cmd) {
    cpu_registers[$IMM] = immediate_sign_extension(cmd->imm);
}

static int is_jump_or_branch(opcode_e opcode) {
    return (opcode >= BEQ && opcode <= JAL) || (opcode == RETI);
}

static int is_irq() {
    return (io_registers[irq0enable] && io_registers[irq0status]) ||
        (io_registers[irq1enable] && io_registers[irq1status]) ||
        (io_registers[irq2enable] && io_registers[irq2status]);
}

static void update_irq2() {
    int tmp_irq2 = g_next_irq2;
    if ((int)g_cycles > g_next_irq2) {
        io_registers[irq2status] = True;
        fscanf_s(g_irq2in_file, "%d\n", &g_next_irq2);
        if (tmp_irq2 == g_next_irq2) { //no new line so no need to irq2
            g_next_irq2 = INT_MAX; //so no other irq2 will occur
        }
    }
    else {
        io_registers[irq2status] = False;
    }
    /*Interupt handler is responsible for setting the status back to false*/
}

static void update_clocks_before(asm_cmd_t* curr_cmd) {
    int tmp_rs = (int)curr_cmd->rs;
    int tmp_rt = (int)curr_cmd->rt;
    int tmp_rd = (int)curr_cmd->rd;
    if (tmp_rs == 1 || tmp_rt == 1 || tmp_rd == 1) { //I format and not lw/sw
        io_registers[clks] += 2; /* Updates cycle clock */
        g_cycles += 2; /* Updates cycles counter for logging */
        io_registers[timercurrent] += 2;
    }
    else { //R format
        io_registers[clks] += 1; /* Updates cycle clock */
        g_cycles += 1; /* Updates cycles counter for logging */
        io_registers[timercurrent] += 1;
    }

}

static void update_clocks_after(asm_cmd_t* curr_cmd) {
    int tmp_opcode = (int)curr_cmd->opcode;
    if (tmp_opcode == 16 || tmp_opcode == 17) {//lw or sw
        io_registers[clks] += 1; /* Updates cycle clock */
        g_cycles += 1; /* Updates cycles counter for logging */
        io_registers[timercurrent] += 1;
    }
}

static int registers_and_opcode_validation(asm_cmd_t* cmd) {
    if (cmd->opcode < 0 || cmd->opcode >= OPCODES_NUM) {
        printf("%d\n", (int)cmd ->opcode);
        printf("Opcode is not valid. continuing to next instruction\n");
        return -1;
    }
    if (cmd->rt < 0 || cmd->rt >= CPU_REGS_NUM) {
        printf("Rt value is not valid. continuing to next instruction\n");
        return -1;
    }
    if (cmd->rs < 0 || cmd->rs >= CPU_REGS_NUM) {
        printf("%d\n", (int)cmd->rs);
        printf("Rs value is not valid. continuing to next instruction\n");
        return -1;
    }
    if (cmd->rd < 0 || cmd->rd >= CPU_REGS_NUM) {
        printf("Rd value is not valid. continuing to next instruction\n");
        return -1;
    }
    return 0;
}

static void command_execution(asm_cmd_t* cmd) {
    /* Execute a command using the functions pointers array */
    update_immediates(cmd); /* $imm1 and $imm2 should hold immediets value */
    cmds_ptr_arr[cmd->opcode](cmd->rd, cmd->rs, cmd->rt); /* Execute the command */
}

static void load_instruction_file(FILE* instr_file) {
    char line[INSTRUCTION_LINE_LEN + 2];
    int instructions_count = 0;
    asm_cmd_t* cmd;
    asm_cmd_t curr_cmd;
    int tmp_rs;
    int tmp_rt;
    int tmp_rd;
    /* stops when either (n-1) characters are read, or /n is read
    We want to read the /n char so it won't get in to the next line */
    while (fgets(line, INSTRUCTION_LINE_LEN + 2, instr_file) != NULL) {
        cmd = &curr_cmd;
        unsigned long raw;
        sscanf_s(line, "%lX", &raw);
        /* construct the command object */
        cmd->rt = (raw) & 0xF;
        cmd->rs = (raw >> 4) & 0xF;
        cmd->rd = (raw >> 8) & 0xF;
        cmd->opcode = (raw >> 12) & 0xFF;
        cmd->raw_cmd = raw;
        tmp_rs = (int)cmd->rs;
        tmp_rt = (int)cmd->rt;
        tmp_rd = (int)cmd->rd;
        if (tmp_rd == 1 || tmp_rs == 1 || tmp_rt == 1) {
            fgets(line, INSTRUCTION_LINE_LEN + 2, instr_file);
            sscanf_s(line, "%lX", &raw);
            cmd->imm = (raw) & 0xFFFFF;
            commands_array[instructions_count] = curr_cmd;
            instructions_count += 2;
        }
        else {
            cmd->imm = 0;
            commands_array[instructions_count] = curr_cmd;
            instructions_count++;
        }
    }
}

static void update_trace_file(FILE* output_trace_file, asm_cmd_t* curr_cmd) {
    /* Writes the trace file for current variables status */
    fprintf(output_trace_file, "%03X %05lX 00000000 %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X\n",
        g_pc,
        curr_cmd->raw_cmd,
        immediate_sign_extension(curr_cmd->imm),
        cpu_registers[$V0],
        cpu_registers[$A0],
        cpu_registers[$A1],
        cpu_registers[$A2],
        cpu_registers[$A3],
        cpu_registers[$T0],
        cpu_registers[$T1],
        cpu_registers[$T2],
        cpu_registers[$S0],
        cpu_registers[$S1],
        cpu_registers[$S2],
        cpu_registers[$GP],
        cpu_registers[$SP],
        cpu_registers[$RA]);
}

static void update_timer() {
    if (io_registers[timerenable] == True) {
        if (io_registers[timercurrent] == io_registers[timermax]) {
            /*interrupt */
            io_registers[irq0status] = True;
            io_registers[timercurrent] = 0;
        }
    }
}

static void update_monitor() {
    if (io_registers[monitorcmd] == True) {
        g_monitor[io_registers[monitoraddr]] = io_registers[monitordata];
        io_registers[monitorcmd] = False;
    }
}

static void update_disk() {
    if (io_registers[diskstatus] == False && (io_registers[diskcmd] == 1 || io_registers[diskcmd] == 2)) {
        /* Disk isn't busy and a command is waiting :
        get sector number and bufer address */
        unsigned int sector = io_registers[disksector];
        unsigned int buffer_addr = io_registers[diskbuffer];
        /* Mark disk as busy */
        io_registers[diskstatus] = True;
        if ((buffer_addr >= DATA_MEMORY_SIZE) ||
            (DATA_MEMORY_SIZE - buffer_addr < DISK_SECTOR_SIZE / 4) ||
            (sector >= DISK_SECTOR_NUM)) {
            /* If one of these conditions occurs, this is an ileagal action :
                1. buffer address is not in range
                2. buffer address is too close to end of memory and can't hold a sector from disk
                3. sector num is out of range */
            return;
        }
        switch (io_registers[diskcmd]) {
        case 1:
            /* Read command
            Copy from disk to memory */
            printf("%d\n", g_disk.data[sector][2]);
            memcpy(&memory_array[buffer_addr], g_disk.data[sector], DISK_SECTOR_SIZE);
            printf("%d\n", memory_array[2]);
            break;
        case 2:
            /* Write command
            Copy from memory to disk */
            memcpy(g_disk.data[sector], &memory_array[buffer_addr], DISK_SECTOR_SIZE);
            break;
        default:
            break;
        }
    }
    else {
        if (io_registers[diskstatus] == True) {
            /* Disk is busy */
            g_disk.time_in_cmd++;
            if (g_disk.time_in_cmd == DISK_HANDLING_TIME) {
                /* Finished operation */
                io_registers[diskstatus] = False;
                io_registers[diskcmd] = False;
                io_registers[irq1status] = True;
                g_disk.time_in_cmd = 0;
            }
        }
    }
}

static void load_memory(FILE* data_input_file) {
    char line_buffer[DATA_LINE_LEN + 2];
    int line_count = 0;
    /* stops when either (n-1) characters are read, or /n is read
    We want to read the /n char so it won't get in to the next line */
    while (fgets(line_buffer, DATA_LINE_LEN + 2, data_input_file) != NULL) {
        sscanf_s(line_buffer, "%X", &memory_array[line_count++]);
    }
    g_max_memory_index = line_count - 1;
}


static void load_disk_file(char const* file_name) {
    FILE* diskin_file = file_validation(file_name, "r");
    char line_buffer[DATA_LINE_LEN + 2];
    int line_count = 0;
    /* stops when either (n-1) characters are read, or /n is read
    We want to read the /n char so it won't get in to the next line */
    while (fgets(line_buffer, DATA_LINE_LEN + 2, diskin_file) != NULL) {
        sscanf_s(line_buffer, "%lX", &g_disk.data[line_count / DISK_SECTOR_SIZE][line_count % DISK_SECTOR_SIZE]);
        line_count++;
    }
    fclose(diskin_file);
}

static void execute_instructions(FILE* output_trace_file) {
    g_is_running = True;
    asm_cmd_t* curr_cmd;
    int tmp_rs = 0;
    int tmp_rt = 0;
    int tmp_rd = 0;
    int tmp_opcode = 0;
    while (g_is_running) {
        curr_cmd = &commands_array[g_pc]; /* Fetch current command to execute */
        int tmp_opcode = (int)curr_cmd->opcode;
        int tmp_rs = (int)curr_cmd->rs;
        int tmp_rt = (int)curr_cmd->rt;
        int tmp_rd = (int)curr_cmd->rd;
        /* Update trace file before executing command */
        if (registers_and_opcode_validation(curr_cmd) == 0) {
            update_clocks_before(curr_cmd);
            update_trace_file(output_trace_file, curr_cmd);
            command_execution(curr_cmd); /* Execute only when the command is valid */
            update_clocks_after(curr_cmd);
        }
        update_monitor(); /* Check for monitor updates */
        update_disk(); /* Check for disk updates */
        update_timer();  /* Update timer */
        update_irq2(); /* Updates value of next interrupt time if needed */
        if (g_in_handler == False && is_irq()) {  /* Check for interrupts */
            g_in_handler = True; /* Now in interrupt handler */
            if (is_jump_or_branch(curr_cmd->opcode)) {
                io_registers[irqreturn] = g_pc - 1; /* Save return address */
            }
            else {
                io_registers[irqreturn] = g_pc;
            }
            if (tmp_rs == 1 || tmp_rt == 1 || tmp_rd == 1) {
                reti_imm = True;
            }
            g_pc = io_registers[irqhandler]; /* Jump to handler */
            if (!is_jump_or_branch(curr_cmd->opcode)) {
                g_pc -= 2;
            }
            else {
                g_pc -= 1;
            }
        }
        if (!is_jump_or_branch(curr_cmd->opcode)) {
            g_pc++;
        }
        if ((tmp_rs == 1 || tmp_rt == 1 || tmp_rd == 1) && tmp_opcode != 9) {//with imm
            g_pc++;
        }
    }
}

static void write_memory_file(char const* file_name) {
    /* Writes the memory data file */
    FILE* output_memory_file = file_validation(file_name, "w");
    for (int i = 0; i <= g_max_memory_index; i++) {
        fprintf(output_memory_file, "%05X\n", memory_array[i]);
    }
    fclose(output_memory_file);
}

static void write_regs_file(char const* file_name) {
    /* Writes the regs file */
    FILE* output_regs_file = file_validation(file_name, "w");
    for (int i = 2; i < CPU_REGS_NUM; i++) {
        fprintf(output_regs_file, "%08X\n", cpu_registers[i]);
    }
    fclose(output_regs_file);
}

static void write_cycles_file(char const* file_name) {
    FILE* output_cycles_file = file_validation(file_name, "w");
    fprintf(output_cycles_file, "%ld", g_cycles);
    fclose(output_cycles_file);
}

static void write_disk_file(char const* file_name) {
    int t = 0;
    /* Writes the disk data file */
    FILE* output_disk_file = file_validation(file_name, "w");
    for (int i = 0; i < DISK_SECTOR_NUM; i++) {
        for (int j = 0; j < DISK_SECTOR_SIZE; j++) {
            fprintf(output_disk_file, "%05X\n", (int)(g_disk.data)[i][j]);
        }
    }
    fclose(output_disk_file);
}

static void write_monitor_files(char const* file_txt_name, char const* file_yuv_name) {
    /* Writes the monitor data file */
    FILE* output_monitor_data_file = file_validation(file_txt_name, "w");
    for (int i = 0; i < MONITOR_DIM * MONITOR_DIM; i++) {
        fprintf(output_monitor_data_file, "%02X\n", (g_monitor)[i]);
    }
    fclose(output_monitor_data_file);

    /* Writes the monitor binary file */
    FILE* output_monitor_binary_file = file_validation(file_yuv_name, "wb");
    fwrite(g_monitor, sizeof(char), MONITOR_DIM * MONITOR_DIM, output_monitor_binary_file);
    fclose(output_monitor_binary_file);
}

/*********************************************Main******************************************************/
int main(int argc, char const* argv[])
{
    FILE* input_cmd_file = file_validation(argv[1], "r"); /* memin.txt */
    FILE* output_trace_file = file_validation(argv[6], "w"); /* trace.txt */
    g_io_reg_trace_file = file_validation(argv[7], "w"); /* IO reg trace file */
    g_leds_file = file_validation(argv[9], "w"); /* leds file */
    g_7segment_file = file_validation(argv[10], "w"); /* 7segment file */
    g_irq2in_file = file_validation(argv[3], "r"); /* Irq2 file */
    fscanf_s(g_irq2in_file, "%d\n", &g_next_irq2);
    load_instruction_file(input_cmd_file); /* Load instructions and store them in commands_array */
    fclose(input_cmd_file);
    input_cmd_file = file_validation(argv[1], "r"); /* memin.txt */
    load_memory(input_cmd_file);
    load_disk_file(argv[2]); /* Load disk file */
    execute_instructions(output_trace_file); /* Execute program */
    write_memory_file(argv[4]); /* Write memout file with the update memory */
    write_regs_file(argv[5]); /* Update regout file with the update regs values */
    write_cycles_file(argv[8]); /* Write to cycles file */
    write_disk_file(argv[11]); /* Write to diskout file */
    write_monitor_files(argv[12], argv[13]); /* Write monitor files */

    fclose(input_cmd_file);
    fclose(output_trace_file);
    fclose(g_io_reg_trace_file);
    fclose(g_leds_file);
    fclose(g_7segment_file);
    fclose(g_irq2in_file);
    return 0;
    /********************************************************************************************************/
}