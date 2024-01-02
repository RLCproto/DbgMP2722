#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <linux/i2c-dev.h>
#include <QDebug>

extern "C"
{
#include "i2cbusses.h"
#include <unistd.h>
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::refreshI2cBusList);
    refreshI2cBusList();

    for(int cnt = 0; adapters[cnt].name; cnt++)
    {
        if(ui->I2cBusListComboBox->itemText(cnt) == "MCP2221 usb-i2c bridge")
        {
            ui->I2cBusListComboBox->setCurrentIndex(cnt);
            break;
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openI2cBus()
{
    ui->I2cBusListComboBox->setDisabled(1);
    disconnect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::openI2cBus);
    connect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::closeI2cBus);
    ui->I2cBusOpenPushButton->setText("Close");

    int i2cBus = lookup_i2c_bus(ui->I2cBusListComboBox->currentText().toUtf8().data());

    char i2cPath[20];
    i2cBusHook = open_i2c_dev(i2cBus, i2cPath, 0);
}

void MainWindow::closeI2cBus()
{
    ui->I2cBusListComboBox->setEnabled(1);
    disconnect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::closeI2cBus);
    connect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::openI2cBus);
    ui->I2cBusOpenPushButton->setText("Open");
    close_i2c_dev(i2cBusHook);
}

void MainWindow::refreshI2cBusList()
{
    adapters = gather_i2c_busses();
    ui->I2cBusListComboBox->clear();
    qDebug() << "List of I2C adapters:";
    for(int cnt = 0; adapters[cnt].name; cnt++)
    {
        ui->I2cBusListComboBox->addItem(adapters[cnt].name);
        qDebug() << "\t" << adapters[cnt].nr << "\t" << adapters[cnt].name;
    }
    if(ui->I2cBusListComboBox->count())
    {
        disconnect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::refreshI2cBusList);
        connect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::openI2cBus);
        ui->I2cBusOpenPushButton->setText("Open");
    }
}
