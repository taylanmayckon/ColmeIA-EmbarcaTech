#include <stdio.h>
#include "pico/stdlib.h"
// Includes para manipular a GPIO
#include "hardware/i2c.h"
// Includes do FreeRTOS
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

#define IR_PIN1 8
#define IR_PIN2 9


void vIR_Sensor(void *params) {
    gpio_init(IR_PIN1);
    gpio_set_dir(IR_PIN1, GPIO_IN);
    gpio_init(IR_PIN2);
    gpio_set_dir(IR_PIN2, GPIO_IN);

    while (true) {
        int ir1 = gpio_get(IR_PIN1);
        int ir2 = gpio_get(IR_PIN2);

        printf("IR1: %d, IR2: %d\n", ir1, ir2);

        sleep_ms(1000);
    }
}


int main(){
    stdio_init_all();

    xTaskCreate(vIR_Sensor, "Task para teste dos sensores IR", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL); 

    vTaskStartScheduler();
    panic_unsupported();
}
