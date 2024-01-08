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
    i2cBusHook = -1;
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
        connect(Regs.last(), &QLineEdit::cursorPositionChanged, this, &MainWindow::ShowDesc);
        Regs.last()->setReadOnly(1);
        Regs.last()->setObjectName(QString("Reg") + QString::number(i) + QString("h"));
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

void MainWindow::ShowDesc()
{
    int regVal = qobject_cast<QLineEdit*>(sender())->text().toInt(nullptr, 16);
    int regNr = qobject_cast<QLineEdit*>(sender())->objectName().remove("Reg").remove("h").toInt();
    ui->RegDescPresenter->setText(RegDecs(regNr,regVal));
    qDebug() << "regNr:" << regNr << "regVal:" << regVal;
}

QString MainWindow::RegDecs(int regNr, int regVal)
{
    QString retStr;
    switch (regNr)
    {
        case 0:
        {
            if(regVal & (1<<7))
                retStr = QString("REG_RST: Reset the registers to their default values\n");
            else
                retStr = QString("REG_RST: Keep the current setting\n");

            if(regVal & (1<<6))
                retStr += QString("EN_STAT_IB: The STAT/IB pin is configured as a battery current indicator (IB)\n");
            else
                retStr += QString("EN_STAT_IB: The STAT/IB pin is configured as an open-drain status indicator (STAT)\n");

            if(regVal & (1<<5))
                retStr += QString("EN_PG_NTC2: The PG/NTC2 pin is configured as a second thermistor input (NTC2)\n");
            else
                retStr += QString("EN_PG_NTC2: The PG/NTC2 pin is configured as an open-drain power good indicator (PG)\n");

            if(regVal & (1<<4))
                retStr += QString("LOCK_CHG: The VBATT[5:0], ICC[5:0], IPRE[3:0], JEITA_VSET[1:0], and JEITA_ISET[1:0] values are unlocked\n");
            else
                retStr += QString("LOCK_CHG: The VBATT[5:0], ICC[5:0], IPRE[3:0], JEITA_VSET[1:0], and JEITA_ISET[1:0] values are locked\n");

            if(regVal & (1<<3))
                retStr += QString("HOLDOFF_TMR: Enable the hold-off timer\n");
            else
                retStr += QString("HOLDOFF_TMR: Disable the hold-off timer\n");

            switch(regVal & (3<<1))
            {
                case (0<<1):
                    retStr += QString("SW_FREQ: 750kHz\n");
                    break;
                case (1<<1):
                    retStr += QString("SW_FREQ: 1MHz\n");
                    break;
                case (2<<1):
                    retStr += QString("SW_FREQ: 1.25MHz\n");
                    break;
                case (3<<1):
                    retStr += QString("SW_FREQ: 1.5MHz\n");
                    break;
                default:
                    retStr += QString("SW_FREQ: Error\n");
                    break;
            }

            if(regVal & (1<<0))
                retStr += QString("EN_VIN_TRK: VIN_LIM also tracks VBATT\n");
            else
                retStr += QString("EN_VIN_TRK: VIN_LIM is fixed\n");
            break;
        }
        case 1:
        {
            switch(regVal & (7<<5))
            {
                case (0<<5):
                {
                    retStr = QString("IIN_MODE: Follow the IIN_LIM setting\n");
                    break;
                }
                case (1<<5):
                {
                    retStr = QString("IIN_MODE: Force IIN_LIM to 100mA\n");
                    break;
                }
                case (2<<5):
                {
                    retStr = QString("IIN_MODE: Force IIN_LIM to 500mA\n");
                    break;
                }
                case (3<<5):
                {
                    retStr = QString("IIN_MODE: Force IIN_LIM to 900mA\n");
                    break;
                }
                case (4<<5):
                {
                    retStr = QString("IIN_MODE: Force IIN_LIM to 1500mA\n");
                    break;
                }
                case (5<<5):
                {
                    retStr = QString("IIN_MODE: Force IIN_LIM to 2000mA\n");
                    break;
                }
                case (6<<5):
                {
                    retStr = QString("IIN_MODE: Force IIN_LIM to 3000mA\n");
                    break;
                }
                default:
                {
                    retStr = QString("IIN_MODE: Invalid value\n");
                    break;
                }
            }
            int IInLim= 100;
            if(regVal &(1<<4))
                IInLim += 1600;
            if(regVal &(1<<3))
                IInLim += 800;
            if(regVal &(1<<2))
                IInLim += 400;
            if(regVal &(1<<1))
                IInLim += 200;
            if(regVal &(1<<0))
                IInLim += 100;
            retStr += QString("IIN_LIM: ") + QString::number(IInLim) + QString("mA\n");
            break;
        }
        case 2:
        {
            switch(regVal & (3<<6))
            {
                case (0<<6):
                {
                    retStr = QString("VPRE: 2.4V\n");
                    break;
                }
                case (1<<6):
                {
                    retStr = QString("VPRE: 2.6V\n");
                    break;
                }
                case (2<<6):
                {
                    retStr = QString("VPRE: 2.8V\n");
                    break;
                }
                case (3<<6):
                {
                    retStr = QString("VPRE: 3.0V\n");
                    break;
                }
                default:
                {
                    retStr = QString("VPRE: invalid");
                }
            }
            int icc = 0;
            if(regVal & (1 << 5))
                icc += 2560;
            if(regVal & (1 << 4))
                icc += 1280;
            if(regVal & (1 << 3))
                icc += 640;
            if(regVal & (1 << 2))
                icc += 320;
            if(regVal & (1 << 1))
                icc += 160;
            if(regVal & (1 << 0))
                icc += 80;
            retStr += QString("ICC: ") + QString::number(icc) + QString("mA\n");
            break;
        }
        case 3:
        {
            int ipre = 0;
            if(regVal & (1 << 3))
                ipre += 320;
            if(regVal & (1 << 2))
                ipre += 160;
            if(regVal & (1 << 1))
                ipre += 80;
            if(regVal & (1 << 0))
                ipre += 40;
            retStr += QString("IPRE: ") + QString::number(ipre) + QString("mA\n");

            int iterm = 0;
            if(regVal & (1 << 3))
                iterm += 240;
            if(regVal & (1 << 2))
                iterm += 120;
            if(regVal & (1 << 1))
                iterm += 60;
            if(regVal & (1 << 0))
                iterm += 30;
            retStr += QString("ITERM: ") + QString::number(iterm) + QString("mA\n");
            break;
        }
        case 4:
        {
            if(regVal & (1 << 7))
                retStr = QString("VRECHG: 200mV");
            else
                retStr = QString("VRECHG: 100mV");

            int itrickle = 0;
            if(regVal & (1 << 6))
                itrickle += 128;
            if(regVal & (1 << 5))
                itrickle += 64;
            if(regVal & (1 << 4))
                itrickle += 32;
            retStr += QString("ITRICKLE: ") + QString::number(itrickle) + QString("mA\n");

            int vinlim = 3880;
            if(regVal & (1 << 3))
                itrickle += 640;
            if(regVal & (1 << 2))
                itrickle += 320;
            if(regVal & (1 << 1))
                itrickle += 160;
            if(regVal & (1 << 0))
                itrickle += 80;
            retStr += QString("ITRICKLE: ") + QString::number(itrickle) + QString("mV\n");
            break;
        }
        case 5:
        {
            switch(regVal & (3<<6))
            {
                case (0<<6):
                {
                    retStr = QString("TOPOFF_TMR: Disable\n");
                    break;
                }
                case (1<<6):
                {
                    retStr = QString("TOPOFF_TMR: 15min\n");
                    break;
                }
                case (2<<6):
                {
                    retStr = QString("TOPOFF_TMR: 30min\n");
                    break;
                }
                case (3<<6):
                {
                    retStr = QString("TOPOFF_TMR: 45min\n");
                    break;
                }
                default:
                {
                    retStr = QString("TOPOFF_TMR: invalid\n");
                    break;
                }
            }

            int vbatt = 3600;
            if(regVal & (1 << 5))
                vbatt += 800;
            if(regVal & (1 << 4))
                vbatt += 400;
            if(regVal & (1 << 3))
                vbatt += 200;
            if(regVal & (1 << 2))
                vbatt += 100;
            if(regVal & (1 << 1))
                vbatt += 50;
            if(regVal & (1 << 0))
                vbatt += 25;
            retStr += QString("VBATT: ") + QString::number(vbatt) + QString("mV\n");
            break;
        }
        case 6:
        {
            switch (regVal & (3<<6))
            {
                case (0<<6):
                {
                    retStr = QString("VIN_OVP: 6.3V\n");
                    break;
                }
                case (1<<6):
                {
                    retStr = QString("VIN_OVP: 11V\n");
                    break;
                }
                case (2<<6):
                {
                    retStr = QString("VIN_OVP: 14V\n");
                    break;
                }
                case (3<<6):
                {
                    retStr = QString("VIN_OVP: Disabled\n");
                    break;
                }
                default:
                    retStr = QString("VIN_OVP: Invalid\n");
            }

            switch (regVal & (7<<3))
            {
                case (0<<3):
                {
                    retStr += QString("SYS_MIN: 2.975V\n");
                    break;
                }
                case (1<<3):
                {
                    retStr += QString("SYS_MIN: 3.15V\n");
                    break;
                }
                case (2<<3):
                {
                    retStr += QString("SYS_MIN: 3.325V\n");
                    break;
                }
                case (3<<3):
                {
                    retStr += QString("SYS_MIN: 3.5V\n");
                    break;
                }
                case (4<<3):
                {
                    retStr += QString("SYS_MIN: 3.588V\n");
                    break;
                }
                case (5<<3):
                {
                    retStr += QString("SYS_MIN: 3.675V\n");
                    break;
                }
                case (6<<3):
                {
                    retStr += QString("SYS_MIN: 3.763V\n");
                    break;
                }
                default:
                {
                    retStr += QString("SYS_MIN: invalid\n");
                }
            }

            switch(regVal &(7<<0))
            {
                case (0<<0):
                {
                    retStr += QString("TREG: 60*C");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("TREG: 70*C");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("TREG: 80*C");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("TREG: 90*C");
                    break;
                }
                case (4<<0):
                {
                    retStr += QString("TREG: 100*C");
                    break;
                }
                case (5<<0):
                {
                    retStr += QString("TREG: 110*C");
                    break;
                }
                case (6<<0):
                {
                    retStr += QString("TREG: 120*C");
                    break;
                }
                default:
                {
                    retStr += QString("TREG: invalid");
                    break;
                }
            }

            break;
        }
        case 7:
        {
            if(regVal & (1<<7))
                retStr = QString("IB_EN: IB outputs all the time\n");
            else
                retStr = QString("IB_EN: IB outputs when the switcher is on\n");

            if(regVal & (1<<6))
                retStr += QString("WATCHDOG_RST: Reset the watchdog timer\n");

            switch (regVal & (3<<4))
            {
                case (0<<4):
                {
                    retStr += QString("WATCHDOG: Disabled\n");
                    break;
                }
                case (1<<4):
                {
                    retStr += QString("WATCHDOG: 40s\n");
                    break;
                }
                case (2<<4):
                {
                    retStr += QString("WATCHDOG: 80s\n");
                    break;
                }
                case (3<<4):
                {
                    retStr += QString("WATCHDOG: 160s\n");
                    break;
                }
                default:
                {
                    retStr += QString("WATCHDOG: invalid\n");
                    break;
                }
            }

            if(regVal & (1<<3))
                retStr = QString("EN_TERM: Enable termination\n");
            else
                retStr = QString("EN_TERM: Disable termination\n");

            if(regVal & (1<<2))
                retStr = QString("EN_TMR2X: Enable 2x timer");
            else
                retStr = QString("EN_TMR2X: Disable 2x timer");

            switch (regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("CHG_TIMER: Disabled\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("CHG_TIMER: 5h\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("CHG_TIMER: 10h\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("CHG_TIMER: 15h\n");
                    break;
                }
                default:
                {
                    retStr += QString("CHG_TIMER: invalid\n");
                    break;
                }
            }

            break;
        }
        case 8:
        {
            if(regVal & (1<<7))
                retStr = QString("BATTFET_DIS: Turn off BATTFET\n");
            else
                retStr = QString("BATTFET_DIS: Allow BATTFET to remain on\n");

            if(regVal & (1<<6))
                retStr += QString("BATTFET_DLY: Turn off BATTFET after a 10s delay\n");
            else
                retStr += QString("BATTFET_DLY: Turn off BATTFET immediately\n");

            if(regVal & (1<<5))
                retStr += QString("BATTFET_RST_EN: Enable the BATTFET reset function\n");
            else
                retStr += QString("BATTFET_RST_EN: Disable the BATTFET reset function\n");

            switch (regVal & (3 << 3))
            {
                case (0<<3):
                {
                    retStr += QString("OLIM: 500mA\n");
                    break;
                }
                case (1<<3):
                {
                    retStr += QString("OLIM: 1.5A\n");
                    break;
                }
                case (2<<3):
                {
                    retStr += QString("OLIM: 2.1A\n");
                    break;
                }
                case (3<<3):
                {
                    retStr += QString("OLIM: 3A\n");
                    break;
                }
                default:
                {
                    retStr += QString("OLIM: invalid\n");
                    break;
                }
            }

            switch (regVal & (7 << 0))
            {
                case (0<<0):
                {
                    retStr += QString("VBOOST: 5.2V\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("VBOOST: 5.25V\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("VBOOST: 5.3V\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("VBOOST: 5.35V\n");
                    break;
                }
                case (4<<0):
                {
                    retStr += QString("VBOOST: 5V\n");
                    break;
                }
                case (5<<0):
                {
                    retStr += QString("VBOOST: 5.05V\n");
                    break;
                }
                case (6<<0):
                {
                    retStr += QString("VBOOST: 5.1\n");
                    break;
                }
                case (7<<0):
                {
                    retStr += QString("VBOOST: 5.15v\n");
                    break;
                }
                default:
                {
                    retStr += QString("VBOOST: invalid\n");
                    break;
                }
            }

            break;
        }
        case 9:
        {
            switch (regVal & (7<<4))
            {
                case (0<<4):
                {
                    retStr = QString("CC_CFG: Sink only\n");
                    break;
                }
                case (1<<4):
                {
                    retStr = QString("CC_CFG: Source only\n");
                    break;
                }
                case (2<<4):
                {
                    retStr = QString("CC_CFG: DRP\n");
                    break;
                }
                case (3<<4):
                {
                    retStr = QString("CC_CFG: DRP with Try.SNK\n");
                    break;
                }
                case (4<<4):
                {
                    retStr = QString("CC_CFG: DRP with Try.SRC\n");
                    break;
                }
                case (5<<4):
                {
                    retStr = QString("CC_CFG: Disabled\n");
                    break;
                }
                default:
                {
                    retStr = QString("CC_CFG: Invalid\n");
                    break;
                }
            }

            if(regVal & (1<<3))
                retStr += QString("AUTOOTG: OTG is automatically controlled by CC detection\n");
            else
                retStr += QString("AUTOOTG: On-the-go (OTG) mode is controlled by the host\n");

            if(regVal & (1<<2))
                retStr += QString("EN_BOOST: Boost enabled\n");
            else
                retStr += QString("EN_BOOST: Boost disabled\n");

            if(regVal & (1<<1))
                retStr += QString("EN_BUCK: Buck allowed\n");
            else
                retStr += QString("EN_BUCK: Buck disabled\n");

            if(regVal & (1<<0))
                retStr += QString("EN_CHG: Charging allowed\n");
            else
                retStr += QString("EN_CHG: Charging disable\n");

            break;
        }
        case 10:
        {
            if(regVal & (1<<5))
                retStr = QString("AUTODPDM: D+/D- detection automatically starts after VIN_GD = 1 and the hold-off timer ends\n");
            else
                retStr = QString("AUTODPDM: D+/D- detection starts manually\n");

            if(regVal & (1<<4))
                retStr += QString("EN_CHG: Force D+/D- detection\n");
            else
                retStr += QString("EN_CHG: Normal\n");

            switch (regVal & (3<<2))
            {
                case (0<<2):
                {
                    retStr += QString("RP_CFG: 80μA default USB\n");
                    break;
                }
                case (1<<2):
                {
                    retStr += QString("RP_CFG: 180μA, USB Type-C 1.5A\n");
                    break;
                }
                case (2<<2):
                {
                    retStr += QString("RP_CFG: 330μA, USB Type-C 3A\n");
                    break;
                }
                default:
                {
                    retStr += QString("RP_CFG: Invalid\n");
                    break;
                }
            }

            switch (regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("FORCE_CC: The CC1 and CC2 pins are automatically configured via CC_CFG\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("FORCE_CC: Forces the CC1 and CC2 pins to Rd\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("FORCE_CC: orces the CC1 and CC2 pins to Rp (set by RP_CFG)\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("FORCE_CC: Forces the CC1 and CC2 pins into a high-impedance (Hi-Z) state\n");
                    break;
                }
                default:
                {
                    retStr += QString("RP_CFG: Invalid\n");
                    break;
                }
            }

            break;
        }
        case 11:
        {
            if(regVal & (1<<4))
                    retStr = QString("HVEN: Enables high-voltage adapter detection\n");
            else
                    retStr = QString("HVEN: Disables high-voltage adapter detection\n");

            if(regVal & (1<<3))
                    retStr += QString("HVUP: DP = DM = 3.3V for 500μs\n");
            else
                    retStr += QString("HVUP: DP and DM do not change\n");

            if(regVal & (1<<2))
                    retStr += QString("HVDOWN : DP = DM = 0.6V for 500μs\n");
            else
                    retStr += QString("HVDOWN : DP DM unchanged\n");

            switch (regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("HVREQ: DP = 0.6V, DM = Hi-Z\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("HVREQ: DP = 3.3V, DM = 0.6V\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("HVREQ: DP = 0.6V, DM = 0.6V\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("HVREQ: DP = 0.6V, DM = 3.3V\n");
                    break;
                }
                default:
                {
                    retStr += QString("HVREQ: Invalid\n");
                    break;
                }
            }

            break;
        }
        case 12:
        {

            if(regVal & (1<<6))
                retStr = QString("NTC1_ACTION: NTC1 is fully functional\n");
            else
                retStr = QString("NTC1_ACTION: Only generate INT when the NTC1 status changes\n");

            if(regVal & (1<<5))
                retStr += QString("NTC2_ACTION: NTC2 is fully functional\n");
            else
                retStr += QString("NTC2_ACTION: Only generate INT when the NTC2 status changes\n");

            if(regVal & (1<<4))
                retStr += QString("BATT_OVP_EN: Battery OVP is enabled \n");
            else
                retStr += QString("BATT_OVP_EN: Battery OVP is neglected\n");

            switch(regVal & (3<<2))
            {
                case (0<<2):
                {
                    retStr += QString("BATT_LOW: 3V falling\n");
                    break;
                }
                case (1<<2):
                {
                    retStr += QString("BATT_LOW: 3.1V falling\n");
                    break;
                }
                case (2<<2):
                {
                    retStr += QString("BATT_LOW: 3.2V falling\n");
                    break;
                }
                case (3<<2):
                {
                    retStr += QString("BATT_LOW: 3.3V falling\n");
                    break;
                }
                default:
                {
                    retStr += QString("BATT_LOW: Invalid\n");
                    break;
                }
            }

            if(regVal & (1<<1))
                retStr += QString("BOOST_STP_EN: The BATT_LOW comparator turns off boost operation and latches\n");
            else
                retStr += QString("BOOST_STP_EN: The BATT_LOW comparator only generates INT\n");

            if(regVal & (1<<0))
                retStr += QString("BOOST_OTP_EN: Boost OTP is ignored\n");
            else
                retStr += QString("BOOST_OTP_EN: Boost OTP occurs at TREG\n");

            break;
        }
        case 13:
        {
            switch(regVal & (3<<6))
            {
                case (0<<6):
                {
                    retStr = QString("WARM_ACT: No action during an NTC warm condition\n");
                    break;
                }
                case (1<<6):
                {
                    retStr = QString("WARM_ACT: Reduce VBATT_REG during an NTC warm condition\n");
                    break;
                }
                case (2<<6):
                {
                    retStr = QString("WARM_ACT: Reduce ICC during an NTC warm condition\n");
                    break;
                }
                case (3<<6):
                {
                    retStr = QString("WARM_ACT: Reduce both VBATT_REG and ICC during an NTC warm condition\n");
                    break;
                }
                default:
                {
                    retStr = QString("WARM_ACT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<4))
            {
                case (0<<4):
                {
                    retStr += QString("COOL_ACT: No action during an NTC cool condition\n");
                    break;
                }
                case (1<<4):
                {
                    retStr += QString("COOL_ACT: Reduce VBATT_REG during an NTC cool condition\n");
                    break;
                }
                case (2<<4):
                {
                    retStr += QString("COOL_ACT: Reduce ICC during an NTC cool condition\n");
                    break;
                }
                case (3<<4):
                {
                    retStr += QString("COOL_ACT: Reduce both VBATT_REG and ICC during an NTC cool condition\n");
                    break;
                }
                default:
                {
                    retStr += QString("COOL_ACT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<2))
            {
                case (0<<2):
                {
                    retStr += QString("JEITA_VSET: VBATT_REG - 100mV\n");
                    break;
                }
                case (1<<2):
                {
                    retStr += QString("JEITA_VSET: VBATT_REG - 150mV\n");
                    break;
                }
                case (2<<2):
                {
                    retStr += QString("JEITA_VSET: VBATT_REG - 200mV\n");
                    break;
                }
                case (3<<2):
                {
                    retStr += QString("JEITA_VSET: VBATT_REG - 250mV\n");
                    break;
                }
                default:
                {
                    retStr += QString("JEITA_VSET: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("JEITA_ISET: 50% of ICC\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("JEITA_ISET: 33% of ICC\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("JEITA_ISET: 20% of ICC\n");
                    break;
                }
                default:
                {
                    retStr += QString("JEITA_ISET: Invalid\n");
                    break;
                }
            }

            break;
        }
        case 14:
        {
            switch(regVal & (3<<6))
            {
                case (0<<6):
                {
                    retStr = QString("VHOT: 29.1% (50°C)\n");
                    break;
                }
                case (1<<6):
                {
                    retStr = QString("VHOT: 25.9% (55°C)\n");
                    break;
                }
                case (2<<6):
                {
                    retStr = QString("VHOT: 23% (60°C)\n");
                    break;
                }
                case (3<<6):
                {
                    retStr = QString("VHOT: 20.4% (65°C)\n");
                    break;
                }
                default:
                {
                    retStr = QString("VHOT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<4))
            {
                case (0<<4):
                {
                    retStr += QString("VWARM: 36.5% (40°C)\n");
                    break;
                }
                case (1<<4):
                {
                    retStr += QString("VWARM: 32.6% (45°C)\n");
                    break;
                }
                case (2<<4):
                {
                    retStr += QString("VWARM: 29.1% (50°C)\n");
                    break;
                }
                case (3<<4):
                {
                    retStr += QString("VWARM: 25.9% (55°C)\n");
                    break;
                }
                default:
                {
                    retStr += QString("VWARM: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<2))
            {
                case (0<<2):
                {
                    retStr += QString("VCOOL: 74.2% (0°C)\n");
                    break;
                }
                case (1<<2):
                {
                    retStr += QString("VCOOL: 69.6% (5°C)\n");
                    break;
                }
                case (2<<2):
                {
                    retStr += QString("VCOOL: 64.8% (10°C)\n");
                    break;
                }
                case (3<<2):
                {
                    retStr += QString("VCOOL: 59.9% (15°C)\n");
                    break;
                }
                default:
                {
                    retStr += QString("VCOOL: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("VCOLD: 78.4% (-5°C)\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("VCOLD: 74.2% (0°C)\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("VCOLD: 69.6% (+5°C)\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("VCOLD: 64.8% (+10°C)\n");
                    break;
                }
                default:
                {
                    retStr += QString("VCOLD: Invalid\n");
                    break;
                }
            }

            break;
        }
        case 15:
        {
            if(regVal & (1<<6))
                retStr = QString("VIN_SRC_EN: Source current to the IN pin\n");
            else
                 retStr = QString("VIN_SRC_EN: Normal\n");

            switch(regVal & (15<<2))
            {
                case (0<<2):
                {
                    retStr += QString("IVIN_SRC: 5μA\n");
                    break;
                }
                case (1<<2):
                {
                    retStr += QString("IVIN_SRC: 10μA\n");
                    break;
                }
                case (2<<2):
                {
                    retStr += QString("IVIN_SRC: 20μA\n");
                    break;
                }
                case (3<<2):
                {
                    retStr += QString("IVIN_SRC: 40μA\n");
                    break;
                }
                case (4<<2):
                {
                    retStr += QString("IVIN_SRC: 80μA\n");
                    break;
                }
                case (5<<2):
                {
                    retStr += QString("IVIN_SRC: 160μA\n");
                    break;
                }
                case (6<<2):
                {
                    retStr += QString("IVIN_SRC: 320μA\n");
                    break;
                }
                case (7<<2):
                {
                    retStr += QString("IVIN_SRC: 640μA\n");
                    break;
                }
                case (8<<2):
                {
                    retStr += QString("IVIN_SRC: 1280μA\n");
                    break;
                }
                default:
                {
                    retStr += QString("IVIN_SRC: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("VIN_TEST: 0.3V\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("VIN_TEST: 0.5V\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("VIN_TEST: 1V\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("VIN_TEST: 1.5V\n");
                    break;
                }
                default:
                {
                    retStr += QString("VIN_TEST: Invalid\n");
                    break;
                }
            }

            break;
        }
        case 16:
        {
            if(regVal & (1<<5))
                retStr = QString("MASK_THERM : Mask the THERM_STAT INT pulse\n");
            else
                retStr = QString("MASK_THERM : Enable the THERM_STAT INT pulse\n");

            if(regVal & (1<<4))
                retStr += QString("MASK_DPM: Mask the VINDPM and IINDPM INT pulses\n");
            else
                retStr += QString("MASK_DPM: Enable the VINDPM and IINDPM INT pulses\n");

            if(regVal & (1<<3))
                retStr += QString("MASK_TOPOFF: Mask the TOPOFF timer INT pulse\n");
            else
                retStr += QString("MASK_TOPOFF: Enable the TOPOFF timer INT pulse\n");

            if(regVal & (1<<2))
                retStr += QString("MASK_CC_INT: Mask the CC_SNK and CC_SRC INT pulse\n");
            else
                retStr += QString("MASK_CC_INT: Enable the CC_SNK and CC_SRC INT pulses\n");

            if(regVal & (1<<1))
                retStr += QString("MASK_BATT_LOW: Mask the BATT_LOW INT pulse\n");
            else
                retStr += QString("MASK_BATT_LOW: Enable the BATT_LOW INT pulse\n");

            if(regVal & (1<<0))
                retStr += QString("MASK_DEBUG_AUDIO: Mask DEBUGACC and AUDIOACC INT pulse\n");
            else
                retStr += QString("MASK_DEBUG_AUDIO: Allow DEBUGACC and AUDIOACC INT pulse\n");

            break;
        }
        case 17:
        {
            switch(regVal & (15<<4))
            {
                case (0<<4):
                {
                    retStr += QString("DPDM_STAT: Not started (500mA)\n");
                    break;
                }
                case (1<<4):
                {
                    retStr += QString("DPDM_STAT: USB SDP (500mA)\n");
                    break;
                }
                case (2<<4):
                {
                    retStr += QString("DPDM_STAT: USB DCP (2A)\n");
                    break;
                }
                case (3<<4):
                {
                    retStr += QString("DPDM_STAT: USB CDP (1.5A)\n");
                    break;
                }
                case (4<<4):
                {
                    retStr += QString("DPDM_STAT: Divider 1 (1A)\n");
                    break;
                }
                case (5<<4):
                {
                    retStr += QString("DPDM_STAT: Divider 2 (2.1A)\n");
                    break;
                }
                case (6<<4):
                {
                    retStr += QString("DPDM_STAT: Divider 3 (2.4A)\n");
                    break;
                }
                case (7<<4):
                {
                    retStr += QString("DPDM_STAT: Divider 4 (2A)\n");
                    break;
                }
                case (8<<4):
                {
                    retStr += QString("DPDM_STAT: Unknown (500mA)\n");
                    break;
                }
                case (9<<4):
                {
                    retStr += QString("DPDM_STAT: High-voltage adapter (2A)\n");
                    break;
                }
                case (14<<4):
                {
                    retStr += QString("DPDM_STAT: Divider 5 (3A)\n");
                    break;
                }
                default:
                {
                    retStr += QString("IVIN_SRC: Invalid\n");
                    break;
                }
            }

            if(regVal & (1<<1))
                retStr += QString("VINDPM_STAT: In VINDPM\n");
            else
                retStr += QString("VINDPM_STAT: Not in VINDPM\n");

            if(regVal & (1<<0))
                retStr += QString("IINDPM_STAT: In IINDPM\n");
            else
                retStr += QString("IINDPM_STAT: Not in IINDPM\n");

            break;
        }
        case 18:
        {
            if(regVal & (1<<6))
                retStr = QString("VIN_GD : The input source is good\n");
            else
                retStr = QString("VIN_GD : The input source is not valid\n");

            if(regVal & (1<<5))
                retStr += QString("VIN_RDY: VIN is ready to charge\n");
            else
                retStr += QString("VIN_RDY: VIN is not ready to charge\n");

            if(regVal & (1<<4))
                retStr += QString("LEGACYCABLE: Legacy cable is detected (not valid in DRP mode)\n");
            else
                retStr += QString("LEGACYCABLE: Normal\n");

            if(regVal & (1<<3))
                retStr += QString("THERM_STAT: In thermal regulation\n");
            else
                retStr += QString("THERM_STAT: Not in thermal regulation\n");

            if(regVal & (1<<2))
                retStr += QString("VSYS_STA: VBATT < VSYS_MIN\n");
            else
                retStr += QString("VSYS_STA: VBATT > VSYS_MIN\n");

            if(regVal & (1<<1))
                retStr += QString("WATCHDOG_FAULT: The watchdog timer has expired\n");
            else
                retStr += QString("WATCHDOG_FAULT: Normal\n");

            if(regVal & (1<<0))
                retStr += QString("WATCHDOG_BARK: The 3/4 watchdog timer has expired\n");
            else
                retStr += QString("WATCHDOG_BARK: Normal\n");

            break;
        }
        case 19:
        {
            switch(regVal & (7<<5))
            {
                case (0<<5):
                {
                    retStr = QString("CHG_STAT: Not charging\n");
                    break;
                }
                case (1<<5):
                {
                    retStr = QString("CHG_STAT: Trickle charge\n");
                    break;
                }
                case (2<<5):
                {
                    retStr = QString("CHG_STAT: Pre-charge\n");
                    break;
                }
                case (3<<5):
                {
                    retStr = QString("CHG_STAT: Fast charge\n");
                    break;
                }
                case (4<<5):
                {
                    retStr = QString("CHG_STAT: Constant-voltage charge\n");
                    break;
                }
                case (5<<5):
                {
                    retStr = QString("CHG_STAT: Charging is done\n");
                    break;
                }
                default:
                {
                    retStr = QString("CHG_STAT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (7<<2))
            {
                case (0<<2):
                {
                    retStr += QString("BOOST_FAULT: Normal\n");
                    break;
                }
                case (1<<2):
                {
                    retStr += QString("BOOST_FAULT: An IN overload or short (latch-off) has occurred\n");
                    break;
                }
                case (2<<2):
                {
                    retStr += QString("BOOST_FAULT: An IN overload or short (latch-off) has occurred\n");
                    break;
                }
                case (3<<2):
                {
                    retStr += QString("BOOST_FAULT: Boost over-temperature protection (latch-off) has occurred\n");
                    break;
                }
                case (4<<2):
                {
                    retStr += QString("BOOST_FAULT: The boost has stopped due to BATT_LOW (latch-off)\n");
                    break;
                }
                default:
                {
                    retStr += QString("BOOST_FAULT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("CHG_FAULT: Normal\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("CHG_FAULT: Input OVP\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("CHG_FAULT: The charge timer has expired\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("CHG_FAULT: Battery OVP\n");
                    break;
                }
                default:
                {
                    retStr += QString("CHG_FAULT: Invalid\n");
                    break;
                }
            }

            break;
        }
        case 20:
        {
            if(regVal & (1<<7))
                retStr = QString("NTC_MISSING: NTC is missing (VNTC > 95% of VVRNTC)\n");
            else
                retStr = QString("NTC_MISSING: Normald\n");

            if(regVal & (1<<6))
                retStr += QString("BATT_MISSING: The battery is missing (2 terminations detected within 3 seconds)\n");
            else
                retStr += QString("BATT_MISSING: Normal\n");

            switch(regVal & (7<<3))
            {
                case (0<<3):
                {
                    retStr += QString("NTC1_FAULT: Normal\n");
                    break;
                }
                case (1<<3):
                {
                    retStr += QString("NTC1_FAULT: Warm\n");
                    break;
                }
                case (2<<3):
                {
                    retStr += QString("NTC1_FAULT: Cool\n");
                    break;
                }
                case (3<<3):
                {
                    retStr += QString("NTC1_FAULT: Cold\n");
                    break;
                }
                case (4<<3):
                {
                    retStr += QString("NTC1_FAULT: Hot\n");
                    break;
                }
                default:
                {
                    retStr += QString("NTC1_FAULT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (7<<0))
            {
                case (0<<0):
                {
                    retStr += QString("NTC2_FAULT: Normal\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("NTC2_FAULT: Warm\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("NTC2_FAULT: Cool\n");
                    break;
                }
                case (3<<0):
                {
                    retStr += QString("NTC2_FAULT: Cold\n");
                    break;
                }
                case (4<<0):
                {
                    retStr += QString("NTC2_FAULT: Hot\n");
                    break;
                }
                default:
                {
                    retStr += QString("NTC2_FAULT: Invalid\n");
                    break;
                }
            }
            break;
        }
        case 21:
        {
            switch(regVal & (3<<6))
            {
                case (0<<6):
                {
                    retStr = QString("CC1_SNK_STAT: CC1 detects vRa\n");
                    break;
                }
                case (1<<6):
                {
                    retStr = QString("CC1_SNK_STAT: CC1 detects vRd-USB\n");
                    break;
                }
                case (2<<6):
                {
                    retStr = QString("CC1_SNK_STAT: CC1 detects vRd-1.5\n");
                    break;
                }
                case (3<<6):
                {
                    retStr = QString("CC1_SNK_STAT: CC1 detects vRd-3.0\n");
                    break;
                }
                default:
                {
                    retStr = QString("CC1_SNK_STAT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<4))
            {
                case (0<<4):
                {
                    retStr += QString("CC2_SNK_STAT: CC2 detects vRa\n");
                    break;
                }
                case (1<<4):
                {
                    retStr += QString("CC2_SNK_STAT: CC2 detects vRd-USB\n");
                    break;
                }
                case (2<<4):
                {
                    retStr += QString("CC2_SNK_STAT: CC2 detects vRd-1.5\n");
                    break;
                }
                case (3<<4):
                {
                    retStr += QString("CC2_SNK_STAT: CC2 detects vRd-3.0\n");
                    break;
                }
                default:
                {
                    retStr += QString("CC2_SNK_STAT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<2))
            {
                case (0<<2):
                {
                    retStr += QString("CC1_SRC_STAT: CC1 is vOPEN\n");
                    break;
                }
                case (1<<2):
                {
                    retStr += QString("CC1_SRC_STAT: CC1 detects vRd\n");
                    break;
                }
                case (2<<2):
                {
                    retStr += QString("CC1_SRC_STAT: CC1 detects vRa\n");
                    break;
                }
                default:
                {
                    retStr += QString("CC1_SRC_STAT: Invalid\n");
                    break;
                }
            }

            switch(regVal & (3<<0))
            {
                case (0<<0):
                {
                    retStr += QString("CC2_SRC_STAT: CC2 is vOPEN\n");
                    break;
                }
                case (1<<0):
                {
                    retStr += QString("CC2_SRC_STAT: CC2 detects vRd\n");
                    break;
                }
                case (2<<0):
                {
                    retStr += QString("CC2_SRC_STAT: CC2 detects vRa\n");
                    break;
                }
                default:
                {
                    retStr += QString("CC2_SRC_STAT: Invalid\n");
                    break;
                }
            }

            break;
        }
        case 22:
        {
            if(regVal & (1 << 6))
                retStr = QString("TOPOFF_ACTIVE: The top-off timer is counting");
            else
                retStr = QString("TOPOFF_ACTIVE: The top-off timer is not counting");

            if(regVal & (1 << 5))
                retStr = QString("BFET_STAT: The battery is discharging");
            else
                retStr = QString("BFET_STAT: The battery is charging or disabled");

            if(regVal & (1 << 5))
                retStr = QString("BATT_LOW_STAT: VBATT is below BATT_LOW[1:0]");
            else
                retStr = QString("BATT_LOW_STAT: VBATT is greater than BATT_LOW[1:0]");

            if(regVal & (1 << 5))
                retStr = QString("OTG_NEED: Boost should be enabled");
            else
                retStr = QString("OTG_NEED: Boost should be disabled");

            if(regVal & (1 << 5))
                retStr = QString("VIN_TEST_HIGH: VIN has reached the VIN_TEST threshold");
            else
                retStr = QString("VIN_TEST_HIGH: VIN is below the VIN_TEST threshold");

            if(regVal & (1 << 5))
                retStr = QString("DEBUGACC: Enters DebugAccessory.SNK state");
            else
                retStr = QString("DEBUGACC: Normal");

            if(regVal & (1 << 5))
                retStr = QString("AUDIOACC: Enters AudioAccessory state");
            else
                retStr = QString("AUDIOACC: Normal");

            break;
        }
        default:
        {
            retStr = QString("Out of scope");
            break;
        }
    }
    return retStr;
}
