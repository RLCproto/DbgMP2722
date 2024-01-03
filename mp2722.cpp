#include "mp2722.h"

extern "C"
{
#include <i2c/smbus.h>
#include "i2cbusses.h"
}

MP2722::MP2722(int i2cBusHook, QObject *parent)
    : QObject{parent}
{
    this->i2cBusHook = i2cBusHook;
}

__uint8_t MP2722::getRegValue(int regNr)
{
    set_slave_addr(i2cBusHook, MP2722_ADDRESS, 0);
    return i2c_smbus_read_byte_data(i2cBusHook, regNr);
}

void MP2722::setRegValue(int regNr, char newValue)
{
    set_slave_addr(i2cBusHook, MP2722_ADDRESS, 0);
    i2c_smbus_write_byte_data(i2cBusHook, regNr, newValue);
}
