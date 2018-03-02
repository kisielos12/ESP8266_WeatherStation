/*
 * This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <http://unlicense.org>
 */


#ifndef __CLI_H__
#define __CLI_H__

#include "esp_common.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char *cmd;
    void (*handler)(uint32_t argc, char *argv[]);
    uint32_t min_arg, max_arg;
    char *help;
    char *usage;
} command_t;

void cli_run(const command_t cmds[], const uint32_t num, const char *app_name);


#endif // __CLI_H__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli.h"
#include "uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "gpio.h"


#ifdef CONFIG_CMD_BUF_SIZE
 #define CMD_BUF_SIZE CONFIG_CMD_BUF_SIZE
#else
 #define CMD_BUF_SIZE (255)
#endif 

#ifdef CONFIG_CMD_EXTRA_PADDING
 #define CMD_EXTRA_PADDING CONFIG_CMD_EXTRA_PADDING
#else
 #define CMD_EXTRA_PADDING (10)
#endif 

#ifdef CONFIG_CMD_MAX_ARGC
 #define MAX_ARGC CONFIG_CMD_MAX_ARGC
#else
 #define MAX_ARGC (16)
#endif 

#ifdef CONFIG_PROMPT_STR
 #define PROMPT_STR CONFIG_PROMPT_STR
#else
 #define PROMPT_STR "#"
#endif 

#ifdef CONFIG_ARG_DELIMITER_STR
 #define ARG_DELIMITER_STR CONFIG_ARG_DELIMITER_STR
#else
 #define ARG_DELIMITER_STR " "
#endif 

#define MAX_COMMANDS 20
#define GETCHAR(x) { x = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF; }
#define CLEAR_FIFO {    SET_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST); \
                        CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST); }

#define INC_IDX_COUNTER { idx_cnt = (idx_cnt + 1) % CMD_BUF_SIZE; }
#define INC_IDX_READ_COUNTER { idx_read = (idx_read + 1) % CMD_BUF_SIZE; }

static command_t cmds[MAX_COMMANDS] = {0, };
static unsigned char commandBuff[CMD_BUF_SIZE] = {0,};
static int idx_cnt = 0;
static int idx_read = 0;
static xSemaphoreHandle Uart_Rx_xSemaphore = NULL;
static int commandCounter = 0;

void UART0_RX_IRQ(void *para)
{
    uint8 RcvChar;
    uint8 uart_no = UART0;//UartDev.buff_uart_no;
    uint8 fifo_len = 0;
    uint8 buf_idx = 0;
    uint8 fifo_tmp[128] = {0};

    uint32 uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no)) ;

    portBASE_TYPE xHigherPriorityTaskWoken  = pdFALSE;

    while (uart_intr_status != 0x0) 
    {
        if (UART_FRM_ERR_INT_ST == (uart_intr_status & UART_FRM_ERR_INT_ST)) 
        {
            WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
        } 
        else if (UART_RXFIFO_FULL_INT_ST == (uart_intr_status & UART_RXFIFO_FULL_INT_ST)) 
        {
            fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
            buf_idx = 0;

            while (buf_idx < fifo_len) 
            {
                buf_idx++;
            }

            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
        } 
        else if (UART_RXFIFO_TOUT_INT_ST == (uart_intr_status & UART_RXFIFO_TOUT_INT_ST)) 
        {
            fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
            buf_idx = 0;

            while (buf_idx < fifo_len) 
            {
                commandBuff[idx_cnt] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
                INC_IDX_COUNTER;

                buf_idx++;

                xSemaphoreGiveFromISR( Uart_Rx_xSemaphore, &xHigherPriorityTaskWoken );                
            }

            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
        } 
        else if (UART_TXFIFO_EMPTY_INT_ST == (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST)) 
        {
            printf("empty\n\r");
            WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
            CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
        }
        else 
        {
            //skip
        }

        uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no)) ;
    }
}

static unsigned char getChar()
{
    unsigned char c = 0x0;

    if(idx_cnt != idx_read)
    {
        c = commandBuff[idx_read];
        INC_IDX_READ_COUNTER;
    }

    return c;
}

// Original function by Alex Stewart (@foogod)
static size_t cli_readline(char *buffer, size_t buf_size, bool echo) {
    size_t i = 0;
    int c;

    while (true) {
        xSemaphoreTake( Uart_Rx_xSemaphore, portMAX_DELAY);
        c = getChar();
        if(c)
        {
            if (c == '\r') {
                if (echo) printf("\n");
                break;
            } else if (c == '\b' || c == 0x7f) {
                if (i) {
                    if (echo) printf("\b \b");
                    i--;
                }
            } else if (c < 0x20) {
                /* Ignore other control characters */
            } else if (i >= buf_size - 1) {
                if (echo) printf("\a");
            } else {
                buffer[i++] = c;
                if (echo) printf("%c",c);
            }
        }
    }

    buffer[i] = 0;


    return i;
}

