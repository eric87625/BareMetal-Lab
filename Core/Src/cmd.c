/*
 * cmd.c
 *
 *  Created on: Jan 30, 2026
 *      Author: YC Lin
 */

#include "cmd.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "main.h"     // GPIO / TIM / UART handle
#include "console.h"  // print()
#include "watchdog.h" // System_Simulate_Deadlock()

/* ---------- external resources from main.c ---------- */
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef  htim2;

extern char cmd_buff[30];
extern uint8_t rx_buff[10];
extern int cmd_buff_idx;

/* ---------- CMD infrastructure ---------- */
typedef void (*CmdHandler)(int argc, char **argv);

typedef struct
{
    char *cmd_name;
    CmdHandler handler;
} CmdEntry;

static void str_to_upper_inplace(char *s)
{
    if (s == NULL)
        return;
    while (*s)
    {
        *s = (char)toupper((unsigned char)*s);
        s++;
    }
}

/* ---------- CMD handlers ---------- */
void func_led_on(int para_count, char **para)
{
    if (para_count != 0)
    {
        print("error: func_led_on: para_count != 0, must check!\r\n");
        return;
    }
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
}
void func_led_off(int para_count, char **para)
{
    if (para_count != 0)
    {
        print("error: func_led_off: para_count != 0, must check!\r\n");
        return;
    }
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
}
void func_set_led(int para_count, char **para)
{
    if (para_count != 1)
    {
        print("error: func_led_off: para_count = %d != 1, must check!\r\n",
                para_count);
        return;
    }
    int led_num;
    sscanf(para[0], "%d", &led_num); // led_num will be 1
    if (led_num == 1)
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
//    else if (led_num == 2) HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
}
void func_uart_tx(int para_count, char **para)
{
    char *paraStr = malloc(strlen(para[0]) + 1);  // +1 for '\0'
    if (paraStr != NULL)
    {
        strcpy(paraStr, para[0]);
    }
    HAL_UART_Transmit(&huart2, (uint8_t*) paraStr, strlen(paraStr),
    HAL_MAX_DELAY);
    // free
    free(paraStr);
}
/* ---------- PWM/GPIO helpers ---------- */
static void set_pwm_pin_gpio(int high)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void set_pwm_pin_af(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM2; // PA0 -> TIM2_CH1
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
void func_pwm_on(int para_count, char **para)
{
    if (para_count != 2)
    {
        print("error: func_pwm_on: para_count != 2, must check!\r\n");
        return;
    }
    int Duty;
    int Freq;

    sscanf(para[0], "%d", &Duty);
    sscanf(para[1], "%d", &Freq);
    if (Duty > 100)
    {
        Duty = 100;
        print("Warning: Max Duty is 100\r\n");
    }
    if (Freq < 0 || Freq > 10000)
    {
        Freq = 1000;
        print("Warning: Freq is out-of-range\r\n");
    }

    // Stop PWM before reconfiguring
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);

    if (Duty == 100 || Duty == 0)
    {
        // Duty 100 -> High, Duty 0 -> Low (GPIO mode)
        set_pwm_pin_gpio(Duty == 100);
    }
    else
    {
        // Ensure pin is in PWM AF mode
        set_pwm_pin_af();
        // ARR is time range.
        //    Duty(%)=ARR/CCR​×100%
        //    ARR (Period) sets the PWM frequency, CCR1 (Pulse) sets the duty cycle.
        //    htim2.Init.Period = 16000;
        //    sConfigOC.Pulse = 5000;
        uint32_t arr = 16000000 / (uint32_t)Freq;
        if (arr == 0) arr = 1;
        htim2.Instance->ARR = arr;
        uint32_t ccr = (arr * (uint32_t)Duty) / 100;
        if (ccr >= arr) ccr = arr - 1; // ensure strictly less than ARR
        htim2.Instance->CCR1 = ccr;
        //
        // Start PWM
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    }

}

void func_crash(int para_count, char **para)
{
    (void)para;
    if (para_count != 0)
    {
        print("error: CRASH takes no parameters\r\n");
        return;
    }

    print("\r\n[SYSTEM] Simulating deadlock now...\r\n");
    System_Simulate_Deadlock();
}
void func_invalid(int para_count, char **para)
{
    // TODO: whether or not
    return;
}

