#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "simulator.h"

static int g_cpu_regs[CPU_REGS_NUM]; /* CPU registers (32 bits each) */
static int g_io_regs[IO_REGS_NUM]; /* IO registers */
asm_cmd_t g_cmd_arr[MAX_ASSEMBLY_LINES]; /* Commands array */
static int g_in_handler = False; /* Flag indicating running in interrupt handler */
static unsigned char g_monitor[MONITOR_DIM * MONITOR_DIM]; /* Global monitor buffer */
static int g_dmem[DATA_MEMORY_SIZE]; /* Data memory */
static int g_pc = 0; /* Program counter */
static int g_is_running; /* Flag indicating program is running */
static int g_next_irq2 = -1; /* Next Irq2 cycle number */
static disk_t g_disk; /* Disk object */
static FILE* g_io_reg_trace_file; /* IO reg trace file */
static FILE* g_leds_file; /* leds file */
static FILE* g_7segment_file; /* 7 segment file */
static FILE* g_irq2in_file; /* Irq2 file */
int g_max_memory_index; /* Holds the max non empty index in data mem array */
static unsigned long long g_cycles = 0; /* Usngined 64 bit for clock cycles counter */

static const char* g_io_regs_arr[] = { "irq0enable",
                                      "irq1enable",
                                      "irq2enable",
                                      "irq0status",
                                      "irq1status",
                                      "irq2status",
                                      "irqhandler",
                                      "irqreturn",
                                      "clks",
                                      "leds",
                                      "display7seg",
                                      "timerenable",
                                      "timercurrent",
                                      "timermax",
                                      "diskcmd",
                                      "disksector",
                                      "diskbuffer",
                                      "diskstatus",
                                      "reserved1",
                                      "reserved2",
                                      "monitoraddr",
                                      "monitordata",
                                      "monitorcmd" };

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

static void update_hw_reg_trace_file(char* type, int io_reg_index, int data) {
    fprintf(g_io_reg_trace_file, "%lld %s %s %08X\n", g_cycles, type, g_io_regs_arr[io_reg_index], data);
}

static void update_leds_file() {
    fprintf(g_leds_file, "%lld %08X\n", g_cycles, g_io_regs[leds]);
}

static void update_7segment_file() {
    fprintf(g_7segment_file, "%lld %08X\n", g_cycles, g_io_regs[display7seg]);
}

/* Array of function pointers used to call the right operation
cmds_ptr_arr[opcode] is a function pointer to a function performing the
opcode */
static void (*cmds_ptr_arr[])(cpu_reg_e, cpu_reg_e, cpu_reg_e) = {
    add_cmd,
    sub_cmd,
    mul_cmd,
    and_cmd,
    or_cmd,
    xor_cmd,
    sll_cmd,
    sra_cmd,
    srl_cmd,
    beq_cmd,
    bne_cmd,
    blt_cmd,
    bgt_cmd,
    ble_cmd,
    bge_cmd,
    jal_cmd,
    lw_cmd,
    sw_cmd,
    reti_cmd,
    in_cmd,
    out_cmd,
    halt_cmd
};

/* Command function each function corresponds to an opcode*/
static void add_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    g_cpu_regs[rd] = g_cpu_regs[rs] + g_cpu_regs[rt];
}

static void sub_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    g_cpu_regs[rd] = g_cpu_regs[rs] - g_cpu_regs[rt];
}

static void mul_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    g_cpu_regs[rd] = g_cpu_regs[rs] * g_cpu_regs[rt];
}

static void and_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    g_cpu_regs[rd] = g_cpu_regs[rs] & g_cpu_regs[rt];
}

static void or_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    g_cpu_regs[rd] = g_cpu_regs[rs] | g_cpu_regs[rt];
}

static void xor_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    g_cpu_regs[rd] = g_cpu_regs[rs] ^ g_cpu_regs[rt];
}

static void sll_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Shift amount must be non negative */
    g_cpu_regs[rd] = g_cpu_regs[rs] << abs(g_cpu_regs[rt]);
}

