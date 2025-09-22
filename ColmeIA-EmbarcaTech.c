#include <stdio.h>
#include "pico/stdlib.h"
// Includes para manipular a GPIO
#include "hardware/i2c.h"
// Includes do FreeRTOS
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
// Includes das Libs
#include "MCP23017.h"

// Configurações da I2C 
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

// Endereços dos Expansores MCP23017
#define EXPANDER1_ADDR 0x20


void vExpander1(void *params) {
    MCP23017 expander1;
    expander1.address = EXPANDER1_ADDR;
    expander1.portA.iodir = 0b11000000;
    expander1.portB.iodir = 0b00000000;

    MCP23017_init(&expander1);

    while (true) {
        MCP23017_read_gpio(&expander1);

        // Verifica o GPA0
        if (expander1.portA.state & (1 << GPA0)) {
            printf("Pino GPA%d: HIGH\n", GPA0);
        } else {
            printf("Pino GPA%d: LOW\n", GPA0);
        }

        // Verifica o GPA1
        if (expander1.portA.state & (1 << GPA1)) {
            printf("Pino GPA%d: HIGH\n", GPA1);
        } else {
            printf("Pino GPA%d: LOW\n", GPA1);
        }

        printf("\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


int main(){
    stdio_init_all();

    // Iniciando o I2C 
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    xTaskCreate(vExpander1, "Task o expansor 1", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL); 

    vTaskStartScheduler();
    panic_unsupported();
}
