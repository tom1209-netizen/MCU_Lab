#include "command_parser.h"
#include <string.h>

// External variables from main.c
extern uint8_t buffer[30];
extern uint8_t index_buffer;
extern uint8_t buffer_flag;

// Internal variables
typedef enum {
  PARSER_INIT,
  PARSER_RECEIVING,
  PARSER_COMMAND_READY
} ParserState;

static ParserState parser_state = PARSER_INIT;
static uint8_t command_buffer[10];
static uint8_t cmd_index = 0;
static uint8_t command_flag = 0;

uint8_t get_command_flag(void) { return command_flag; }

void clear_command_flag(void) { command_flag = 0; }

void command_parser_fsm(void) {
  for (uint8_t i = 0; i < index_buffer; i++) {
    uint8_t ch = buffer[i];

    switch (parser_state) {
    case PARSER_INIT:
      if (ch == '!') {
        cmd_index = 0;
        memset(command_buffer, 0, sizeof(command_buffer));
        parser_state = PARSER_RECEIVING;
      }
      break;

    case PARSER_RECEIVING:
      if (ch == '#') {
        // End of command, parse it
        command_buffer[cmd_index] = '\0';

        if (strcmp((char *)command_buffer, "RST") == 0) {
          command_flag = CMD_RST;
        } else if (strcmp((char *)command_buffer, "OK") == 0) {
          command_flag = CMD_OK;
        } else {
          command_flag = 0; // Invalid command
        }

        parser_state = PARSER_INIT;
      } else if (ch == '!') {
        // New command started, reset
        cmd_index = 0;
        memset(command_buffer, 0, sizeof(command_buffer));
      } else {
        // Store command character
        if (cmd_index < 9) {
          command_buffer[cmd_index++] = ch;
        } else {
          // Command too long, reset
          parser_state = PARSER_INIT;
        }
      }
      break;

    default:
      parser_state = PARSER_INIT;
      break;
    }
  }

  // Clear buffer after processing
  index_buffer = 0;
  memset(buffer, 0, sizeof(buffer));
}