static void sra_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Arithmetic shift with sign extension is done sutomaticly by C */
    g_cpu_regs[rd] = g_cpu_regs[rs] >> abs(g_cpu_regs[rt]);
}

static void srl_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Casting as unsigned should perform logical shift
    (adding zeros in MSB's) */
    g_cpu_regs[rd] = (unsigned)g_cpu_regs[rs] >> abs(g_cpu_regs[rt]);
}

static void beq_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (g_cpu_regs[rs] == g_cpu_regs[rt]) {
        /* Branch if equal - set global pc to 12 LSB's of rm */
        g_pc = g_cpu_regs[rd] & 0x00000FFF;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void bne_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (g_cpu_regs[rs] != g_cpu_regs[rt]) {
        /* Branch if not equal - set global pc to 12 LSB's of rm */
        g_pc = g_cpu_regs[rd] & 0x00000FFF;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void blt_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (g_cpu_regs[rs] < g_cpu_regs[rt]) {
        /* Branch if less than- set global pc to 12 LSB's of rm */
        g_pc = g_cpu_regs[rd] & 0x00000FFF;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void bgt_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (g_cpu_regs[rs] > g_cpu_regs[rt]) {
        /* Branch if greater than- set global pc to 12 LSB's of rm */
        g_pc = g_cpu_regs[rd] & 0x00000FFF;
        g_pc--;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void ble_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (g_cpu_regs[rs] <= g_cpu_regs[rt]) {
        /* Branch if less or equal than- set global pc to 12 LSB's of rm */
        g_pc = g_cpu_regs[rd] & 0x00000FFF;
        g_pc--;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void bge_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (g_cpu_regs[rs] >= g_cpu_regs[rt]) {
        /* Branch if greater or equal than- set global pc to 12 LSB's of rm */
        g_pc = g_cpu_regs[rd] & 0x00000FFF;
        g_pc--;
    }
    else {
        /* Not equal - continue */
        g_pc++;
    }
}

static void jal_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    /* Save next command address */
    g_cpu_regs[rd] = g_pc + 2;
    /* Jump */
    g_pc = g_cpu_regs[rs] & 0x00000FFF;
    g_pc--;
}

static void lw_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    g_cpu_regs[rd] = g_dmem[(g_cpu_regs[rs] + g_cpu_regs[rt]) % DATA_MEMORY_SIZE];
    //printf("%X\n", g_dmem[(g_cpu_regs[rs] + g_cpu_regs[rt]) % DATA_MEMORY_SIZE]);
}

static void sw_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    int memory_address;
    g_dmem[(g_cpu_regs[rs] + g_cpu_regs[rt]) % DATA_MEMORY_SIZE] = g_cpu_regs[rd];
    if (g_cpu_regs[rd] != 0) {
        memory_address = g_cpu_regs[rs] + g_cpu_regs[rt];
        g_max_memory_index = MAX(g_max_memory_index, memory_address);
    }
}

static void reti_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    g_in_handler = False;
    g_pc = g_io_regs[irqreturn];
    g_pc--;
    
}

static void in_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    if (rd == $IMM || rd == $ZERO) {
        return;
    }
    if (g_io_regs[g_cpu_regs[rs] + g_cpu_regs[rt]] == monitorcmd) {
        g_cpu_regs[rd] = 0;
    }
    g_cpu_regs[rd] = g_io_regs[g_cpu_regs[rs] + g_cpu_regs[rt]];
    update_hw_reg_trace_file("READ", g_cpu_regs[rs] + g_cpu_regs[rt], g_cpu_regs[rd]);
}

static void out_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    g_io_regs[g_cpu_regs[rs] + g_cpu_regs[rt]] = g_cpu_regs[rd];
    update_hw_reg_trace_file("WRITE", g_cpu_regs[rs] + g_cpu_regs[rt], g_cpu_regs[rd]);
    if (g_cpu_regs[rs] + g_cpu_regs[rt] == leds) {
        update_leds_file();
    }
    if (g_cpu_regs[rs] + g_cpu_regs[rt] == display7seg) {
        update_7segment_file();
    }
}

