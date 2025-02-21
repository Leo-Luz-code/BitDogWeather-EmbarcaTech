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
uint slice_led_r, slice_led_g;             // Slices PWM dos LEDs
uint LED_ON = 100;
uint LED_OFF = 0;

void gpio_irq_handler(uint gpio, uint32_t event);

// Configuração do PWM para os LEDs RGB
void setup_pwm_led(uint led, uint *slice, uint16_t level)
{
    gpio_set_function(led, GPIO_FUNC_PWM);
    *slice = pwm_gpio_to_slice_num(led);
    pwm_set_clkdiv(*slice, DIVIDER_PWM);
    pwm_set_wrap(*slice, PERIOD);
    pwm_set_gpio_level(led, level);
    pwm_set_enabled(*slice, true);
}

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
    setup_pwm_led(LED_R, &slice_led_r, LED_ON);
    setup_pwm_led(LED_G, &slice_led_g, LED_ON);

    gpio_set_irq_enabled_with_callback(SW, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
}

// Função de alternação dos LEDs
void alter_leds()
{
    STATE_LEDS = !STATE_LEDS;

    if (!STATE_LEDS)
    {
        pwm_set_gpio_level(LED_R, LED_OFF);
        pwm_set_gpio_level(LED_G, LED_OFF);
    }
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

        // Converte os valores do joystick para porcentagem e ppm
        uint8_t umidade = (vrx_value * 100) / 4070;        // Porcentagem do sensor de umidade
        uint16_t qualidade_ar = (vry_value * 2000) / 4070; // ppm do sensor de qualidade do ar

        // Formata os valores como string
        snprintf(str_x, sizeof(str_x), "Um: %d %%", umidade);
        snprintf(str_y, sizeof(str_y), "Ar: %d ppm", qualidade_ar);

        ssd1306_fill(&ssd, !COLOR);
        ssd1306_rect(&ssd, 3, 3, 120, 56, COLOR, !COLOR, 1); // Desenha a borda

        // Determina as posições centralizadas para os textos
        uint8_t center_x_x = (WIDTH - (strlen(str_x) * 8)) / 2;
        uint8_t center_x_y = (WIDTH - (strlen(str_y) * 8)) / 2;
        uint8_t center_y = HEIGHT / 2 - 8; // Posição vertical ajustada

        if (STATE_LEDS)
        {
            if ((umidade >= 40 && umidade <= 60) && (qualidade_ar >= 800 && qualidade_ar <= 1200)) // Dentro da zona segura
            {
                pwm_set_gpio_level(LED_G, LED_ON);
                pwm_set_gpio_level(LED_R, LED_OFF);
            }
            else if (((umidade > 60 && umidade < 90) || (umidade < 40 && umidade > 10)) ||
                     ((qualidade_ar > 1200 && qualidade_ar < 1800) || (qualidade_ar < 800 && qualidade_ar > 200))) // Zona de alerta
            {
                pwm_set_gpio_level(LED_G, LED_ON);
                pwm_set_gpio_level(LED_R, LED_ON);
            }
            else if (umidade >= 90 || umidade <= 10 || qualidade_ar >= 1800 || qualidade_ar <= 200) // Zona de perigo
            {
                pwm_set_gpio_level(LED_G, LED_OFF);
                pwm_set_gpio_level(LED_R, LED_ON);
            }
        }

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
        if (current_time - last_time_SW > 200000) // 500 ms de debouncing
        {
            last_time_SW = current_time;
            alter_leds();
        }
    }
}