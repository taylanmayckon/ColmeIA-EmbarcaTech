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

// Endereços e GPIO da ISR dos Expansores MCP23017
#define EXPANDER1_ADDR 0x20
#define EXPANDER1_INT_PIN 9
// Tamanho da fila de cada par de sensor
#define BEE_QUEUE_LENGTH 5
#define NUM_CHANNELS_MCP 8
// Timeouts para os estados das abelhas (ms)
#define BEE_EVENT_TIMEOUT_MS 5000 // Tempo para descartar abelhas que nao completam a passagem
#define BEE_PASSAGE_WINDOW_MS 2000 // Janela para aceitar uma passagem de abelha (A->B e B->A)

// Expansores conectados
MCP23017 expander1;
// Filas para cada expansor
QueueHandle_t beeQueue1[2][NUM_CHANNELS_MCP]; // [0][X] PortA e [1][X] PortB
// Semáforo para sinalizar interrupção de cada expansor
SemaphoreHandle_t xSemaphoreInt1;

// Contador principal de abelhas entrando
uint32_t bee_counter = 0;

// Mutex para proteger acesso ao contador
SemaphoreHandle_t xMutexCounter;

void bee_update_queues(MCP23017 *expander, QueueHandle_t beeQueue[2][8]){
    // Funcao para analisar as flags de interrupçao e popular as filas
    uint8_t flagA = expander->intfA;
    uint8_t flagB = expander->intfB;
    TickType_t current_time = xTaskGetTickCount();

    // Processa sensores da PORTA A (entrada da colmeia)
    if(flagA){
        for(int i = 0; i < 8; i++) {
            if (flagA & (1 << i)) {
                // Verifica se foi borda de descida (sensor ativado)
                if ((expander->capA & (1 << i)) == 0) {
                    printf("Sensor A%d ativado (ENTRADA DA COLMEIA)\n", i);
                    if(xQueueSend(beeQueue[0][i], &current_time, 0) != pdPASS)
                        printf("\n[QUEUE SEND] Erro ao registrar dado do Expansor %d, pino A%d\n.", expander->address, i);
                }
            }
        }
    }
    // Processa sensores da PORTA B (dentro da colmeia)
    if(flagB){
        for(int i = 0; i < 8; i++) {
            if (flagB & (1 << i)) {
                if((expander->capB & (1 << i)) == 0){
                    printf("Sensor B%d ativado (DENTRO DA COLMEIA)\n", i);
                    if(xQueueSend(beeQueue[1][i], &current_time, 0) != pdPASS)
                        printf("\n[QUEUE SEND] Erro ao registrar dado do Expansor %d, pino B%d.\n", expander->address, i);
                }
            }
        }
    }
}


// Atualiza as flags de interrupcao do expansor acionado
void exp_handle_flags(MCP23017 *expander){
    expander->intfA = read_register(expander->address, MCP_INTFA);
    expander->intfB = read_register(expander->address, MCP_INTFB);
    expander->capA  = read_register(expander->address, MCP_INTCAPA);
    expander->capB  = read_register(expander->address, MCP_INTCAPB);
}

