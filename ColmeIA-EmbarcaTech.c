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


// Struct para o evento de passagem de abelha
typedef struct{
    uint8_t sensor_id; // 0-7
    char port; // A ou B
    // Depois define direito se A vai ser entrada, B vai ser saida...
    TickType_t timestamp; 
} BeeEvent_t;

// Expansores conectados
MCP23017 expander1;
// Filas para cada expansor
QueueHandle_t beeQueue1[8];
// Semáforo para sinalizar interrupção de cada expansor
SemaphoreHandle_t xSemaphoreInt1;

// Contador principal de abelhas entrando
uint32_t bee_counter_in = 0;

// Mutex para proteger acesso ao contador
SemaphoreHandle_t xMutexCounter;

// Função para resetar pares de sensores que ultrapassaram o timeout
// void check_sensor_timeouts(void) {
//     TickType_t current_time = xTaskGetTickCount();
//     for(int i = 0; i < 8; i++) {
//         if(sensor_pairs1[i].state != STATE_IDLE) {
//             TickType_t elapsed = current_time - sensor_pairs1[i].last_trigger_time;
//             if(elapsed > pdMS_TO_TICKS(SENSOR_TIMEOUT_MS)) {
//                 printf("Timeout no par de sensores %d - resetando estado\n", i);
//                 sensor_pairs1[i].state = STATE_IDLE;
//             }
//         }
//     }
// }


void bee_consume_queues();

void bee_update_queues(MCP23017 *expander, QueueHandle_t *beeQueue){
    // Funcao para analisar as flags de interrupçao e popular as filas
    uint8_t flagA = read_register(expander->address, MCP_INTFA);
    uint8_t flagB = read_register(expander->address, MCP_INTFB);
    TickType_t current_time = xTaskGetTickCount();
    BeeEvent_t event;

    // Processa sensores da PORTA A (entrada da colmeia)
    if(flagA){
        uint8_t captured_value = read_register(expander->address, MCP_INTCAPA);
        for(int i = 0; i < 8; i++) {
            if (flagA & (1 << i)) {
                // Verifica se foi borda de descida (sensor ativado)
                if ((captured_value & (1 << i)) == 0) {
                    printf("Sensor A%d ativado (ENTRADA DA COLMEIA)\n", i);
                    event.port='A';
                    event.sensor_id=i;
                    event.timestamp=current_time;
                    if(xQueueSend(beeQueue[i], &event, 0) != pdPASS)
                        printf("\n[QUEUE SEND] Erro ao registrar dado do Expansor %d, pino A%d\n.", expander->address, i);
                }
            }
        }
    }
    // Processa sensores da PORTA B (dentro da colmeia)
    if(flagB){
        uint8_t captured_value = read_register(expander->address, MCP_INTCAPB);
        for(int i = 0; i < 8; i++) {
            if (flagB & (1 << i)) {
                if((captured_value & (1 << i)) == 0){
                    printf("Sensor B%d ativado (DENTRO DA COLMEIA)\n", i);
                    event.port='B';
                    event.sensor_id=i;
                    event.timestamp=current_time;
                    if(xQueueSend(beeQueue[i], &event, 0) != pdPASS)
                        printf("\n[QUEUE SEND] Erro ao registrar dado do Expansor %d, pino B%d.\n", expander->address, i);
                }
            }
        }
    }
}

// ISR - apenas sinaliza o semáforo
void gpio_irq_handler(uint gpio, uint32_t events){
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(gpio == EXPANDER1_INT_PIN) {
        xSemaphoreGiveFromISR(xSemaphoreInt1, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void vExpander1(void *params) {
    expander1.address = EXPANDER1_ADDR;
    expander1.interrupt_pin = EXPANDER1_INT_PIN;
    expander1.portA.iodir = 0b11111111; // Todos como entrada
    expander1.portB.iodir = 0b11111111; // Todos como entrada

    // Inicializa as filas dos pares de sensores
    for(int i = 0; i < 8; i++) {
        beeQueue1[i] = xQueueCreate(10, sizeof(BeeEvent_t));
        if (beeQueue1[i] == NULL) {
            printf("Erro ao criar a fila para o sensor %d do expansor %d!\n", i, expander1.address);
        }
    }
    
    // Inicializando o semáforo para a interrupçao desse expansor
    xSemaphoreInt1 = xSemaphoreCreateBinary();
    MCP23017_init(&expander1);
    // Limpa qualquer interrupção pendente
    read_register(expander1.address, MCP_INTCAPA);
    read_register(expander1.address, MCP_INTCAPB);

    gpio_set_irq_enabled_with_callback(expander1.interrupt_pin, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true) {
        // Aguarda sinal de interrupção
        if(xSemaphoreTake(xSemaphoreInt1, portMAX_DELAY) == pdTRUE) {
            bee_update_queues(&expander1, beeQueue1);
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

    // Mutex para acesso do contador de abelhas
    xMutexCounter = xSemaphoreCreateMutex();

    // Cria as tasks
    xTaskCreate(vExpander1, "Sensor Monitor", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
    //xTaskCreate(vStatistics, "Statistics", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}