static void halt_cmd(cpu_reg_e rd, cpu_reg_e rs, cpu_reg_e rt) {
    g_is_running = False;
}

/* File opening and validation used for all files */
static FILE* open_and_validate_file(char const* file_name, char const* perms) {
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

static int sign_extension_imm(int imm) {
    /* Any immediate is stored in 12 bits in SIMP instruction
    This function assumes num is the shepe of 0x00000###
    Check if the 12th bit is on and extend accordingly */
    if (imm >= 0x800) { // 0x800 is 1000 0000 0000 in binary
        return imm | 0xFFFFF000;
    }
    return imm;
}

static void update_immediates(asm_cmd_t* cmd) {
    g_cpu_regs[$IMM] = sign_extension_imm(cmd->imm);
}

static int is_jump_or_branch(opcode_e opcode) {
    return (opcode >= BEQ && opcode <= JAL) || (opcode == RETI);
}

static int is_irq() {
    return (g_io_regs[irq0enable] && g_io_regs[irq0status]) ||
        (g_io_regs[irq1enable] && g_io_regs[irq1status]) ||
        (g_io_regs[irq2enable] && g_io_regs[irq2status]);
}

static void update_irq2() {
    if (g_cycles >= g_next_irq2) {
        g_io_regs[irq2status] = True;
        fscanf_s(g_irq2in_file, "%d\n", &g_next_irq2);
    }
    else {
        g_io_regs[irq2status] = False;
    }
    /*Interupt handler is responsible for setting the status back to false*/
}

static void update_clocks(asm_cmd_t* curr_cmd) {
    int tmp_opcode = (int)curr_cmd->opcode;
    int tmp_rs = (int)curr_cmd->rs;
    int tmp_rt = (int)curr_cmd->rt;
    int tmp_rd = (int)curr_cmd->rd;
    if (tmp_opcode == 16 || tmp_opcode == 17) {//lw or sw
        g_io_regs[clks] += 3; /* Updates cycle clock */
        g_cycles += 3; /* Updates cycles counter for logging */
    }
    else if (tmp_rs == 1 || tmp_rt == 1 || tmp_rd == 1) { //I format and not lw/sw
        g_io_regs[clks] += 2; /* Updates cycle clock */
        g_cycles += 2; /* Updates cycles counter for logging */
    }
    else { //R format
        g_io_regs[clks] += 1; /* Updates cycle clock */
        g_cycles += 1; /* Updates cycles counter for logging */
    }
}

static int validate_opcode_and_regs(asm_cmd_t* cmd) {
    if (cmd->opcode < 0 || cmd->opcode >= OPCODES_NUM) {
        printf("Opcode is not valid. continuing to next instruction\n");
        //printf("%s\n", cmd->opcode);
        return -1;
    }
    if (cmd->rt < 0 || cmd->rt >= CPU_REGS_NUM) {
        printf("Rt value is not valid. continuing to next instruction\n");
        return -1;
    }
    if (cmd->rs < 0 || cmd->rs >= CPU_REGS_NUM) {
        printf("Rs value is not valid. continuing to next instruction\n");
        return -1;
    }
    if (cmd->rd < 0 || cmd->rd >= CPU_REGS_NUM) {
        printf("Rd value is not valid. continuing to next instruction\n");
        return -1;
    }
    return 0;
}

//static void parse_line_to_cmd(char* line, asm_cmd_t* cmd) {
    /* Each command is 48 bits so 64 bits are required */
  //  unsigned long long raw;
  //  sscanf_s(line, "%llX", &raw);

    /* construct the command object */
  //  cmd->rt = (raw) & 0xF;
  //  cmd->rs = (raw >> 4) & 0xF;
  //  cmd->rd = (raw >> 8) & 0xF;
  //  cmd->opcode = (raw >> 12) & 0xFF;
   // if (cmd->rt == 1 || cmd->rs == 1) {
      //  cmd->imm = sscanf_s(line, "%llX", &raw);
    //}
    //cmd->raw_cmd = raw;
//}

static void exec_cmd(asm_cmd_t* cmd) {
    /* Execute a command using the functions pointers array */
    update_immediates(cmd); /* $imm1 and $imm2 should hold immediets value */
    cmds_ptr_arr[cmd->opcode](cmd->rd, cmd->rs, cmd->rt); /* Execute the command */
}

static void load_instructions(FILE* instr_file) {
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
        unsigned long long raw;
        sscanf_s(line, "%llX", &raw);
        /* construct the command object */
        cmd->rt = (raw) & 0xF;
        cmd->rs = (raw >> 4) & 0xF;
        cmd->rd = (raw >> 8) & 0xF;
        cmd->opcode = (raw >> 12) & 0xFF;
        cmd->raw_cmd = raw;
        tmp_rs = (int) cmd->rs;
        tmp_rt = (int) cmd->rt;
        tmp_rd = (int) cmd->rd;
        if (tmp_rd == 1 || tmp_rs == 1 || tmp_rt == 1) {
            fgets(line, INSTRUCTION_LINE_LEN + 2, instr_file);
            sscanf_s(line, "%llX", &raw);
            cmd->imm = (raw) & 0xFFFFF;
            //printf("%llX\n", cmd->imm);
            g_cmd_arr[instructions_count] = curr_cmd;
            instructions_count += 2;
        }
        else {
            cmd->imm = 0;
            g_cmd_arr[instructions_count] = curr_cmd;
            instructions_count++;
        }        
    }
}

