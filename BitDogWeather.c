#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "src/ssd1306.h"
#include "src/pin_configuration.h"
#include "src/font.h"

ssd1306_t ssd;                             // Estrutura para manipulação do display
uint32_t current_time;                     // Armazena o tempo atual
static volatile uint32_t last_time_SW = 0; // Variável para debounce
volatile bool STATE_LEDS = true;           // Varíavel de estado dos LEDs

void gpio_irq_handler(uint gpio, uint32_t event);

// Configura joystick e botão
void setup_joystick_and_button()
{
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);

    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

// Configuração do display OLED
void setup_display()
{
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ADDRESS, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

// Configuração inicial do sistema
void setup()
{
    stdio_init_all();
    setup_joystick_and_button();
    setup_display();

    gpio_set_irq_enabled_with_callback(SW, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
}

// Função de alternação dos LEDs
void alter_leds()
{
    STATE_LEDS = !STATE_LEDS;

    gpio_put(LED_B, STATE_LEDS);
    gpio_put(LED_R, STATE_LEDS);
    gpio_put(LED_G, STATE_LEDS);
}

// Função para leitura do joystick
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value)
{
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vrx_value = adc_read();

    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *vry_value = adc_read();
}

int main()
{
    stdio_init_all();
    uint16_t vrx_value, vry_value;
    char str_x[20], str_y[20]; // Buffers para armazenar os valores formatados
    setup();

    while (true)
    {
        joystick_read_axis(&vrx_value, &vry_value);

        // Converte os valores do joystick para porcentagem
        uint8_t umidade = (vrx_value * 100) / 4070;
        uint16_t qualidade_ar = (vry_value * 2000) / 4070;

        // Formata os valores como string
        snprintf(str_x, sizeof(str_x), "Um: %d %%", umidade);
        snprintf(str_y, sizeof(str_y), "Ar: %d ppm", qualidade_ar);

        ssd1306_fill(&ssd, !COLOR);
        ssd1306_rect(&ssd, 3, 3, 120, 56, COLOR, !COLOR, 1); // Desenha a borda

        // Determina as posições centralizadas para os textos
        uint8_t center_x_x = (WIDTH - (strlen(str_x) * 8)) / 2;
        uint8_t center_x_y = (WIDTH - (strlen(str_y) * 8)) / 2;
        uint8_t center_y = HEIGHT / 2 - 8; // Posição vertical ajustada

        // Desenha os valores no centro do display
        ssd1306_draw_string(&ssd, str_x, center_x_x, center_y);
        ssd1306_draw_string(&ssd, str_y, center_x_y, center_y + 10);

        ssd1306_send_data(&ssd); // Envia os dados para o leitor

        sleep_ms(10);
    }
}

// Função de interrupção para os botões com debounce
void gpio_irq_handler(uint gpio, uint32_t event)
{
    current_time = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual em microssegundos

    if (gpio == SW) // Se a interrupção veio do botão SW
    {
        if (current_time - last_time_SW > 300000) // 300 ms de debouncing
        {
            alter_leds();
        }
    }
}