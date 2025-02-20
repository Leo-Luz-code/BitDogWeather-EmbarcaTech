#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "src/ssd1306.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

int main()
{
    stdio_init_all();

    while (true)
    {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
