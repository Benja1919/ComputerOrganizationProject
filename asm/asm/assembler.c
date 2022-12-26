/*********************************Some Explainations about the code***************************************/
/*
                                     id 208685784, 315416057
                In this file we implemented an assembler whose purpose is to take the asm files,
                process them and output the memin file that will be used as input to the simulator.
                Therefore, in the assembler.h file we have implemented a struct suitable for labels
                and in addition various functions that go over the asm file. The purpose of each
                function is detailed below.
                                                                                                        */
                                                                                                        /********************************************************************************************************/

                                                                                                        /*********************************************Includes****************************************************/
#include "assembler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
/*********************************************************************************************************/



/*****************************************Consts, Statics & Arrays****************************************/
static int label_counter = 0; /*counter for labels */
static int num_of_lines = 0;
int word_index = 0; /*for words_array */
static int commands_counter = 0; /*counter for commands */
label_temp labels_array[MAX_LINES]; /* Stores all the labels */
int words_array[MAX_LINES]; /* Stores all the '.word' commands */

static const char* opcodes[] = { "add", "sub", "mul", "and", "or", "xor", "sll", "sra", "srl", "beq", "bne", "blt", "bgt", "ble",
                                "bge", "jal", "lw", "sw", "reti", "in", "out", "halt" };
static const char* registers[] = { "zero", "imm", "v0", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "s0", "s1", "s2", "gp", "sp", "ra" };
/*********************************************************************************************************/


/*****************************************Functions declarations******************************************/
static int num_of_opcode(char* opcode_name);
static int num_of_register(char* register_name);
static int num_of_label(char* label);
static int check_if_label(char* imm);
static void remove_spaces(char** line);
static int check_if_comment(char* line);
static int check_line_with_label(char* line);
static int line_has_command(char* line);
static int check_if_word(char* line);
static void command_to_output(FILE* output_file, char* line);
static void add_word_to_array(char* line);
static void word_to_output(FILE* output_data_file);
static void file_flow(int pass_num, FILE* assembly_prog, FILE* output_file);
/*********************************************************************************************************/

static int num_of_opcode(char* opcode_name)
{
    /*returns opcode's num or -1 for error*/
    int i = 0;
    while (i <= 21)
    {
        if (strcmp(opcode_name, opcodes[i]) == 0)
        {
            return i;
        }
        i++;
    }
    return -1;
}

static int num_of_register(char* register_name)
{
    /*returns register's num or -1 for error*/
    int i = 0;
    while (i <= 15)
    {
        if (strcmp(register_name, registers[i]) == 0)
        {
            return i;
        }
        i++;
    }
    return -1;
}

static int num_of_label(char* label)
{
    /*returns lanel's num or -1 for error*/
    int i = 0;
    while (i < label_counter)
    {
        if (strcmp(label, labels_array[i].label) == 0)
        {
            return labels_array[i].index;
        }
        i++;
    }
    return -1;
}

// static int does_line_contain_label(char *line) {
//     for (int i = 0; i < MAX_LABEL_LEN; i++) {
//         if (line[i] == ':') { 
//             return 1;
//         }
//         if (line[i] == '\0') {
//             return 0;
//         }
//     }
//     return 0;
// }

static int check_if_label(char* imm)
{
    /*checks if the given imm is a label*/
    return (imm[0] >= 'a' && imm[0] <= 'z') || (imm[0] >= 'A' && imm[0] <= 'Z');
}

static void remove_spaces(char** line)
{
    /*removes leading spaces*/
    while (isspace((char)**line))
    {
        (*line)++;
    }
}

static int check_if_comment(char* line)
{
    /*checks for comment by the "#"*/
    return line[0] == '#';
}

static int check_line_with_label(char* line)
{
    /*checks if a line contains a label*/
    int i = 0;
    char c = line[0];

    while (c != '\0')
    {
        if (c == '#') /*for comment*/
        {
            break;
        }
        if (c == ':') /*how the label ends*/
        {
            return i;
        }
        c = line[++i];
    }

    return -1;
}

static int check_if_word(char* line) {
    /*Checks if the first char is '.', at this point we already trimmed the leading spaces.*/
    return line[0] == '.';
}

static int line_has_command(char* line)
{
    /*Checks if a line has both a label and a command*/
    char first_word[MAX_OPCODE_LEN];
    sscanf_s(line, "%s", first_word, MAX_OPCODE_LEN);
    return num_of_opcode(first_word) > -1; /*If we got a positive value then the first word is a valid opcode*/
}

