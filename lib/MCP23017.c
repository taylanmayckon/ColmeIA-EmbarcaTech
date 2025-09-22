#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "MCP23017.h"

void write_register(uint8_t MCP23017_address, uint8_t reg, uint8_t value){
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(I2C_PORT, MCP23017_address, data, 2, false);
}


uint8_t read_register(uint8_t MCP23017_address, uint8_t reg){
    uint8_t value;
    i2c_write_blocking(I2C_PORT, MCP23017_address, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MCP23017_address, &value, 1, false);
    return value;
}


void MCP23017_init(MCP23017 *expander){
    // Definindo direçao dos pinos e habilitando pull-ups
    write_register(expander->address, MCP_IODIRA, expander->portA.iodir);
    write_register(expander->address, MCP_GPPUA, expander->portA.iodir); 
    write_register(expander->address, MCP_IODIRB, expander->portB.iodir);
    write_register(expander->address, MCP_GPPUB, expander->portB.iodir); 

    // Configurando interrupções
    // Ativa o modo espelhado: MIRROR = 1 (bit 6)
    write_register(expander->address, MCP_IOCON, 0b01000000);

    // Habilita interrupções para PORTA e PORTB
    write_register(expander->address, MCP_IODIRA, 0xFF);
    write_register(expander->address, MCP_GPPUA, 0xFF);
    write_register(expander->address, MCP_GPINTENA, 0xFF);

    write_register(expander->address, MCP_IODIRB, 0xFF);
    write_register(expander->address, MCP_GPPUB, 0xFF);
    write_register(expander->address, MCP_GPINTENB, 0xFF);
    
    // Limpa interrupções pendentes
    read_register(expander->address, MCP_IODIRA);
    read_register(expander->address, MCP_IODIRB);
}


void MCP23017_read_gpio(MCP23017 *expander){
    expander->portA.state = read_register(expander->address, MCP_GPIOA);
    expander->portB.state = read_register(expander->address, MCP_GPIOB);
}