#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <linux/i2c.h>
#include "mp2722.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    struct i2c_adap *adapters;
    Ui::MainWindow *ui;
    int i2cBusHook;
    MP2722 *MP2722Dev;

private slots:
    void openI2cBus();
    void closeI2cBus();
    void refreshI2cBusList();
    void readRegs();
    void writeReg();
};
#endif // MAINWINDOW_H
