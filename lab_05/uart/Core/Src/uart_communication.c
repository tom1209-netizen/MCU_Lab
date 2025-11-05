#include "uart_communication.h"
#include "command_parser.h"
#include <stdio.h>
#include <string.h>

// External variables
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;

// FSM States
typedef enum {
  UART_IDLE,
  UART_SEND_ADC,
  UART_WAIT_ACK,
  UART_RETRANSMIT
} UartCommState;

// Internal variables
static UartCommState comm_state = UART_IDLE;
static uint16_t adc_value = 0;
static char tx_buffer[20];
static uint32_t timeout_start = 0;
static const uint32_t TIMEOUT_MS = 3000; // 3 seconds

void uart_communication_fsm(void) {
  uint8_t cmd_flag = get_command_flag();
  uint32_t current_time = HAL_GetTick();

  switch (comm_state) {
  case UART_IDLE:
    if (cmd_flag == CMD_RST) {
      // Transition to SEND_ADC
      comm_state = UART_SEND_ADC;
      clear_command_flag();
    }
    break;

  case UART_SEND_ADC:
    // Read ADC value
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
      adc_value = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    // Format message: !ADC=xxxx#
    sprintf(tx_buffer, "!ADC=%d#\r\n", adc_value);

    // Transmit to terminal
    HAL_UART_Transmit(&huart2, (uint8_t *)tx_buffer, strlen(tx_buffer), 1000);

    // Start timeout timer
    timeout_start = HAL_GetTick();

    // Transition to WAIT_ACK
    comm_state = UART_WAIT_ACK;
    break;

  case UART_WAIT_ACK:
    if (cmd_flag == CMD_OK) {
      // Acknowledgment received
      clear_command_flag();
      comm_state = UART_IDLE;
    } else if ((current_time - timeout_start) >= TIMEOUT_MS) {
      // Timeout expired
      comm_state = UART_RETRANSMIT;
    }
    break;

  case UART_RETRANSMIT:
    // Retransmit the same ADC packet
    HAL_UART_Transmit(&huart2, (uint8_t *)tx_buffer, strlen(tx_buffer), 1000);

    // Reset timeout timer
    timeout_start = HAL_GetTick();

    // Go back to WAIT_ACK
    comm_state = UART_WAIT_ACK;
    break;

  default:
    comm_state = UART_IDLE;
    break;
  }
}