void cli_run(const command_t cmds[], const uint32_t num, const char *app_name)
{
    int i = 0;
    char *cmd_buffer = malloc(CMD_BUF_SIZE);
    size_t len;

    if (!cmd_buffer) {
        printf("ERROR: Cannot allocate command buffer for CLI!\n");
        return;
    }

    if (app_name) {
        printf("\nWelcome to %s!", app_name);
    } else {
        printf("\nWelcome!");
    }
    printf(" Enter 'help' for available commands.\n\n");

    while (true) {
        bool found_cmd = false;
        printf("%s ", PROMPT_STR);
        len = cli_readline(cmd_buffer, CMD_BUF_SIZE, true);
        if (!len) continue;
        // Split string "<command> <argument 1> <argument 2>  ...  <argument N>"
        // into argc, argv style
        char *argv[MAX_ARGC];
        int argc = 1;
        char *temp, *rover;
        memset((void*) argv, 0, sizeof(argv));
        argv[0] = cmd_buffer;
        rover = cmd_buffer;
        while(argc < MAX_ARGC && (temp = strstr(rover, ARG_DELIMITER_STR))) 
        { // @todo: fix for multiple delimiters
            argv[argc++] = temp+1;
            rover = temp+1;
            *temp = 0;
        }
        for (i=0; i<num; i++) 
        {
            if (cmds[i].cmd && strcmp(argv[0], cmds[i].cmd) == 0) 
            {
                if (cmds[i].min_arg > argc-1 || cmds[i].max_arg < argc-1)
                {
                    printf("Wrong number of arguments %d (%d..%d).\n", argc-1, cmds[i].min_arg, cmds[i].max_arg);
                    if (cmds[i].usage) 
                    {
                        printf("Usage: %s%s%s\n", cmds[i].cmd, ARG_DELIMITER_STR, cmds[i].usage);
                    }
                }
                else 
                {
                    cmds[i].handler(argc, argv);
                }
                found_cmd = true;
            }
        }
        if (!found_cmd) 
        {
            if (strcmp(argv[0], "help") == 0) 
            {
                printf("CLI COMMANDS: \r\n");
                uint32_t max_len = 0;
                // Make sure command list is nicely padded
                for (i=0; i<num; i++) 
                {
                    if (cmds[i].help && max_len < strlen(cmds[i].cmd)) 
                    {
                        max_len = strlen(cmds[i].cmd);
                    }
                }
                for (i=0; i<num; i++) 
                {
                    if (cmds[i].help) 
                    {
                        char cmd[max_len+CMD_EXTRA_PADDING];
                        sprintf(cmd, "%-*s", CMD_EXTRA_PADDING, cmds[i].cmd);
                        printf("%s %s\r\n", cmd, cmds[i].help);
                    }
                }
                printf("\r\n\r\n");
            } 
            else 
            {
                printf("Unknown command\n");
            }
        }
    }
}


static void uart_init(void)
{
    UART_WaitTxFifoEmpty(UART0);
    UART_WaitTxFifoEmpty(UART1);

    UART_ConfigTypeDef uart_config;
    uart_config.baud_rate    = BIT_RATE_74880;
    uart_config.data_bits     = UART_WordLength_8b;
    uart_config.parity          = USART_Parity_None;
    uart_config.stop_bits     = USART_StopBits_1;
    uart_config.flow_ctrl      = USART_HardwareFlowControl_None;
    uart_config.UART_RxFlowThresh = 120;
    uart_config.UART_InverseMask = UART_None_Inverse;
    UART_ParamConfig(UART0, &uart_config);

    UART_IntrConfTypeDef uart_intr;
    uart_intr.UART_IntrEnMask = UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA;
    uart_intr.UART_RX_FifoFullIntrThresh = 10;
    uart_intr.UART_RX_TimeOutIntrThresh = 2;
    uart_intr.UART_TX_FifoEmptyIntrThresh = 20;
    UART_IntrConfig(UART0, &uart_intr);

    UART_SetPrintPort(UART0);
    UART_intr_handler_register(UART0_RX_IRQ, NULL);
    ETS_UART_INTR_ENABLE();

}
void CLI_Register(char *cmd, void (*handler)(uint32_t argc, char *argv[]), uint32_t min_arg, uint32_t max_arg, char *help)
{
      cmds[commandCounter].cmd = cmd,
      cmds[commandCounter].handler = handler,
      cmds[commandCounter].min_arg = min_arg, 
      cmds[commandCounter].max_arg = max_arg,
      cmds[commandCounter].help = help,
      cmds[commandCounter].usage = "N/A",

      commandCounter++;
}

static void cliTask (void *pvParameters)
{
    cli_run(cmds, commandCounter, NULL);
}

void CLI_Init()
{
    Uart_Rx_xSemaphore = xSemaphoreCreateCounting(CMD_BUF_SIZE, 0);

    uart_init();

    xTaskCreate(cliTask, (signed char *)"cliTask", 4 * 1024, NULL, 2, NULL);
}
