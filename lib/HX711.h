#ifndef HX711_H
#define HX711_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hx711.pio.h"

class HX711{
    private:
        PIO _pio;
        uint _sm;
        uint _pin_data;
        uint _pin_clock;
        uint _offset; // Offset do programa PIO
        float _scale = 1.0f;
        int32_t _offset_value = 0;
        float _last_weight = 0.0f;

    public:
        // Construtor
        HX711(uint pin_data, uint pin_clock);
        // Metodos
        void begin(PIO pio, uint sm, uint offset);
        int32_t read_raw(uint8_t gain_pulses=1);
        void tare(int readings = 10);
        void set_scale(float scale);
        float get_units(int readings = 1);
        float get_last_weight();
        float calibrate_auto(float known_weight, int readings = 20);
        float calbirate_manual(float known_weight, int readings = 20);
};



#endif