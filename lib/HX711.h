#ifndef HX711_H
#define HX711_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
// #include "hx711.pio.h"

class HX711{
    private:
        PIO _pio;
        uint _sm;
        uint _pin_data;
        uint _pin_clock;
        uint _offset;
        float _scale = 1.0f;
        int32_t _offset_value = 0;

    public:
        // Construtor
        HX711(PIO pio, uint sm, uint offset, uint pin_data, uint pin_clock);
        // Metodos
        void begin();
        int32_t read_raw(uint8_t gain_pulses=1);
        void tare(int readings = 10);
        void set_scale(float scale);
        float get_units(int readings = 1);
        float calibrate_auto(float known_weight, int readings = 20);
        float calbirate_manual(float known_weight, int readings = 20);
};



#endif