static void update_trace_file(FILE* output_trace_file, asm_cmd_t* curr_cmd) {
    /* Writes the trace file for current variables status */
    fprintf(output_trace_file, "%03X %05llX 00000000 %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X\n",
        g_pc,
        curr_cmd->raw_cmd,
        sign_extension_imm(curr_cmd->imm),
        g_cpu_regs[$V0],
        g_cpu_regs[$A0],
        g_cpu_regs[$A1],
        g_cpu_regs[$A2],
        g_cpu_regs[$A3],
        g_cpu_regs[$T0],
        g_cpu_regs[$T1],
        g_cpu_regs[$T2],
        g_cpu_regs[$S0],
        g_cpu_regs[$S1],
        g_cpu_regs[$S2],
        g_cpu_regs[$GP],
        g_cpu_regs[$SP],
        g_cpu_regs[$RA]);
}

static void update_timer() {
    if (g_io_regs[timerenable] == True) {
        g_io_regs[timercurrent]++;
        if (g_io_regs[timercurrent] == g_io_regs[timermax]) {
            /* Timer is enabled and it's time to interrupt */
            g_io_regs[irq0status] = True;
            /* reset timer */
            g_io_regs[timercurrent] = 0;
        }
        /*Interupt handler is responsible for setting the status back to false*/
    }
}

static void update_monitor() {
    if (g_io_regs[monitorcmd] == True) {
        g_monitor[g_io_regs[monitoraddr]] = g_io_regs[monitordata];
        g_io_regs[monitorcmd] = False;
    }
}

