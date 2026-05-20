#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pwm_z42.h>

#define TPM_MODULE 1000

static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

static void uart_putchar(uint8_t c)
{
    uart_poll_out(uart_dev, c);
}

static uint8_t uart_getchar(void)
{
    uint8_t c;
    // Loop limpo: interroga o hardware da UART na velocidade máxima do chip.
    // Assim que um caractere entra no registrador, o retorno é 0 e saímos do loop na hora.
    while (uart_poll_in(uart_dev, &c) != 0) {
        // Removemos o k_busy_wait daqui para não perder o "timing" dos bytes vindo em rajada
    }
    return c;
}

static void uart_print(const char *s)
{
    for (; *s; s++) uart_putchar((uint8_t)*s);
}

static void uart_print_u32(uint32_t v)
{
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (v == 0) { uart_putchar('0'); return; }
    while (v > 0 && i > 0) {
        buf[--i] = '0' + (v % 10);
        v /= 10;
    }
    uart_print(&buf[i]);
}

static int read_line(char *buf, int maxlen)
{
    int i = 0;
    uint8_t c;

    while (i < maxlen - 1) {
        c = uart_getchar();

        // Se receber o fim de linha (ENTER)
        if (c == '\r' || c == '\n') {
            if (i == 0) continue;  // Ignora se o usuário apertar ENTER com o buffer vazio
            uart_print("\r\n");
            break;
        }

        // Trata o Backspace (apagar caractere)
        if (c == '\b' || c == 0x7F) {
            if (i > 0) { 
                i--; 
                uart_print("\b \b"); 
            }
            continue;
        }

        // Ignora caracteres de controle/inválidos ocultos na transmissão
        if (c < 0x20 || c > 0x7E) continue;

        // Ecoa de volta para o terminal do usuário saber o que digitou
        uart_putchar(c);  
        buf[i++] = (char)c;
    }

    buf[i] = '\0';
    return i;
}

int main(void)
{
    uart_print("\r\n=== MAIN INICIOU ===\r\n");

    // Habilita o clock do TPM2
    SIM->SCGC6 |= SIM_SCGC6_TPM2_MASK;
    k_busy_wait(1000);

    // RESET COMPLETO DOS REGISTRADORES DO TIMER
    TPM2->SC = 0;             // Para o contador antes de configurar
    k_busy_wait(1000);
    TPM2->CNT = 0;            // Zera o contador atual
    TPM2->STATUS = 0xFF;      // Limpa todas as flags acumuladas

    // Inicializa o TPM2 e os canais usando a biblioteca do seu projeto
    // Passando o módulo fixo em 1000
    pwm_tpm_Init(TPM2, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);
    pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, 18); // Vermelho
    pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_H, GPIOB, 19); // Verde
    
    // Como a placa é Anodo Comum (0 acende, 1000 apaga), 
    // forçamos 1000 nos dois canais para que eles iniciem APAGADOS.
    pwm_tpm_CnV(TPM2, 0, TPM_MODULE);
    pwm_tpm_CnV(TPM2, 1, TPM_MODULE);

    uart_print("--- Controle do LED Laranja ---\r\n\r\n");

    char buf[8];
    int intensidade;

    for (;;)
    {
        // Força a entrada no loop de leitura da UART
        intensidade = -1; 

        while (intensidade < 0 || intensidade > 100)
        {
            uart_print("Digite a intensidade (0 a 100): ");

            int len = read_line(buf, sizeof(buf));

            int valido = 1;
            for (int j = 0; j < len; j++) {
                if (!isdigit((unsigned char)buf[j])) { valido = 0; break; }
            }
            if (!valido || len == 0) {
                uart_print("  Erro: apenas numeros.\r\n");
                continue;
            }

            intensidade = atoi(buf);
        }

        // --- CÁLCULO DA INTENSIDADE GLOBAL (0 a 1000) ---
        uint32_t escala_global = (intensidade * TPM_MODULE) / 100;

        // --- PROPORÇÃO DA COR LARANJA ---
        // Se a intensidade for 100 (Máxima): vermelho_alvo = 1000, verde_alvo = 750
        // Se a intensidade for 50  (Média):  vermelho_alvo = 500,  verde_alvo = 375
        uint32_t vermelho_alvo = escala_global;
        uint32_t verde_alvo    = (escala_global * 75) / 100; // Verde a 75% da força do vermelho

        // --- COMPENSAÇÃO REAL PARA ANODO COMUM (Inversão) ---
        // Se intensidade = 0 (Apagado):
        //   red_duty   = 1000 - 0 = 1000 (Desliga o pino físico)
        //   green_duty = 1000 - 0 = 1000 (Desliga o pino físico)
        // Se intensidade = 100 (Laranja Máximo):
        //   red_duty   = 1000 - 1000 = 0   (Condução máxima -> Vermelho 100%)
        //   green_duty = 1000 - 750  = 250 (Condução alta -> Verde 75%)
        uint32_t red_duty   = TPM_MODULE - vermelho_alvo;
        uint32_t green_duty = TPM_MODULE - verde_alvo;

        // Proteção extra: Garante que os valores nunca passem dos limites do timer
        if (red_duty > TPM_MODULE)   red_duty = TPM_MODULE;
        if (green_duty > TPM_MODULE) green_duty = TPM_MODULE;

        // Envia os valores ajustados para os comparadores de hardware do chip
        pwm_tpm_CnV(TPM2, 0, (int)red_duty);
        pwm_tpm_CnV(TPM2, 1, (int)green_duty);

        // Feedback via terminal para conferência
        uart_print("-> ");
        uart_print_u32((uint32_t)intensidade);
        uart_print("%  |  Registrador CnV Vermelho=");
        uart_print_u32(red_duty);
        uart_print("  Verde=");
        uart_print_u32(green_duty);
        uart_print("\r\n\r\n");
    }

    return 0;
}