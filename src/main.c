#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

// Referências do RGB nativo da placa
#define LED_GREEN_NODE DT_ALIAS(led0)
#define LED_BLUE_NODE  DT_ALIAS(led1)
#define LED_RED_NODE   DT_ALIAS(led2)

static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);

// 1. Criação dos Estados
// Usamos um 'enum' para dar nomes amigáveis aos números 0, 1 e 2.
typedef enum {
    ESTADO_VERDE,
    ESTADO_AMARELO,
    ESTADO_VERMELHO
} estado_semaforo_t;

void main(void)
{
    if (!gpio_is_ready_dt(&led_green) || !gpio_is_ready_dt(&led_blue) || !gpio_is_ready_dt(&led_red)) {
        printk("Erro: Controladores do LED RGB não estão prontos!\n");
        return;
    }

    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);

    printk("Máquina de Estados: Semáforo iniciado!\n");

    // 2. Definimos o estado inicial
    estado_semaforo_t estado_atual = ESTADO_VERDE;

    // 3. Loop principal
    while (1) {
        
        // Avalia em qual estado estamos e executa a ação correspondente
        switch (estado_atual) {
            
            case ESTADO_VERDE:
                gpio_pin_set_dt(&led_red, 0);
                gpio_pin_set_dt(&led_blue, 0);
                gpio_pin_set_dt(&led_green, 1);
                printk("Sinal: VERDE\n");
                
                k_msleep(5000); 
                
                // Transição: Próximo estado será o Amarelo
                estado_atual = ESTADO_AMARELO; 
                break;

            case ESTADO_AMARELO:
                gpio_pin_set_dt(&led_green, 1);
                gpio_pin_set_dt(&led_red, 1);
                gpio_pin_set_dt(&led_blue, 0);
                printk("Sinal: AMARELO\n");
                
                k_msleep(2000);
                
                // Transição: Próximo estado será o Vermelho
                estado_atual = ESTADO_VERMELHO;
                break;

            case ESTADO_VERMELHO:
                gpio_pin_set_dt(&led_green, 0);
                gpio_pin_set_dt(&led_blue, 0);
                gpio_pin_set_dt(&led_red, 1);
                printk("Sinal: VERMELHO\n");
                
                k_msleep(5000);
                
                // Transição: Próximo estado volta a ser o Verde (ciclo recomeça)
                estado_atual = ESTADO_VERDE;
                break;
        }
    }
}