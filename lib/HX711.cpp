#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "HX711.h"
#include "hx711.pio.h"


HX711::HX711(PIO pio, uint sm, uint offset, uint pin_data, uint pin_clock)
    : _pio(pio), _sm(sm), _offset(offset), _pin_data(pin_data), _pin_clock(pin_clock){};

void HX711::begin(){
    // Pino Data configurado como Input
    pio_gpio_init(_pio, _pin_data);
    
    // Pino Clock configurado como Output
    pio_gpio_init(_pio, _pin_clock);
    gpio_set_dir(_pin_clock, GPIO_OUT);

    // Configuracao padrao da SM
    pio_sm_config c = hx711_program_get_default_config(_offset);

    // Mapeando pinos da SM
    sm_config_set_in_pins(&c, _pin_data);
    sm_config_set_sideset_pins(&c, _pin_clock);

    // 125MHz/125 = 1MHz
    float div = clock_get_hz(clk_sys) / 1000000.0;
    sm_config_set_clkdiv(&c, div);

    // Iniciando a SM
    pio_sm_init(_pio, _sm, _offset, &c);
    pio_sm_set_enabled(_pio, _sm, true);
}


int32_t HX711::read_raw(uint8_t gain_pulses){
    pio_sm_put_blocking(_pio, _sm, gain_pulses);

    uint32_t raw = pio_sm_get_blocking(_pio, _sm);

    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }
    return (int32_t)raw;
}


void HX711::tare(int readings){
    int64_t sum = 0;
    for(int i = 0; i<readings; i++)
        sum += read_raw();
    _offset_value = sum/readings;
}


void HX711::set_scale(float scale){
    _scale = scale;
}


float HX711::get_units(int readings){
    if(readings==1){
        return (read_raw() - _offset_value) / _scale;
    }
    int64_t sum = 0;
    for(int i = 0; i<readings; i++)
        sum += read_raw();
    return ((sum/readings) - _offset_value) / _scale;
}