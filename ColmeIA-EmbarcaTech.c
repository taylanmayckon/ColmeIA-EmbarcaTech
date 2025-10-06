#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "MCP23017.h"

// Configurações da I2C 
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

// Endereços dos Expansores MCP23017
#define EXPANDER1_ADDR 0x20
#define EXPANDER1_INT_PIN 9

// Timeout para resetar o estado (ms)
#define SENSOR_TIMEOUT_MS 2000

#define PASSING_INTERVAL 1000

#define COUNTER_TIMEOUT 3000

// Estados de passagem de cada par de sensores
typedef enum {
    STATE_IDLE,           // Nenhum sensor ativado
    STATE_ENTRANCE,       // Sensor A ativado (entrada)
    STATE_COMPLETED       // Ambos sensores ativados (passagem completa)
} sensor_state_t;

// Estrutura para rastrear cada par de sensores
typedef struct {
    sensor_state_t state;
    TickType_t last_trigger_time;
} sensor_pair_t;

// Expansores conectados
MCP23017 expander1;

// Array de 8 pares de sensores (A0-B0, A1-B1, ... A7-B7)
sensor_pair_t sensor_pairs[8];

// Contador principal de abelhas entrando
uint32_t bee_counter_in = 0;

// Semáforo para sinalizar interrupção
SemaphoreHandle_t xSemaphoreInt;

// Mutex para proteger acesso ao contador
SemaphoreHandle_t xMutexCounter;

// Função para resetar pares de sensores que ultrapassaram o timeout
void check_sensor_timeouts(void) {
    TickType_t current_time = xTaskGetTickCount();
    
    for(int i = 0; i < 8; i++) {
        if(sensor_pairs[i].state != STATE_IDLE) {
            TickType_t elapsed = current_time - sensor_pairs[i].last_trigger_time;
            if(elapsed > pdMS_TO_TICKS(SENSOR_TIMEOUT_MS)) {
                printf("Timeout no par de sensores %d - resetando estado\n", i);
                sensor_pairs[i].state = STATE_IDLE;
            }
        }
    }
}

void handle_flags(MCP23017 *expander){
    uint8_t flagA = read_register(expander->address, MCP_INTFA);
    uint8_t flagB = read_register(expander->address, MCP_INTFB);
    
    TickType_t current_time = xTaskGetTickCount();

    // Processa sensores da PORTA A (entrada da colmeia)
    if(flagA){
        uint8_t captured_value = read_register(expander->address, MCP_INTCAPA);
        
        for(int i = 0; i < 8; i++) {
            if (flagA & (1 << i)) {
                // Verifica se foi borda de descida (sensor ativado)
                if ((captured_value & (1 << i)) == 0) {
                    printf("Sensor A%d ativado (ENTRADA)\n", i);
                    
                    // Atualiza o estado do par de sensores
                    if(sensor_pairs[i].state == STATE_IDLE) {
                        sensor_pairs[i].state = STATE_ENTRANCE;
                        sensor_pairs[i].last_trigger_time = current_time;
                        printf("  -> Aguardando sensor B%d para confirmar passagem\n", i);
                    }
                }
            }
        }
    }

    // Processa sensores da PORTA B (centro da colmeia)
    if(flagB){
        uint8_t captured_value = read_register(expander->address, MCP_INTCAPB);
        
        for(int i = 0; i < 8; i++) {
            if (flagB & (1 << i)) {
                // Verifica se foi borda de descida (sensor ativado)
                if ((captured_value & (1 << i)) == 0) {
                    printf("Sensor B%d ativado (CENTRO)\n", i);
                    
                    // Verifica se o sensor A correspondente foi ativado antes
                    if(sensor_pairs[i].state == STATE_ENTRANCE) {
                        // Passagem completa detectada!
                        sensor_pairs[i].state = STATE_COMPLETED;
                        
                        // Incrementa o contador com proteção de mutex
                        if(xSemaphoreTake(xMutexCounter, portMAX_DELAY) == pdTRUE) {
                            bee_counter_in++;
                            printf("*** ABELHA DETECTADA NO CANAL %d! Total: %lu ***\n", i, bee_counter_in);
                            xSemaphoreGive(xMutexCounter);
                        }
                        
                        // Reseta o estado após um delay para evitar contagem duplicada
                        vTaskDelay(pdMS_TO_TICKS(COUNTER_TIMEOUT));
                        sensor_pairs[i].state = STATE_IDLE;
                    } else {
                        printf("  -> Sensor B%d ativado mas A%d não foi ativado antes (ignorado)\n", i, i);
                    }
                }
            }
        }
    }
}

// ISR - apenas sinaliza o semáforo
void gpio_irq_handler(uint gpio, uint32_t events){
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(gpio == EXPANDER1_INT_PIN) {
        xSemaphoreGiveFromISR(xSemaphoreInt, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void vExpander1(void *params) {
    expander1.address = EXPANDER1_ADDR;
    expander1.interrupt_pin = EXPANDER1_INT_PIN;
    expander1.portA.iodir = 0b11111111; // Todos como entrada
    expander1.portB.iodir = 0b11111111; // Todos como entrada

    // Inicializa todos os pares de sensores
    for(int i = 0; i < 8; i++) {
        sensor_pairs[i].state = STATE_IDLE;
        sensor_pairs[i].last_trigger_time = 0;
    }
    
    xSemaphoreInt = xSemaphoreCreateBinary();
    xMutexCounter = xSemaphoreCreateMutex();

    MCP23017_init(&expander1);

    // Limpa qualquer interrupção pendente
    read_register(expander1.address, MCP_INTCAPA);
    read_register(expander1.address, MCP_INTCAPB);

    gpio_set_irq_enabled_with_callback(expander1.interrupt_pin, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    printf("Sistema de contagem de abelhas inicializado!\n");
    printf("Aguardando deteccoes...\n\n");

    while (true) {
        // Aguarda sinal de interrupção
        if(xSemaphoreTake(xSemaphoreInt, pdMS_TO_TICKS(100)) == pdTRUE) {
            handle_flags(&expander1);
        }
        
        // Verifica timeouts periodicamente
        check_sensor_timeouts();
        
        vTaskDelay(pdMS_TO_TICKS(PASSING_INTERVAL));
    }
}

// Task opcional para exibir estatísticas
// void vStatistics(void *params) {
//     while(true) {
//         vTaskDelay(pdMS_TO_TICKS(10000)); // A cada 10 segundos
        
//         if(xSemaphoreTake(xMutexCounter, portMAX_DELAY) == pdTRUE) {
//             printf("\n=== ESTATISTICAS ===\n");
//             printf("Total de abelhas detectadas: %lu\n", bee_counter_in);
//             printf("====================\n\n");
//             xSemaphoreGive(xMutexCounter);
//         }
//     }
// }

int main(){
    stdio_init_all();
    
    bee_counter_in = 0;

    // Iniciando o I2C 
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Cria as tasks
    xTaskCreate(vExpander1, "Sensor Monitor", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
    //xTaskCreate(vStatistics, "Statistics", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}