static void update_disk() {
    if (g_io_regs[diskstatus] == False && (g_io_regs[diskcmd] == 1 || g_io_regs[diskcmd] == 2)) {
        /* Disk isn't busy and a command is waiting :
        get sector number and bufer address */
        unsigned int sector = g_io_regs[disksector];
        unsigned int buffer_addr = g_io_regs[diskbuffer];
        /* Mark disk as busy */
        g_io_regs[diskstatus] = True;
        if ((buffer_addr >= DATA_MEMORY_SIZE) ||
            (DATA_MEMORY_SIZE - buffer_addr < DISK_SECTOR_SIZE / 4) ||
            (sector >= DISK_SECTOR_NUM)) {
            /* If one of these conditions occurs, this is an ileagal action :
                1. buffer address is not in range
                2. buffer address is too close to end of memory and can't hold a sector from disk
                3. sector num is out of range */
            return;
        }
        switch (g_io_regs[diskcmd]) {
        case 1:
            /* Read command
            Copy from disk to memory */
            memcpy(&g_dmem[buffer_addr], g_disk.data[sector], DISK_SECTOR_SIZE);
            break;
        case 2:
            /* Write command
            Copy from memory to disk */
            memcpy(g_disk.data[sector], &g_dmem[buffer_addr], DISK_SECTOR_SIZE);
            break;
        default:
            break;
        }
    }
    else {
        if (g_io_regs[diskstatus] == True) {
            /* Disk is busy */
            g_disk.time_in_cmd++;
            if (g_disk.time_in_cmd == DISK_HANDLING_TIME) {
                /* Finished operation */
                g_io_regs[diskstatus] = False;
                g_io_regs[diskcmd] = False;
                g_io_regs[irq1status] = True;
                g_disk.time_in_cmd = 0;
            }
        }
    }
}

static void load_data_memory(FILE* data_input_file) {
    char line_buffer[DATA_LINE_LEN + 2];
    int line_count = 0;
    /* stops when either (n-1) characters are read, or /n is read
    We want to read the /n char so it won't get in to the next line */
    while (fgets(line_buffer, DATA_LINE_LEN + 2, data_input_file) != NULL) {
        sscanf_s(line_buffer, "%X", &g_dmem[line_count++]);
        //printf("%X\n", g_dmem[line_count]);
    }
    g_max_memory_index = line_count - 1;
}


static void load_disk_file(char const* file_name) {
    FILE* diskin_file = open_and_validate_file(file_name, "r");
    char line_buffer[DATA_LINE_LEN + 2];
    int line_count = 0;
    /* stops when either (n-1) characters are read, or /n is read
    We want to read the /n char so it won't get in to the next line */
    while (fgets(line_buffer, DATA_LINE_LEN + 2, diskin_file) != NULL) {
        sscanf_s(line_buffer, "%hhX", &g_disk.data[line_count / DISK_SECTOR_SIZE][line_count % DISK_SECTOR_SIZE]);
        line_count++;
    }
    fclose(diskin_file);
}

static void exec_instructions(FILE* output_trace_file) {
    g_is_running = True;
    asm_cmd_t* curr_cmd;
    int line = 0;
    int tmp_rs;
    int tmp_rt;
    int tmp_rd;
    int tmp_opcode;
    while (g_is_running) {
        if (g_in_handler == False && is_irq()) {  /* Check for interrupts */
            printf("%s\n", "entered");
            g_in_handler = True; /* Now in interrupt handler */
            g_io_regs[irqreturn] = g_pc; /* Save return address */
            //printf("%d\n", g_io_regs[irqreturn]);
            g_pc = g_io_regs[irqhandler]; /* Jump to handler */
            //printf("%d\n", g_pc);
        }
        //printf("%s\n", "before fetch, cycles:");
        //printf("%llu\n", g_cycles);
        //printf("%s\n", "before fetch, irq2status:");
        //printf("%d\n", g_io_regs[irq2status]);
        curr_cmd = &g_cmd_arr[g_pc]; /* Fetch current command to execute */
        line++;
        update_trace_file(output_trace_file, curr_cmd); /* Update trace file before executing command */
        if (validate_opcode_and_regs(curr_cmd) == 0) {
        //if (validate_opcode_and_regs(curr_cmd) == 0 && g_in_handler == True) {
            exec_cmd(curr_cmd); /* Execute only when the command is valid */
        }
        int tmp_opcode = (int)curr_cmd->opcode;
        int tmp_rs = (int)curr_cmd->rs;
        int tmp_rt = (int)curr_cmd->rt;
        int tmp_rd = (int)curr_cmd->rd;
        update_monitor(); /* Check for monitor updates */
        update_disk(); /* Check for disk updates */
        update_timer();  /* Update timer */
        //printf("%s\n", "before:");
        //printf("%llu\n", g_cycles);
        //printf("%s\n", "status:");
        //printf("%d\n", g_io_regs[irq2status]);
        update_irq2(); /* Updates value of next interrupt time if needed */
        printf("%llu\n", g_cycles);
        printf("%ld\n", g_next_irq2);
        //printf("%s\n", "after:");
        //printf("%s\n", "status:");
        //printf("%d\n", g_io_regs[irq2status]);
        update_clocks(curr_cmd);
        //printf("%llu\n", g_cycles);
        /* If the command is not branch or jump than advance PC */
        //(g_io_regs[irq2enable] && g_io_regs[irq2status])
        if (!is_jump_or_branch(curr_cmd->opcode)) {
            g_pc++;
        }
        if (tmp_rs == 1 || tmp_rt == 1 || tmp_rd == 1) {//with imm
            g_pc++;
        }
        
    }
}

