#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <linux/i2c-dev.h>
#include <QDebug>

extern "C"
{
#include "i2cbusses.h"
#include <unistd.h>
#include <linux/i2c.h>
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::refreshI2cBusList);
    connect(ui->I2cBusRead, &QPushButton::clicked, this, &MainWindow::readRegs);
    connect(ui->I2cBusWrite, &QPushButton::clicked, this, &MainWindow::writeReg);
    refreshI2cBusList();

    for(int i = 0; i < ui->I2cRegX->maximum(); i++)
    {
        Regs.append(new QLineEdit);
        Regs.last()->setAlignment(Qt::AlignHCenter);
        ui->RegsGrid->addWidget(Regs.last(), i/5, i%5);
        connect(Regs.last(), &QLineEdit::cursorPositionChanged, this, &MainWindow::RestoreColor);
        //Regs.last()->setObjectName(QString("Reg") + QString::number(i, 16) + QString("h"));
        Regs.last()->setReadOnly(1);
    }

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
    if(i2cBusHook >= 0)
    {
        delete MP2722Dev;
        close_i2c_dev(i2cBusHook); i2cBusHook = -1;
    }

    delete ui;
}

void MainWindow::openI2cBus()
{
    int i2cBus = lookup_i2c_bus(ui->I2cBusListComboBox->currentText().toUtf8().data());

    char i2cPath[20];
    i2cBusHook = open_i2c_dev(i2cBus, i2cPath, 0);
    if(i2cBusHook < 0)
        return;

    ui->I2cBusListComboBox->setDisabled(1);
    disconnect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::openI2cBus);
    connect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::closeI2cBus);
    ui->I2cBusOpenPushButton->setText("Close");

    MP2722Dev = new MP2722(i2cBusHook);

    ui->I2cBusRead->setEnabled(1);
    ui->I2cBusWrite->setEnabled(1);
}

void MainWindow::closeI2cBus()
{
    ui->I2cBusListComboBox->setEnabled(1);
    disconnect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::closeI2cBus);
    connect(ui->I2cBusOpenPushButton, &QPushButton::clicked, this, &MainWindow::openI2cBus);
    ui->I2cBusOpenPushButton->setText("Open");
    close_i2c_dev(i2cBusHook); i2cBusHook = -1;

    delete MP2722Dev;

    ui->I2cBusRead->setDisabled(1);
    ui->I2cBusWrite->setDisabled(1);
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

void MainWindow::readRegs()
{
    for(int i = 0; i < ui->I2cRegX->maximum(); i++)
    {
        QString newVal = QString::number(MP2722Dev->getRegValue(i), 16);
        QString oldVal = Regs[i]->text();
        if(oldVal != "" && oldVal != newVal)
        {
            Regs[i]->setStyleSheet("background:#ff0000");
        }
        Regs[i]->setText(newVal);
    }
}

void MainWindow::writeReg()
{
    int nrReg = ui->I2cRegX->value();
    int RegVal = ui->I2cRegVal->value();
    MP2722Dev->setRegValue(nrReg, RegVal);
}

void MainWindow::RestoreColor(int, int)
{
    qobject_cast<QLineEdit*>(sender())->setStyleSheet("background:#FFFFFF");
}
