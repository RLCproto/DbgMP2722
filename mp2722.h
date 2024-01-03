#ifndef MP2722_H
#define MP2722_H

#include <QObject>

#define MP2722_ADDRESS 0x3F

class MP2722 : public QObject
{
    Q_OBJECT
public:
    explicit MP2722(int i2cBusHook, QObject *parent = nullptr);

    __uint8_t getRegValue(int regNr);
    void setRegValue(int regNr, char newValue);
private:
    int i2cBusHook;
signals:
};

#endif // MP2722_H