static void write_memory_file(char const* file_name) {
    /* Writes the memory data file */
    FILE* output_memory_file = open_and_validate_file(file_name, "w");
    for (int i = 0; i <= g_max_memory_index; i++) {
        fprintf(output_memory_file, "%05X\n", g_dmem[i]);
    }
    fclose(output_memory_file);
}

static void write_regs_file(char const* file_name) {
    /* Writes the regs file */
    FILE* output_regs_file = open_and_validate_file(file_name, "w");
    for (int i = 3; i < CPU_REGS_NUM; i++) {
        fprintf(output_regs_file, "%08X\n", g_cpu_regs[i]);
    }
    fclose(output_regs_file);
}

static void write_cycles_file(char const* file_name) {
    FILE* output_cycles_file = open_and_validate_file(file_name, "w");
    fprintf(output_cycles_file, "%lld", g_cycles);
    fclose(output_cycles_file);
}

static void write_disk_file(char const* file_name) {
    /* Writes the disk data file */
    FILE* output_disk_file = open_and_validate_file(file_name, "w");
    for (int i = 0; i < DISK_SECTOR_NUM; i++) {
        for (int j = 0; j < DISK_SECTOR_SIZE; j++) {
            fprintf(output_disk_file, "%08X\n", (g_disk.data)[i][j]);
        }
    }
    fclose(output_disk_file);
}

static void write_monitor_files(char const* file_txt_name, char const* file_yuv_name) {
    /* Writes the monitor data file */
    FILE* output_monitor_data_file = open_and_validate_file(file_txt_name, "w");
    for (int i = 0; i < MONITOR_DIM * MONITOR_DIM; i++) {
        fprintf(output_monitor_data_file, "%02X\n", (g_monitor)[i]);
    }
    fclose(output_monitor_data_file);

    /* Writes the monitor binary file */
    FILE* output_monitor_binary_file = open_and_validate_file(file_yuv_name, "wb");
    fwrite(g_monitor, sizeof(char), MONITOR_DIM * MONITOR_DIM, output_monitor_binary_file);
    fclose(output_monitor_binary_file);
}

int main(int argc, char const* argv[])
{
    FILE* input_cmd_file = open_and_validate_file(argv[1], "r"); /* memin.txt */
    FILE* output_trace_file = open_and_validate_file(argv[6], "w"); /* trace.txt */
    g_io_reg_trace_file = open_and_validate_file(argv[7], "w"); /* IO reg trace file */
    g_leds_file = open_and_validate_file(argv[9], "w"); /* leds file */
    g_7segment_file = open_and_validate_file(argv[10], "w"); /* 7segment file */
    g_irq2in_file = open_and_validate_file(argv[3], "r"); /* Irq2 file */
    fscanf_s(g_irq2in_file, "%d\n", &g_next_irq2);
  //  printf("%d", g_next_irq2);

    load_instructions(input_cmd_file); /* Load instructions and store them in g_cmd_arr */
    fclose(input_cmd_file);
    input_cmd_file = open_and_validate_file(argv[1], "r"); /* memin.txt */
    load_data_memory(input_cmd_file);
    load_disk_file(argv[2]); /* Load disk file */
    exec_instructions(output_trace_file); /* Execute program */
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
}
