#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <linux/i2c.h>

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

private slots:
    void openI2cBus();
    void closeI2cBus();
    void refreshI2cBusList();
};
#endif // MAINWINDOW_H
