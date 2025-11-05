#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "main.h"

// Command flags
#define CMD_RST 1
#define CMD_OK 2

// Function prototypes
void command_parser_fsm(void);
uint8_t get_command_flag(void);
void clear_command_flag(void);

#endif