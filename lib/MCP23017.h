#ifndef MCP23017_H
#define MCP23017_H

#include "pico/stdlib.h"

#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

// Pinos do MCP23017
#define GPA0 0
#define GPA1 1          
#define GPA2 2
#define GPA3 3
#define GPA4 4
#define GPA5 5
#define GPA6 6
#define GPA7 7
#define GPB0 8
#define GPB1 9
#define GPB2 10
#define GPB3 11
#define GPB4 12
#define GPB5 13
#define GPB6 14
#define GPB7 15

// Registradores do MCP23017 para PORTA
#define MCP_IODIRA 0x00
#define MCP_GPPUA 0x0C
#define MCP_GPIOA 0x12

// Registradores do MCP23017 para PORTB 
#define MCP_IODIRB 0x01
#define MCP_GPPUB 0x0D
#define MCP_GPIOB 0x13

// Registradores de interrupção
#define MCP_GPINTENA 0x04 // Habilita interrupção por pino para PORTA
#define MCP_GPINTENB 0x05 // Habilita interrupção por pino para PORTB
#define MCP_DEFVALA 0x06 // Valor padrão de comparação
#define MCP_DEFVALB 0x07 // Valor padrão de comparação
#define MCP_INTCONA 0x08 // Controle de comparação da interrupção
#define MCP_INTCONB 0x09 // Controle de comparação da interrupção
#define MCP_INTFA 0x0E // Flags de interrupçao
#define MCP_INTFB 0x0F  // Flags de interrupçao
#define MCP_INTCAPA 0x10 // Captura o estado e limpa a interrupção
#define MCP_INTCAPB 0x11 // Captura o estado e limpa a interrupção

#define MCP_IOCON 0x0A

typedef struct {
    // Struct para armazenar as informaçoes de um conjunto de pinos (PORTA ou PORTB)
    uint8_t iodir; // Direção dos pinos (1 = entrada, 0 = saída)
    uint8_t state; // Estado atual dos pinos (1 = alto, 0 = baixo)
} MCP23017_PortInfo;

typedef struct {
    uint8_t address; // Endereço I2C do MCP23017
    int interrupt_pin; // Pino de interrupção conectado ao MCP23017
    MCP23017_PortInfo portA; // Informações da PORTA
    MCP23017_PortInfo portB; // Informações da PORTB
    uint8_t intfA, intfB;
    uint8_t capA, capB; 
    // TickType_t last_interrupt_time; // Caso queira implementar debounce em cada expansor
} MCP23017;

void write_register(uint8_t MCP23017_address, uint8_t reg, uint8_t value);
uint8_t read_register(uint8_t MCP23017_address, uint8_t reg);
void MCP23017_init(MCP23017 *expander);
void MCP23017_read_gpio(MCP23017 *expander);

#endif