/* ---------- CMD table ---------- */
static CmdEntry cmd_table[] = {
    {"LED_ON",     func_led_on},
    {"LED_OFF",    func_led_off},
    {"SET_LED",    func_set_led},
    {"UART_TX",    func_uart_tx},
    {"PWM_ON",     func_pwm_on},
    {"CRASH",      func_crash},
    {"INVALID_CMD",func_invalid},
};


/* ---------- helpers ---------- */
static int is_valid_input(char c)
{

    if ((c >= 32 && c <= 126) || c == '\r' || c == '\n' || c == '\t')
    {
        print("%c", (int) c);
        return 1;
    }
    if (c == 3)
    {
        // ctrl-c
        memset(cmd_buff, 0, sizeof(cmd_buff));
        memset(rx_buff, 0, sizeof(rx_buff));
        cmd_buff_idx = 0;
        print("^C\r\n");
    } else
    {
        print("\r\nInvalid input: %d\r\n", c);
        int i = 0;
        while (i < cmd_buff_idx)
        {
            print("%c", (int) cmd_buff[i]);
            i++;
        }
        memset(rx_buff, 0, sizeof(rx_buff));
    }
    return 0;
}

/* ---------- public APIs ---------- */
void process_cmd(void)
{

    char rx_char = rx_buff[0];

    if (!is_valid_input(rx_char))
    {
        return;
    }

    if (rx_char == '\r' || rx_char == '\n' || rx_char == '\t')
    { // Ready to parse CMD_HEAD,  CMD_PARA
        cmd_buff[cmd_buff_idx] = '\0'; // overwrite last byte

        /* CMD compare BEGIN */
        const char *delim = " "; // only accept " " for separate parameters
        char *token;
        char cmd_head[10];
        char *cmd_para[5];
        int para_count = 0;

        print("\r\nTokens:\n\r");

        // First call to get the first token: CMD_HEAD
        token = strtok(cmd_buff, delim);
        str_to_upper_inplace(token);
        print("first token is %s\r\n", token);
        // Check token in CMD_table
        CmdHandler cmd_handler;
        int cmd_idx = 0;
        while (cmd_idx != INVALID_CMD)
        {
            if (strcmp(token, cmd_table[cmd_idx].cmd_name) == 0)
            {
                strcpy(cmd_head, token);
                cmd_handler = cmd_table[cmd_idx].handler;
                break;
            }
            cmd_idx++;
        }
        if (cmd_idx == INVALID_CMD)
        {
            print("Invalid CMD !\r\n");
            memset(cmd_buff, 0, sizeof(cmd_buff));
            memset(rx_buff, 0, sizeof(rx_buff));
            cmd_buff_idx = 0;
            return;
        }

        token = strtok(NULL, delim);
        while (token != NULL)
        {
            print("next token is %s\r\n", token);
            // Subsequent calls with NULL to get the next tokens: CMD_PARA
            cmd_para[para_count++] = token;
            token = strtok(NULL, delim);
        }
        cmd_handler(para_count, cmd_para);
        memset(cmd_buff, 0, sizeof(cmd_buff));
        cmd_buff_idx = 0;
        /* CMD compare END */

    } else
    {
        cmd_buff[cmd_buff_idx++] = rx_char;
    }

    return;
}

void handle_binary_cmd(uint8_t *payload, uint8_t length)
{
    uint8_t cmd_id = payload[0];
    uint8_t para_count = length - 1;

    if (cmd_id >= INVALID_CMD)
    {
        print("Binary CMD invalid: %d\r\n", cmd_id);
        return;
    }

    char *argv[5];
    char temp[5][16];

    if (cmd_id == UART_TX)
    {
        argv[0] = (char *)&payload[1];
        cmd_table[cmd_id].handler(para_count, argv);
        return;
    }

    for (int i = 0; i < para_count; i++)
    {
        sprintf(temp[i], "%d", payload[1 + i]);
        argv[i] = temp[i];
    }

    cmd_table[cmd_id].handler(para_count, argv);
}
