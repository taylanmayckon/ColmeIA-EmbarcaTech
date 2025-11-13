#include "MCP23017.h"

#include "hardware/i2c.h"

MCP23017::MCP23017(uint8_t addr, int int_pin) : address(addr), interrupt_pin(int_pin){
    this->portA.iodir = 0b11111111; // Todos como entrada
    this->portB.iodir = 0b11111111; // Todos como entrada
}

void MCP23017::init(){
    // Definindo a direcao dos pinos e habilitando pull-ups
    writeRegister(MCP_IODIRA, portA.iodir);
    writeRegister(MCP_GPPUA, portA.iodir);
    writeRegister(MCP_IODIRB, portB.iodir);
    writeRegister(MCP_GPPUB, portB.iodir);

    // Configurando interrupçoes
    // Ativa o modo espalhado: MIRROR = 1 (bit 6)
    writeRegister(MCP_IOCON, 0b01000000);
    // Habilita interrupções para PORTA e PORTB
    writeRegister(MCP_IODIRA, 0xFF);
    writeRegister(MCP_GPPUA, 0xFF);
    writeRegister(MCP_GPINTENA, 0xFF);
    writeRegister(MCP_INTCONA, 0x00); // habilita a comparação com o registrador "DEF_VAL"
    writeRegister(MCP_DEFVALA, 0xFF); // Definir DEFVAL = 1 (interrupção quando GPA0 cair para 0 → borda de descida)

    writeRegister(MCP_IODIRB, 0xFF);
    writeRegister(MCP_GPPUB, 0xFF);
    writeRegister(MCP_GPINTENB, 0xFF);
    writeRegister(MCP_INTCONB, 0x00); // habilita a comparação com o registrador "DEF_VAL"
    writeRegister(MCP_DEFVALB, 0xFF);

    // Limpa interrupcoes pendentes
    readRegister(MCP_IODIRA);
    readRegister(MCP_IODIRB);
}

void MCP23017::writeRegister(uint8_t reg, uint8_t value){
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(I2C_PORT, address, data, 2, false);
}

uint8_t MCP23017::readRegister(uint8_t reg){
    uint8_t value;
    i2c_write_blocking(I2C_PORT, address, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, address, &value, 1, false);
    return value;
}

void MCP23017::readGPIO(){
    portA.state = readRegister(MCP_GPIOA);
    portB.state = readRegister(MCP_GPIOB);
}

uint8_t MCP23017::getPortAState(){
    return portA.state;
}

uint8_t MCP23017::getPortBState(){
    return portB.state;
}

uint8_t MCP23017::getIntfA(){
    return intfA;
}

uint8_t MCP23017::getIntfB(){
    return intfB;
}

uint8_t MCP23017::getCapA(){
    return capA;
}

uint8_t MCP23017::getCapB(){
    return capB;
}

uint8_t MCP23017::getAddress(){
    return address;
}

uint8_t MCP23017::getInterruptPin(){
    return interrupt_pin;
}

void MCP23017::handle_flags(){
    intfA = readRegister(MCP_INTFA);
    intfB = readRegister(MCP_INTFB);
    capA = readRegister(MCP_INTCAPA);
    capB = readRegister(MCP_INTCAPB);
}