// ISR - apenas sinaliza o semáforo de cada expansor
void gpio_irq_handler(uint gpio, uint32_t events){
    // Aplica um debounce por GPIO
    // last_gpio e last_int_timestamp no caso, pra impedir que acione varias vezes a interrupcao
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(gpio == EXPANDER1_INT_PIN) {
        xSemaphoreGiveFromISR(xSemaphoreInt1, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void vExpander1(void *params) {
    // Essa funcao só serve pra capturar as interrupcoes e adicionar valores nas filas
    // Entao ela só precisa dessa parte de checar o semaforo da propria interrupcao no while(true)
    // O tratamento das filas fica para outra task
    expander1.address = EXPANDER1_ADDR;
    expander1.interrupt_pin = EXPANDER1_INT_PIN;
    expander1.portA.iodir = 0b11111111; // Todos como entrada
    expander1.portB.iodir = 0b11111111; // Todos como entrada

    // Inicializa as filas dos pares de sensores
    // beeQueue[PORT][CANAL], uma fila para cada canal (Entrada/Saida)
    for(int i = 0; i < NUM_CHANNELS_MCP; i++) {
        for(int j = 0; j<2; j++){
            beeQueue1[j][i] = xQueueCreate(BEE_QUEUE_LENGTH, sizeof(TickType_t));
            if (beeQueue1[j][i] == NULL) {
                printf("Erro ao criar a fila para o sensor %d do port %d do expansor %d!\n", i, j, expander1.address);
            }
        }
    }
    
    // Inicializando o semáforo para a interrupçao desse expansor
    xSemaphoreInt1 = xSemaphoreCreateBinary();
    MCP23017_init(&expander1);
    // Limpa qualquer interrupção pendente
    exp_handle_flags(&expander1);

    gpio_set_irq_enabled_with_callback(expander1.interrupt_pin, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true) {
        // Aguarda sinal de interrupção
        if(xSemaphoreTake(xSemaphoreInt1, portMAX_DELAY) == pdTRUE) {
            exp_handle_flags(&expander1);
            bee_update_queues(&expander1, beeQueue1);
        }
    }
}




void consume_individual_expander_queue(QueueHandle_t beeQueue[2][8]){
    TickType_t entry_time;
    TickType_t exit_time;

    // Iteraqndo pelos 8 canais do expansor
    for(int channel = 0; channel < NUM_CHANNELS_MCP; channel++){
        // Verificando os novos eventos de cada fila do canal atual
        // O xQueuePeek pega o dado sem consumir a fila
        if(xQueuePeek(beeQueue[0][channel], &entry_time, 0) == pdPASS && xQueuePeek(beeQueue[1][channel], &exit_time, 0) == pdPASS){
            // ENTRADA (Evento A acontece antes do evento B)
            if(entry_time < exit_time){
                // Dentro do timeout?
                if((exit_time - entry_time) <= pdMS_TO_TICKS(BEE_PASSAGE_WINDOW_MS)){
                    // Entrada válida
                    if(xSemaphoreTake(xMutexCounter, portMAX_DELAY) == pdTRUE){
                        bee_counter++;
                        // printf("[ENTRADA VÁLIDA] no canal %d! Total: %lu\n", channel, bee_counter);
                        xSemaphoreGive(xMutexCounter);
                    }
                    // Removendo os eventos processados
                    xQueueReceive(beeQueue[0][channel], &entry_time, 0);
                    xQueueReceive(beeQueue[1][channel], &exit_time, 0);
                }
                else{
                    // O evento de entrada é muito antigo, já descarta o A
                    // printf("[TIMEOUT] Canal A%d.\n", channel);
                    xQueueReceive(beeQueue[0][channel], &entry_time, 0);
                }
            }
        
            // SAIDA (Evento B acontece antes do evento A)
            else{ // exit_time < entry_time
                // Dentro do timeout?
                if((entry_time - exit_time) <= pdMS_TO_TICKS(BEE_PASSAGE_WINDOW_MS)){
                    // Saida valida
                    if(xSemaphoreTake(xMutexCounter, portMAX_DELAY) == pdTRUE){
                        // NOTA: Será que isso vai bugar o código? E se uma abelha sai na hora que o sistema liga?
                        // Por seguranca vou colocar uma validacao se o contador nao é 0
                        if(bee_counter)
                            bee_counter--;
                        // printf("[FILA - SAIDA VÁLIDA] No canal %d! Total: %lu\n", channel, bee_counter);
                        xSemaphoreGive(xMutexCounter);
                    }
                    // Removendo os eventos processados
                    xQueueReceive(beeQueue[0][channel], &entry_time, 0);
                    xQueueReceive(beeQueue[1][channel], &exit_time, 0);
                }
                else{
                    // Evento de entrada muito antigo, descarta B
                    // printf("[FILA - TIMEOUT] Canal B%d.\n", channel);
                    xQueueReceive(beeQueue[1][channel], &exit_time, 0);
                }
            }
        }

        // Se nao tiver nenhum par pra comparar limpa os eventos da fila
        else{
            TickType_t current_time = xTaskGetTickCount();
            // Evento de entrada antigo
            if(xQueuePeek(beeQueue[0][channel], &entry_time, 0) == pdPASS){
                if((current_time - entry_time) > pdMS_TO_TICKS(BEE_EVENT_TIMEOUT_MS)){
                    // printf("[FILA - TIMEOUT] Limpando evento de entrada do canal A%d\n", channel);
                    xQueueReceive(beeQueue[0][channel], &entry_time, 0); // Remove da fila
                }
            }
            // Evento de saida antigo
            if(xQueuePeek(beeQueue[1][channel], &exit_time, 0) == pdPASS){
                if((current_time - exit_time) > pdMS_TO_TICKS(BEE_EVENT_TIMEOUT_MS)){
                    // printf("[FILA - TIMEOUT] Limpando evento de saida do canal B%d\n", channel);
                    xQueueReceive(beeQueue[1][channel], &exit_time, 0); // Remove da fila
                }
            }
        }
    }
}

void vBeeConsumeQueuesTask(void *params){
    // Task para limpar as filas de cada expansor separadamente
    while(true){
        consume_individual_expander_queue(beeQueue1);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



// Task opcional para exibir estatísticas
void vStatistics(void *params) {
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // A cada 10 segundos
        
        if(xSemaphoreTake(xMutexCounter, portMAX_DELAY) == pdTRUE) {
            printf("\n=== ESTATISTICAS ===\n");
            printf("Total de abelhas detectadas: %lu\n", bee_counter);
            printf("====================\n\n");
            xSemaphoreGive(xMutexCounter);
        }
    }
}

int main(){
    stdio_init_all();
    
    bee_counter = 0;

    // Iniciando o I2C 
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Mutex para acesso do contador de abelhas
    xMutexCounter = xSemaphoreCreateMutex();

    // Cria as tasks
    xTaskCreate(vExpander1, "Expansor1", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
    xTaskCreate(vBeeConsumeQueuesTask, "Bee Consume Queues", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
    
    // Task opcional para debug
    //xTaskCreate(vStatistics, "Statistics", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
    
    vTaskStartScheduler();
    panic_unsupported();
}