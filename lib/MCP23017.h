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


typedef struct{
    // Struct para armazenar as informações de um conjunto de pinos (PORTA ou PORTB)
    uint8_t iodir; // Direçao dos pinos (1=entrada, 0=saida)
    uint8_t state; // Estado atual dos pinos (1=alto, 0=baixo)
} MCP23017_PortInfo;

class MCP23017{
    private:
        uint8_t address; // Endereco I2C do MCP23017
        int interrupt_pin; // Pino de interrupcao conectado ao MCP23017
        MCP23017_PortInfo portA; // Informações da PORTA
        MCP23017_PortInfo portB; // Informações da PORTB
        uint8_t intfA, intfB;
        uint8_t capA, capB; 
    public:
        // Construtor
        MCP23017(uint8_t addr, int int_pin);

        // Métodos
        // Manipulacao de hardware
        void init(); // Inicializa o MCP23017
        void writeRegister(uint8_t reg, uint8_t value); // Escreve em um registrador
        uint8_t readRegister(uint8_t reg); // Le um registrador
        void readGPIO(); // Le o estado dos pinos GPIO
        // Getters
        uint8_t getPortAState(); // Retorna o estado atual do PortA
        uint8_t getPortBState(); // Retorna o estado atual do PortB
        uint8_t getIntfA(); 
        uint8_t getIntfB();
        uint8_t getCapA();
        uint8_t getCapB();
        uint8_t getAddress();
        uint8_t getInterruptPin();

        void handle_flags(); // Atualiza as flags do PortA e PortB
};


#endif