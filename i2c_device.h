#ifndef I2C_DEVICE_H
#define I2C_DEVICE_H

#include <QObject>
#include <linux/i2c-dev.h>

class I2C_Device : public QObject
{
    Q_OBJECT
public:
    explicit I2C_Device(QObject *parent = nullptr);

private:
    int i2cbus;

signals:
};

#endif // I2C_DEVICE_H
