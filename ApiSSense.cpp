#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "MCP23017.h"
#include "HX711.h"

#include "pico/cyw43_arch.h"        // Biblioteca para arquitetura Wi-Fi da Pico com CYW43  
#include "lwip/apps/mqtt.h"         // Biblioteca LWIP MQTT -  fornece funções e recursos para conexão MQTT
#include "lwip/apps/mqtt_priv.h"    // Biblioteca que fornece funções e recursos para Geração de Conexões
#include "lwip/dns.h"               // Biblioteca que fornece funções e recursos suporte DNS:
#include "lwip/altcp_tls.h"         // Biblioteca que fornece funções e recursos para conexões seguras usando TLS:

// Configurações da I2C 
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

// --- EXPANSORES (MCP23017) --- 
// Endereços e GPIO da ISR dos Expansores MCP23017
#define EXPANDER1_ADDR 0x20
#define EXPANDER1_INT_PIN 9
// Tamanho da fila de cada par de sensor
#define BEE_QUEUE_LENGTH 5
#define NUM_CHANNELS_MCP 8
// Timeouts para os estados das abelhas (ms)
#define BEE_EVENT_TIMEOUT_MS 5000 // Tempo para descartar abelhas que nao completam a passagem
#define BEE_PASSAGE_WINDOW_MS 2000 // Janela para aceitar uma passagem de abelha (A->B e B->A)

// Expansores (MCP23017) conectados
MCP23017 expander1(EXPANDER1_ADDR, EXPANDER1_INT_PIN);
// Filas para cada expansor
QueueHandle_t beeQueue1[2][NUM_CHANNELS_MCP]; // [0][X] PortA e [1][X] PortB
// Semáforo para sinalizar interrupção de cada expansor
SemaphoreHandle_t xSemaphoreInt1;

// Contador principal de abelhas entrando
int32_t bee_counter = 0;

// Mutex para proteger acesso ao contador
SemaphoreHandle_t xMutexCounter;


// --- Favos de Mel (LOADCELL) ---
// Loadcell 1
#define loadcell1_dt 19
#define loadcell1_sck 18
#define loadcell1_scale 26.598213f

HX711 loadcell1(loadcell1_dt, loadcell1_sck);


void bee_update_queues(MCP23017 expander, QueueHandle_t beeQueue[2][8]){
    // Funcao para analisar as flags de interrupçao e popular as filas
    uint8_t flagA = expander.getIntfA();
    uint8_t flagB = expander.getIntfB();
    TickType_t current_time = xTaskGetTickCount();

    // Processa sensores da PORTA A (entrada da colmeia)
    if(flagA){
        for(int i = 0; i < 8; i++) {
            if (flagA & (1 << i)) {
                // Verifica se foi borda de descida (sensor ativado)
                if ((expander.getCapA() & (1 << i)) == 0) {
                    printf("%s: Sensor A%d ativado (ENTRADA DA COLMEIA)\n", pcTaskGetName(NULL), i);
                    if(xQueueSend(beeQueue[0][i], &current_time, 0) != pdPASS)
                        printf("\n%s: [QUEUE] Erro ao registrar dado do Expansor 0x%X, pino A%d\n.", pcTaskGetName(NULL), expander.getAddress(), i);
                }
            }
        }
    }
    // Processa sensores da PORTA B (dentro da colmeia)
    if(flagB){
        for(int i = 0; i < 8; i++) {
            if (flagB & (1 << i)) {
                if((expander.getCapB() & (1 << i)) == 0){
                    printf("%s: Sensor B%d ativado (DENTRO DA COLMEIA)\n", pcTaskGetName(NULL), i);
                    if(xQueueSend(beeQueue[1][i], &current_time, 0) != pdPASS)
                        printf("\n%s: [QUEUE] Erro ao registrar dado do Expansor 0x%X, pino B%d.\n", pcTaskGetName(NULL), expander.getAddress(), i);
                }
            }
        }
    }
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

    // Inicializa as filas dos pares de sensores
    // beeQueue[PORT][CANAL], uma fila para cada canal (Entrada/Saida)
    for(int i = 0; i < NUM_CHANNELS_MCP; i++) {
        for(int j = 0; j<2; j++){
            beeQueue1[j][i] = xQueueCreate(BEE_QUEUE_LENGTH, sizeof(TickType_t));
            if (beeQueue1[j][i] == NULL) {
                printf("%s: Erro ao criar a fila para o sensor %d do port %d do expansor %d!\n", pcTaskGetName(NULL), i, j);
            }
        }
    }
    
    // Inicializando o semáforo para a interrupçao desse expansor
    xSemaphoreInt1 = xSemaphoreCreateBinary();
    // Limpa qualquer interrupção pendente
    expander1.handle_flags();

    gpio_set_irq_enabled_with_callback(expander1.getInterruptPin(), GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true) {
        // Aguarda sinal de interrupção
        if(xSemaphoreTake(xSemaphoreInt1, portMAX_DELAY) == pdTRUE) {
            expander1.handle_flags();
            bee_update_queues(expander1, beeQueue1);
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
                        printf("%s: ENTRADA VALIDA no canal %d! Total: %d\n", pcTaskGetName(NULL), channel, bee_counter);
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
                        printf("%s: SAIDA VÁLIDA no canal %d! Total: %d\n", pcTaskGetName(NULL), channel, bee_counter);
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
            printf("Total de abelhas detectadas: %d\n", bee_counter);
            printf("====================\n\n");
            xSemaphoreGive(xMutexCounter);
        }
    }
}


// Task para as Loadcells
void vLoadCellsTask(void *params){
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &hx711_program);
    int sm = pio_claim_unused_sm(pio, true);

    loadcell1.begin(pio, sm, offset);
    loadcell1.set_scale(loadcell1_scale);
    loadcell1.tare(20); 

    while(true){
        loadcell1.get_units(10);
        printf("%s: Peso lido: %.2f g\n", pcTaskGetName(NULL), loadcell1.get_last_weight());
        vTaskDelay(pdMS_TO_TICKS(2000)); 
        // loadcell1.calbirate_manual(224.0f, 20);
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

    // Iniciando os expansores (MCP23017)
    // expander1.init();

    // // Mutex para acesso do contador de abelhas
    // xMutexCounter = xSemaphoreCreateMutex();

    // // Cria as tasks
    // xTaskCreate(vExpander1, "vExpander1", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
    // xTaskCreate(vBeeConsumeQueuesTask, "vBeeConsumeQueuesTask", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
    xTaskCreate(vLoadCellsTask, "vLoadCellsTask", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
    
    // Task opcional para debug
    // xTaskCreate(vStatistics, "Statistics", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
    
    vTaskStartScheduler();
    panic_unsupported();
}