static void command_to_output(FILE* output_file, char* line)
{
    char opcode[MAX_OPCODE_LEN], rd[MAX_REG_LEN], rs[MAX_REG_LEN], rt[MAX_REG_LEN], imm[MAX_IMM_LEN];
    sscanf_s(line, " %[^ $] $%[^,], $%[^,], $%[^,], %s ",
        opcode, MAX_OPCODE_LEN,
        rd, MAX_REG_LEN,
        rs, MAX_REG_LEN,
        rt, MAX_REG_LEN,
        imm, MAX_IMM_LEN);

    int opcode_d, rd_d, rs_d, rt_d, imm_d;
    /*Get the opcode and registers numbers from their dictionaries*/
    opcode_d = num_of_opcode(opcode);
    rd_d = num_of_register(rd);
    rs_d = num_of_register(rs);
    rt_d = num_of_register(rt);

    if (check_if_label(imm)) {
        imm_d = num_of_label(imm);
    }
    else {
        imm_d = strtol(imm, NULL, 0);
    }
    /*Takes the 3 LSB bits of imm1/imm2. This is used to prevent nagative numbers to be printed
    with their most significant bit extended*/
    imm_d = imm_d & 0xFFFFF;

    /*Write the command decoding to the output file*/
    fprintf(output_file, "%02X%01X%01X%01X\n", opcode_d, rd_d, rs_d, rt_d);
    num_of_lines++;
    if (strstr(line, "$imm") != NULL) {
        fprintf(output_file, "%05X\n", imm_d);
        num_of_lines++;
    }
}

static void add_word_to_array(char* line)
{
    /*Used for a '.word' command - will store the second number at the first number index in
    the words_array array*/
    int address, value;
    sscanf_s(line, ".word %i %i", &address, &value); /*%i deducts if it is int or hexa*/
    words_array[address] = value;
    word_index = MAX(word_index, address); /*Updates the max non empty index at the data_memory array*/
}

static void word_to_output(FILE* output_data_file)
{
    /*Writes the memory data file*/
    for (int i = num_of_lines; i <= word_index; i++) {
        fprintf(output_data_file, "%05X\n", words_array[i]);
    }
}

/* The main function used to pass over the input file */
static void file_flow(int pass_num, FILE* assembly_prog, FILE* output_file)
{
    char* line = (char*)malloc(MAX_LINE_LEN);
    if (line == NULL) {
        printf("Error - malloc failed");
        exit(0);
    }
    char* base_line_ptr = line;
    int colon_index;

    while (fgets(line, MAX_LINE_LEN, assembly_prog) != NULL)
    {
        remove_spaces(&line);
        if ((line[0] != '\0') && (check_if_comment(line) == 0))
        { /*If the line is empty or just a comment we skip it*/
            colon_index = check_line_with_label(line); /*If line starts with label then returns ':' index else -1*/
            if (pass_num == 1 && colon_index != -1)
            {
                line[colon_index] = '\0'; /*Gets only the label itself*/
                label_temp tmp_label;
                tmp_label.index = commands_counter;
                strcpy_s(tmp_label.label, MAX_LABEL_LEN, line);

                labels_array[label_counter++] = tmp_label;
            }
            line += (colon_index + 1); /*Skips the label (if there is no label it won't do anything)*/
            remove_spaces(&line);
            if (check_if_word(line)) { /*Checks if the line is a '.word' command*/
                if (pass_num == 1) { /*We save the data to memory only on the first pass*/
                    add_word_to_array(line);
                }
            }
            else {
                if (line_has_command(line) == 1) { /*If the line have a command we count it*/
                    commands_counter++;
                    if (strstr(line, "$imm") != NULL) {
                        commands_counter++;
                    }
                    if (pass_num == 2) { /*On the second pass we write the decoded command to the output file*/
                        command_to_output(output_file, line);
                    }
                }
            }
            line = base_line_ptr; /*We reset the line to point to its original address,
                                    otherwise the calloc for the line might not be enough*/
        }
    }
    commands_counter = 0;
    line = base_line_ptr; /* Make sure we free what we malloced */
    free(line);
}

int main(int argc, char const* argv[])
{
    /* Main func */
    FILE* assembly_prog, * output_file;
    fopen_s(&assembly_prog, argv[1], "r");
    if (assembly_prog == NULL) {
        /*raise error*/
        printf("Could not open assembly program file\n");
        exit(0);
    }
    fopen_s(&output_file, argv[2], "w");
    if (output_file == NULL) {
        /*raise error*/
        printf("Could not open output cmd file\n");
        exit(0);
    }
    file_flow(1, assembly_prog, output_file);
    rewind(assembly_prog);
    /*second flow*/
    file_flow(2, assembly_prog, output_file);
    word_to_output(output_file);
    fclose(assembly_prog);
    fclose(output_file);
}