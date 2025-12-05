#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "HX711.h"
#include "hx711.pio.h"


HX711::HX711(uint pin_data, uint pin_clock)
    : _pin_data(pin_data), _pin_clock(pin_clock){
    _scale = 1.0f;
    _offset_value = 0;
};

void HX711::begin(PIO pio, uint sm, uint offset){
    _pio = pio;
    _sm = sm;
    _offset = offset;

    // Inicializa os pinos
    gpio_init(_pin_data);
    gpio_set_dir(_pin_data, GPIO_IN);
    gpio_pull_up(_pin_data);
    
    gpio_init(_pin_clock);
    gpio_set_dir(_pin_clock, GPIO_OUT);
    gpio_put(_pin_clock, 0);
    
    vTaskDelay(pdMS_TO_TICKS(100)); 
    
    // Força uma leitura "dummy" para acordar o HX711
    // Dá 25-27 pulsos de clock para resetar o estado interno
    for(int i = 0; i < 27; i++) {
        gpio_put(_pin_clock, 1);
        vTaskDelay(pdMS_TO_TICKS(1)); 
        gpio_put(_pin_clock, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Aguarda estabilização
    
    // Verifica se está pronto
    printf("Verificando se HX711 está pronto...\n");
    int timeout = 0;
    while(gpio_get(_pin_data) == 1 && timeout < 50) {
        vTaskDelay(pdMS_TO_TICKS(10));
        timeout++;
    }
    
    if(gpio_get(_pin_data) == 1) {
        printf("AVISO: HX711 ainda não está pronto (DOUT=HIGH)\n");
    } else {
        printf("HX711 está pronto! (DOUT=LOW)\n");
    }

    // Configura o PIO
    pio_sm_config c = hx711_program_get_default_config(_offset);
    sm_config_set_in_pins(&c, _pin_data);
    sm_config_set_set_pins(&c, _pin_clock, 1); // Usando SET ao invés de SIDESET

    float div = clock_get_hz(clk_sys) / 500000.0f;
    sm_config_set_clkdiv(&c, div);
    sm_config_set_in_shift(&c, false, false, 24);

    // Transfere controle para o PIO
    pio_gpio_init(_pio, _pin_data);
    pio_gpio_init(_pio, _pin_clock);
    pio_sm_set_consecutive_pindirs(_pio, _sm, _pin_clock, 1, true);

    pio_sm_init(_pio, _sm, _offset, &c);
    pio_sm_set_enabled(_pio, _sm, true);
    
    printf("HX711 inicializado no PIO (DATA=GPIO%d, CLK=GPIO%d)\n", _pin_data, _pin_clock);
}


int32_t HX711::read_raw(uint8_t gain_pulses){
    // Ganho 128 (canal A) = 1 pulso  -> enviar 0
    // Ganho 32  (canal B) = 2 pulsos -> enviar 1
    // Ganho 64  (canal A) = 3 pulsos -> enviar 2
    
    if(gain_pulses == 0) gain_pulses = 1; // Padrão: ganho 128
    
    // Envia (pulsos - 1) porque o PIO faz jmp x--
    pio_sm_put_blocking(_pio, _sm, gain_pulses - 1);

    // Recebe os 24 bits
    uint32_t raw = pio_sm_get_blocking(_pio, _sm);

    // Extensão de sinal: 24 bits -> 32 bits (complemento de 2)
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }
    
    return (int32_t)raw;
}


void HX711::tare(int readings){
    int64_t sum = 0;
    for(int i = 0; i<readings; i++){
        sum += read_raw();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
        
    _offset_value = sum/readings;
}


void HX711::set_scale(float scale){
    _scale = scale;
}


float HX711::get_units(int readings){
    int64_t sum = 0;
    for (int i = 0; i < readings; i++) {
        sum += read_raw();
         // O HX711 roda a 10Hz ou 80Hz. Ler rapido demais pega o mesmo valor.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    float average = (float)(sum / readings);
    
    // Formula: (ValorLido - Tara) / Escala
    _last_weight = (average - _offset_value) / _scale;
    return _last_weight;
}


float HX711::get_last_weight(){
    return _last_weight;
}


float HX711::calibrate_auto(float known_weight, int readings){
    // O peso (known_weight) tem que ser em gramas (g)
    tare(readings);

    int64_t sum = 0;
    for(int i=0; i<readings; i++)
        sum+=read_raw();
    
    float raw_units = (sum/readings) - _offset_value;
    _scale = raw_units/known_weight;
    return _scale;    
}


float HX711::calbirate_manual(float known_weight, int readings){
    printf("\nRetire todos os pesos da balança.\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    printf("Tarando...\n");
    tare(readings);
    printf(">>> Offset (tara) = %ld\n", _offset_value); // DEBUG
    
    printf("Coloque o peso de %.2f g.\n", known_weight);
    vTaskDelay(pdMS_TO_TICKS(5000)); 

    printf("Lendo valor com peso...\n");
    
    int64_t sum = 0;
    for(int i = 0; i < 30; i++){
        int32_t raw = read_raw();
        printf("  Leitura %d: %ld\n", i, raw); // DEBUG 
        sum += raw;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    float raw_average_with_weight = (float)(sum / 30);
    printf(">>> Media com peso = %.2f\n", raw_average_with_weight); // DEBUG

    float delta = raw_average_with_weight - _offset_value;
    
    if(known_weight != 0){
        _scale = delta / known_weight;
    } else {
        _scale = 1.0;
    }

    printf("Calibrado! Delta RAW: %.2f | Scale: %.6f\n", delta, _scale);
    return _scale;
}