#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pwm_z42.h>

#define TPM_MODULE 1000

/* Pinos lidos do Device Tree — sem literais 18 ou 19 no código */
#define LED_VERMELHO_NODE  DT_NODELABEL(red_led)
#define LED_VERDE_NODE     DT_NODELABEL(green_led)

#define PIN_VERMELHO  DT_GPIO_PIN(LED_VERMELHO_NODE, gpios)
#define PIN_VERDE     DT_GPIO_PIN(LED_VERDE_NODE, gpios)

static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

static void uart_putchar(uint8_t c) { uart_poll_out(uart_dev, c); }
static void uart_print(const char *s) { for (; *s; s++) uart_putchar(*s); }
static uint8_t uart_getchar(void) {
    uint8_t c;
    while (uart_poll_in(uart_dev, &c) != 0) {}
    return c;
}
static void uart_print_u32(uint32_t v) {
    char buf[11]; int i = 10; buf[i] = '\0';
    if (v == 0) { uart_putchar('0'); return; }
    while (v > 0 && i > 0) { buf[--i] = '0' + (v % 10); v /= 10; }
    uart_print(&buf[i]);
}
static int read_line(char *buf, int maxlen) {
    int i = 0; uint8_t c;
    while (i < maxlen - 1) {
        c = uart_getchar();
        if (c == '\r' || c == '\n') { if (i == 0) continue; uart_print("\r\n"); break; }
        if (c == '\b' || c == 0x7F) { if (i > 0) { i--; uart_print("\b \b"); } continue; }
        if (c < 0x20 || c > 0x7E) continue;
        uart_putchar(c); buf[i++] = (char)c;
    }
    buf[i] = '\0'; return i;
}

int main(void)
{
    uart_print("\r\n=== MAIN INICIOU ===\r\n");

    SIM->SCGC6 |= SIM_SCGC6_TPM2_MASK;
    k_busy_wait(1000);
    TPM2->SC = 0;
    k_busy_wait(1000);
    TPM2->CNT = 0;
    TPM2->STATUS = 0xFF;

    pwm_tpm_Init(TPM2, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);

    /* Números de pino vêm do DTS, não são literais */
    pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, PIN_VERMELHO);
    pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_H, GPIOB, PIN_VERDE);

    pwm_tpm_CnV(TPM2, 0, TPM_MODULE);
    pwm_tpm_CnV(TPM2, 1, TPM_MODULE);

    uart_print("--- Controle do LED Laranja ---\r\n\r\n");

    char buf[8];
    int intensidade;

    for (;;) {
        intensidade = -1;
        while (intensidade < 0 || intensidade > 100) {
            uart_print("Digite a intensidade (0 a 100): ");
            int len = read_line(buf, sizeof(buf));
            int valido = (len > 0);
            for (int j = 0; j < len; j++)
                if (!isdigit((unsigned char)buf[j])) { valido = 0; break; }
            if (!valido) { uart_print("  Erro: apenas numeros.\r\n"); continue; }
            intensidade = atoi(buf);
        }

        uint32_t escala   = (intensidade * TPM_MODULE) / 100;
        uint32_t red_duty = TPM_MODULE - escala;
        uint32_t grn_duty = TPM_MODULE - (escala * 75) / 100;

        pwm_tpm_CnV(TPM2, 0, (int)red_duty);
        pwm_tpm_CnV(TPM2, 1, (int)grn_duty);

        uart_print("-> ");
        uart_print_u32(intensidade);
        uart_print("%  |  CnV Vermelho=");
        uart_print_u32(red_duty);
        uart_print("  Verde=");
        uart_print_u32(grn_duty);
        uart_print("\r\n\r\n");
    }
}