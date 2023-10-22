#include <QtWidgets>
#include <QLibrary>
#include "HA7NetPlugin.h"
#include "../common.h"

HA7netPlugin::HA7netPlugin(QWidget *parent) : QWidget(parent)
{
    ui = new QWidget();
    QGridLayout *layout = new QGridLayout(ui);
    //QTabWidget *interfaces = new QTabWidget();
    //interfaces->setTabPosition(QTabWidget::West);
    //layout->addWidget(interfaces);
    //QWidget *firstTab = new QWidget();
    QWidget *w = new QWidget();
    layout->addWidget(w);
    mui = new Ui::HA7NetUI;
    mui->setupUi(w);
    mui->logTxt->hide();
    //mui->checkBoxLog->setCheckState(Qt::Checked);
    //interfaces->addTab(firstTab, "HA7Net");
    reply = nullptr;
    converttimer.setSingleShot(true);
    connect(this, SIGNAL(requestdone()), this, SLOT(fifonext()));
    connect(&converttimer, SIGNAL(timeout()), this, SLOT(timerconvertout()));
    TimerPause.setSingleShot(true);
    connect(&TimerPause, SIGNAL(timeout()), this, SLOT(fifonext()));
    connect(mui->Read, SIGNAL(clicked()), this, SLOT(ReadClick()));
    connect(mui->Search, SIGNAL(clicked()), this, SLOT(SearchClick()));
    connect(mui->Conversion, SIGNAL(clicked()), this, SLOT(convertSlot()));
    connect(mui->HA7NetConfig, SIGNAL(clicked()), this, SLOT(Ha7Netconfig()));
    connect(mui->editIP, SIGNAL(textChanged(QString)), this, SLOT(IPEdited(QString)));
    connect(mui->editPort, SIGNAL(textChanged(QString)), this, SLOT(PortEdited(QString)));
    connect(mui->checkBoxLog, SIGNAL(stateChanged(int)), this, SLOT(logEnable(int)));
    connect(mui->Start, SIGNAL(clicked()), this, SLOT(Start()));
    connect(mui->Stop, SIGNAL(clicked()), this, SLOT(Stop()));
    connect(mui->Clear, SIGNAL(clicked()), this, SLOT(Clear()));
    connect(&TimerReqDone, SIGNAL(timeout()), this, SLOT(emitReqDone()));
    connect(mui->devices, SIGNAL(itemDoubleClicked (QListWidgetItem*)), this, SLOT(ReadClick(QListWidgetItem*)));
    connect(mui->devices, SIGNAL(itemSelectionChanged()), this, SLOT(ShowClick()));
    TimerReqDone.setSingleShot(true);
    TimerReqDone.setInterval(100);
}

HA7netPlugin::~HA7netPlugin()
{
    if (!LockID.isEmpty())
    {
        request = ReleaseLock;
        setrequest("ReleaseLock.html?" + LockID);
    }
}




QObject *HA7netPlugin::getObject()
{
    return this;
}


QWidget *HA7netPlugin::widgetUi()
{
    return ui;
}


QWidget *HA7netPlugin::getDevWidget(QString RomID)
{
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID){
            return dev->ui;
        }
    }
    return nullptr;
}


void HA7netPlugin::setConfigFileName(const QString fileName)
{
    configFileName = fileName;
}


QString HA7netPlugin::getName()
{
    return "HA7net";
}


double HA7netPlugin::getMaxValue(const QString devRomID)
{
    QString family = devRomID.right(2);
    QString family4 = devRomID.right(4);
    if (devRomID.contains("_")) family = family4.left(2);
    Dev_Data *device = nullptr;
    foreach (Dev_Data *dev, dev_Data) { if (dev->RomID == devRomID) device = dev; }
    if (device->ScratchPad.isEmpty()) { readDevice(device); return -1; }
    if (family == family1820) return 125;
    else if (family == family18B20) return 125;
    else if (family == family1822) return 125;
    else if (family == family1825) return 125;
    else if (family == family2406) { if (!device) return 100;
        if (family4 == family2406_A) {
            if (device->ui2406->operateDecimal->isChecked()) return 3; }
        if (family4 == family2406_B) return 1; }
    else if (family == family2408) { if (!device) return 100;
            if (device->ui2408->operateDecimal->isChecked()) return 255; }
    else if (family == family2413) { if (!device) return 100;
        if (family4 == family2413_A) {
            if (device->ui2413->operateDecimal->isChecked()) return 3; }
        if (family4 == family2413_B) return 1; }
    else if (family == family3A2100H) { if (!device) return 100;
        if (family4 == family3A2100H_A) {
            if (device->ui2413->operateDecimal->isChecked()) return 3; }
        if (family4 == family3A2100H_B) return 1; }
    else if (family == family2423) return 0xFFFF;
    else if (family == family2438) return 0xFFFF;
    else if (family == family2450) return 0xFFFF;
    else if (family == familyLedOW)  {
        if (!device) return 100;
        if (device->uiLedOW->radioButtonLED->isChecked()) return 100;
        else return device->uiLedOW->TTV->value(); }
    else if (family == familyLCDOW) return true;
    return 1;
}


bool HA7netPlugin::isManual(const QString devRomID)
{
    QString family = devRomID.right(2);
    QString family4 = devRomID.right(4);
    if (devRomID.contains("_")) family = family4.left(2);
    Dev_Data *device = nullptr;
    foreach (Dev_Data *dev, dev_Data) { if (dev->RomID == devRomID) device = dev; }
    if (family == familyLedOW) { if (!device) return false; if (device->manual) { device->manual = false; return true; } return false; }
    return false;
}


bool HA7netPlugin::isDimmable(const QString devRomID)
{
    QString family = devRomID.right(2);
    QString family4 = devRomID.right(4);
    if (devRomID.contains("_")) family = family4.left(2);
    Dev_Data *device = nullptr;
    foreach (Dev_Data *dev, dev_Data) { if (dev->RomID == devRomID) device = dev; }
    if (family == family2408) { if (!device) return false; else return device->ui2408->operateDecimal->isChecked(); }
    else if ((family == family2413) || (family == family3A2100H)) { if (!device) return false; else return device->ui2413->operateDecimal->isChecked(); }
    else if (family == familyLedOW) return true;
    else if (family == familyLCDOW) return true;
    return false;
}


bool HA7netPlugin::acceptCommand(const QString devRomID)
{
    QString family = devRomID.right(2);
    QString family4 = devRomID.right(4);
    if (devRomID.contains("_")) family = family4.left(2);
    if (family == family1820) return false;
    else if (family == family18B20) return false;
    else if (family == family1822) return false;
    else if (family == family1825) return false;
    else if (family == family2406) return true;
    else if (family == family2408) return true;
    else if ((family == family2413) || (family == family3A2100H)) return true;
    else if (family == family2423) return false;
    else if (family == family2438) return false;
    else if (family == family2450) return false;
    else if (family == familyLedOW) return true;
    else if (family == familyLCDOW) return true;
    return false;
}


void HA7netPlugin::setStatus(const QString status)
{
    bool ok;
    if ((status == lastStatus) && waitAnswer.isActive()) return;
    lastStatus = status;
    waitAnswer.start(5000);
    QStringList split = status.split("=");
    QString RomID = split.first();
    QString command = split.last();
    if (command == "read") readDevice(RomID);
    else if (command == "on") on(RomID);
    else if (command == "on100") on100(RomID);
    else if (command == "off") off(RomID);
    else if (command == "dim") dim(RomID);
    else if (command == "brigth") bright(RomID);
    else if (command == "open") open(RomID);
    else if (command == "close") close(RomID);
    else {
        double v = command.toDouble(&ok);
        if (ok) setValue(RomID, v);
    }
}


void HA7netPlugin::emitReqDone()
{
    if (TimerReqDone.isActive()) return;
    emit requestdone();
}

void HA7netPlugin::Ha7Netconfig()
{
      /*messageBox::aboutHide(this, tr("Ha7net Configuration"),
                           tr("Admin login :  user 'admin' password 'eds'\n"
                           "Default IP adress 192.168.0.250\n"
                           "Change Lock idle time out int the Miscellaneous tab to 5 seconds\n"
                           "\n"
                           "\n\n"));*/
}


void HA7netPlugin::saveConfig()
{
    QFile file("HA7Net.cfg");
    QByteArray configdata;
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out.setGenerateByteOrderMark(true);
        QString str;
        QDateTime now = QDateTime::currentDateTime();
        str += "Configuration file " + now.toString() + "\n";
        str += saveformat("IPadress", ipaddress);
        str += saveformat("Port", QString("%1").arg(port));
        if (mui->LockEnable->isChecked()) str += saveformat("LockID", "1"); else str += saveformat("LockID", "0");
        if (mui->Global_Convert->isChecked()) str += saveformat("GlobalConvert", "1"); else str += saveformat("GlobalConvert", "0");
        str += saveformat("ConvertDelay", QString("%1").arg(mui->ConvertDelay->value()));
        out << str;
        file.close();
    }
}


void HA7netPlugin::readConfig()
{
    QFile file("HA7Net.cfg");
    QByteArray configdata;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        QString configData;
        configData.append(in.readAll());
        file.close();
        setipaddress(getvalue("IPadress", configData));
        setport(getvalue("Port", configData));
    }
    addtofifo(Search);
}


void HA7netPlugin::setLockedState(bool)
{

}


QString HA7netPlugin::getDeviceConfig(QString RomID)
{
    QString str;
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            if (dev->ui1820) {
                str += saveformat("Skip85", QString("%1").arg(dev->ui1820->skip85->isChecked())); }
            if (dev->ui2408) {
                str += saveformat("Input", QString("%1").arg(dev->ui2408->radioButtonInput->isChecked()));
                str += saveformat("UseDecimal", QString("%1").arg(dev->ui2408->operateDecimal->isChecked()));
                str += saveformat("InvertCO", QString("%1").arg(dev->ui2408->InvertOut->isChecked())); }
            if (dev->ui2413) {
                str += saveformat("Input", QString("%1").arg(dev->ui2413->radioButtonInput->isChecked()));
                str += saveformat("UseDecimal", QString("%1").arg(dev->ui2413->operateDecimal->isChecked()));
                str += saveformat("InvertCO", QString("%1").arg(dev->ui2413->InvertOut->isChecked())); }
            if (dev->ui2423) {
                str += saveformat("counterMode", QString("%1").arg(dev->ui2423->counterMode->currentIndex()));
                str += saveformat("counterOffset", QString("%1").arg(dev->ui2423->Offset->value()));
                str += saveformat("NextResetOffset", dev->ui2423->nextOffsetAdjust->dateTime().toString(Qt::ISODate));
                str += saveformat("CountInitMode", QString("%1").arg(dev->ui2423->Interval->currentIndex()));
                str += saveformat("CountInitEnabled", QString("%1").arg(dev->ui2423->countInit->isChecked()));
                str += saveformat("SaveOnUpdate", QString("%1").arg(dev->ui2423->SaveOnUpdate->isChecked()));
            }
            if (dev->uiLedOW) {
                str += saveformat("LED_Mode", QString("%1").arg(dev->uiLedOW->radioButtonLED->isChecked()));
            }
            if (dev->uiLCDOW) {
                str += saveformat("LCD_Coef", dev->uiLCDOW->Coef->text(), true);
            } } }
    return str;
}


void HA7netPlugin::setDeviceConfig(const QString &RomID, const QString &strsearch)
{
    bool ok;
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            if (dev->ui1820) {
                int sk85 = getvalue("Skip85", strsearch).toInt(&ok);
                if (!ok) { dev->ui1820->skip85->setCheckState(Qt::Checked); dev->skip85 = true; }
                else if (ok && sk85) { dev->ui1820->skip85->setCheckState(Qt::Checked); dev->skip85 = true; }
                else { dev->ui1820->skip85->setCheckState(Qt::Unchecked); dev->skip85 = false; } }
            if (dev->ui2408) {
                int input = getvalue("Input", strsearch).toInt(&ok);
                if (ok) {
                    if (input) { dev->ui2408->radioButtonInput->setChecked(Qt::Checked); dev->isOutput = false; }
                    else { dev->ui2408->radioButtonOutput->setChecked(Qt::Checked); dev->isOutput = true; } }
                int decimal = getvalue("UseDecimal", strsearch).toInt(&ok);
                if (ok) {
                    if (decimal) dev->ui2408->operateDecimal->setChecked(Qt::Checked);
                    else dev->ui2408->operateDecimal->setChecked(Qt::Unchecked); }
                int invert = getvalue("InvertCO", strsearch).toInt(&ok);
                if (ok) {
                    if (invert) dev->ui2408->InvertOut->setChecked(Qt::Checked);
                    else dev->ui2408->InvertOut->setChecked(Qt::Unchecked); } }
            if (dev->ui2413) {
                int input = getvalue("Input", strsearch).toInt(&ok);
                if (ok) {
                    if (input) { dev->ui2413->radioButtonInput->setChecked(Qt::Checked); dev->isOutput = false; }
                    else { dev->ui2413->radioButtonOutput->setChecked(Qt::Checked); dev->isOutput = true; } }
                int decimal = getvalue("UseDecimal", strsearch).toInt(&ok);
                if (ok) {
                    if (decimal) dev->ui2413->operateDecimal->setChecked(Qt::Checked);
                    else dev->ui2413->operateDecimal->setChecked(Qt::Unchecked); }
                int invert = getvalue("InvertCO", strsearch).toInt(&ok);
                if (ok) {
                    if (invert) dev->ui2413->InvertOut->setChecked(Qt::Checked);
                    else dev->ui2413->InvertOut->setChecked(Qt::Unchecked); } }
                if (dev->ui2423) {
                    bool ok = false;
                    dev->ui2423->Offset->setEnabled(false);
                    dev->ui2423->countInit->setEnabled(false);
                    QString Name = getvalue("Name", strsearch);
                    int mode = getvalue("counterMode", strsearch).toInt(&ok);
                    if (ok) {
                        dev->ui2423->counterMode->setCurrentIndex(mode);
                        if (mode == offsetMode) dev->ui2423->countInit->setEnabled(true); }
                            else dev->ui2423->counterMode->setCurrentIndex(absoluteMode);
                    int offset = getvalue("counterOffset", strsearch).toInt(&ok);
                    if (ok) dev->ui2423->Offset->setValue(offset);
                    QString next = getvalue("NextResetOffset", strsearch);
                    if (next.isEmpty()) dev->ui2423->nextOffsetAdjust->setDateTime(setDateTimeS0(QDateTime::currentDateTime()));
                            else dev->ui2423->nextOffsetAdjust->setDateTime(setDateTimeS0(QDateTime::fromString (next, Qt::ISODate)));
                    int savemode = getvalue("CountInitMode", strsearch).toInt(&ok);
                    if (ok) if (savemode < lastInterval) dev->ui2423->Interval->setCurrentIndex(savemode);
                    int saveen = getvalue("CountInitEnabled", strsearch).toInt(&ok);
                    if (ok) { if (saveen) dev->ui2423->countInit->setChecked(true);
                                    else  dev->ui2423->countInit->setChecked(false); }
                    int saveupdate = getvalue("SaveOnUpdate", strsearch).toInt(&ok);
                    if (ok) {
                            if (saveupdate) dev->ui2423->SaveOnUpdate->setCheckState(Qt::Checked);
                            else dev->ui2423->SaveOnUpdate->setCheckState(Qt::Unchecked); }
                    modeChanged(dev);
                }
                if (dev->uiLedOW) {
                    int led = getvalue("LED_Mode", strsearch).toInt(&ok);
                    if (ok) {
                        if (led) dev->uiLedOW->radioButtonLED->setChecked(Qt::Checked);
                        else dev->uiLedOW->radioButtonVolet->setChecked(Qt::Checked); } }
                if (dev->uiLCDOW) {
                    dev->uiLCDOW->Coef->setText(getvalue("LCD_Coef", strsearch));
                } } }
}


void HA7netPlugin::timerconvertout()
{
    fifoListRemoveFirst();
    busy = false;
    mui->LockEnable->setEnabled(!busy);
    emit requestdone();
}


void HA7netPlugin::noRequest()
{
    fifoListRemoveFirst();
    emit requestdone();
}


void HA7netPlugin::ReadClick()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (item) {
        QStringList split = item->text().split(" ");
        QString devRomID = split.first();
        readDevice(devRomID); }
}


void HA7netPlugin::ReadClick(QListWidgetItem *item)
{
    QStringList split = item->text().split(" ");
    QString devRomID = split.first();
    readDevice(devRomID);
}


void HA7netPlugin::ShowClick()
{
    ShowClick(mui->devices->currentItem());
}


void HA7netPlugin::on()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    on(RomID);
}


void HA7netPlugin::on(QString RomID)
{
    GenMsg("On " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if ((family == family2413) || (family == family3A2100H)) {
            GenMsg("On " + RomID);
            if (dev->ScratchPad.isEmpty()) addtofifo(ReadDualSwitch, RomID, "R");
            addtofifo(setChannelOn, RomID, "W");
            addtofifo(ReadDualSwitch, RomID, "R"); }
    else if (family == family2408) {
        addtofifo(setChannelOn, RomID, "W"); }
    else if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
            QString hex = setPowerOn "00";
            addCRC8(hex);
            hex += "FF";
            addtofifo(WriteLed, RomID16, hex, "W");
            addtofifo(ReadTemp, RomID, "R"); }
}


void HA7netPlugin::on100()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    on100(RomID);
}


void HA7netPlugin::on100(QString RomID)
{
    GenMsg("On100 " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if ((family == family2413) || (family == family3A2100H)) {
            GenMsg("On " + RomID);
            if (dev->ScratchPad.isEmpty()) addtofifo(ReadDualSwitch, RomID, "P");
            addtofifo(setChannelOn, RomID, "W"); }
    else if (family == family2408) {
        addtofifo(setChannelOn, RomID, "W");
    // ici si on est en decimal
    }
    else if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
            QString hex = setPower "64";
            addCRC8(hex);
            hex += "FF";
            addtofifo(WriteLed, RomID16, hex, "W");
            addtofifo(ReadTemp, RomID, "R"); }
}


void HA7netPlugin::off()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    off (RomID);
}

void HA7netPlugin::off(QString RomID)
{
    GenMsg("Off " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if ((family == family2413) || (family == family3A2100H)) {
            GenMsg("Off " + RomID);
            if (dev->ScratchPad.isEmpty()) addtofifo(ReadDualSwitch, RomID, "P");
            addtofifo(setChannelOff, RomID, "W"); }
    else if (family == family2408) {
        addtofifo(setChannelOff, RomID, "W"); }
    else if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
            QString hex = setPowerOff "00";
            addCRC8(hex);
            hex += "FF";
            addtofifo(WriteLed, RomID16, hex, "W");
            addtofifo(ReadTemp, RomID, "R"); }
}


void HA7netPlugin::dim()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    dim(RomID);
}

void HA7netPlugin::dim(QString RomID)
{
    GenMsg("Dim " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
        bool ok;
        int ledState = dev->ScratchPad.mid(0,2).toInt(&ok, 16);
        if (ledState >= 20) ledState -= 10; else ledState -= 2;
        if (ledState < 0) ledState = 0;
        QString hex = setPower + QString("%1").arg(ledState, 2, 16, QChar('0'));
        addCRC8(hex);
        hex += "FF";
        addtofifo(WriteLed, RomID16, hex, "W");
        addtofifo(ReadTemp, RomID, "R"); }
}

void HA7netPlugin::bright()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    bright(RomID);
}


void HA7netPlugin::bright(QString RomID)
{
    GenMsg("Bright " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
        bool ok;
        int ledState = dev->ScratchPad.mid(0,2).toInt(&ok, 16);
        if (ledState >= 20) ledState += 10; else ledState += 2;
        if (ledState > 100) ledState = 100;
        QString hex = setPower + QString("%1").arg(ledState, 2, 16, QChar('0'));
        addCRC8(hex);
        hex += "FF";
        addtofifo(WriteLed, RomID16, hex, "W");
        addtofifo(ReadTemp, RomID, "R"); }
}


void HA7netPlugin::open()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    open(RomID);
}


void HA7netPlugin::open(QString RomID)
{
    // Command / Data / crc8Table
    //64 + data (1 Byte) + crc(1 Bytes);
    GenMsg("Open " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
            QString hex = voletOpen "00";
            addCRC8(hex);
            hex += "FF";
            addtofifo(WriteLed, RomID16, hex, "W"); }
}



void HA7netPlugin::setTTV(QString RomID)
{
// Command / Data / crc8Table
//6A + data (1 Byte) + crc(1 Bytes);
    GenMsg("setTTV " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
    QString hex = setTTVcmd + QString("%1").arg(dev->uiLedOW->TTV->value(), 2, 16, QChar('0')).toUpper();
    addCRC8(hex);
    hex += "FF";
    addtofifo(WriteLed, RomID, hex, "W"); }
}

void HA7netPlugin::setTTV()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    setTTV(RomID);
}


void HA7netPlugin::setValue(QString RomID, double value)
{
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            if (family == family2408) {
/*
QString hex = QString("%1").arg(uchar(OLSR), 2, 16, QChar('0')).toUpper();
QString s = dev->ScratchPad.replace(8, 2, hex);
dev->ScratchPad = s;
hex += QString("%1").arg(uchar(~OLSR), 2, 16, QChar('0'));
return hex; }
*/
            }
            if (family == familyLedOW) {
                bool ok;
                // Command / Data / crc8Table
                // 61 + data (1 Byte) + crc(1 Bytes);
                int v = int(value);
                QString hex;
                if (dev->uiLedOW->radioButtonLED->isChecked()) {
                    if (value < 0) v = 0;
                    if (value > 100) v = 100;
                    hex = setPower + QString("%1").arg(v, 2, 16, QChar('0')); }
                else {
                    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
                    if (v == 0) hex = voletClose "00";
                    else if (v >= dev->uiLedOW->TTV->value()) hex = voletOpen "00";
                    else if (v > dev->value) hex = voletOpen + QString("%1").arg(int(value - dev->value), 2, 16, QChar('0'));
                    else if (v < dev->value) hex = voletClose + QString("%1").arg(int(dev->value - value), 2, 16, QChar('0'));
                }
                QVector <uint8_t> hexv;
                for (int n=0; n<hex.length(); n+=2) hexv.append(uint8_t(hex.mid(n, 2).toUInt(&ok, 16)));
                uint8_t crccalc = calcCRC8(hexv);
                // add crc
                hex += QString("%1").arg(crccalc, 2, 16, QChar('0'));
                hex += "FF";
                addtofifo(WriteLed, RomID, hex, "W");
                addtofifo(ReadTemp, RomID, "R"); }
        }
    }
}


void HA7netPlugin::setVolet(QString RomID)
{
    // Command / Data / crc8Table
    //65 + data (1 Byte) + crc(1 Bytes);
    GenMsg("setVolet " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if (family == familyLedOW) { setValue(RomID, dev->uiLedOW->spinBoxVolet->value()); }
}


void HA7netPlugin::setVolet()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    setVolet(RomID);
}


void HA7netPlugin::close()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    close(RomID);
}


void HA7netPlugin::setConfig2438()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            int cfg = 0;
            if (dev->ui2438->IADButton->isChecked()) cfg += 1;
            if (dev->ui2438->CAButton->isChecked()) cfg += 2;
            if (dev->ui2438->EEButton->isChecked()) cfg += 4;
            if (dev->ui2438->ADButton->isChecked()) cfg += 8;
            QString scratchpad = QString("%1").arg(cfg, 2, 16, QChar('0')).toUpper();
            addtofifo(WriteScratchpad00, RomID, scratchpad, "W");
            addtofifo(CopyScratchpad00, RomID, "W"); }
    }
}


QString HA7netPlugin::toHex(QByteArray data)
{
    QString hex;
    for (int n=0; n<data.length(); n++) hex += QString("%1").arg(uchar(data[n]), 2, 16, QChar('0'));
    return hex.toUpper();
}



void HA7netPlugin::state2408(bool state)
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            if (state) dev->isOutput = false; else dev->isOutput = true; }
    }
    addtofifo(ReadPIO, RomID, "R");
}


void HA7netPlugin::state2413(bool state)
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            if (state) dev->isOutput = false; else dev->isOutput = true; }
    }
    addtofifo(ReadDualSwitch, RomID, "R");
}


void HA7netPlugin::read2408(int)
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    addtofifo(ReadPIO, RomID, "R");
}


void HA7netPlugin::read2413(int)
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    addtofifo(ReadDualSwitch, RomID, "R");
}

void HA7netPlugin::writeConfig()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            bool ok;
            //F5 + ID (1 Byte) + Address(1 Byte) + Data (8 Bytes) + crc(1 Bytes);
            QString hex = writeMemoryLCD;
            // ID
            if (RomID.right(2) == "_A") hex += "00";
            if (RomID.right(2) == "_B") hex += "01";
            if (RomID.right(2) == "_C") hex += "02";
            if (RomID.right(2) == "_D") hex += "03";
            // Address
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            // Nothing
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            // FontSize
            hex += QString("%1").arg(uchar((dev->uiLCDOW->Fontsize->value()) + dev->uiLCDOW->TextFontSize->value() * 0x10) , 2, 16, QChar('0'));
            // XPos
            hex += QString("%1").arg(uchar(dev->uiLCDOW->XPos->value()), 2, 16, QChar('0'));
            // YPos
            hex += QString("%1").arg(uchar(dev->uiLCDOW->YPos->value()), 2, 16, QChar('0'));
            // XposTxt
            hex += QString("%1").arg(uchar(dev->uiLCDOW->XPosTxt->value()), 2, 16, QChar('0'));
            // YposTxt
            hex += QString("%1").arg(uchar(dev->uiLCDOW->YPosTxt->value()), 2, 16, QChar('0'));
            // Digits
            hex += QString("%1").arg(uchar(dev->uiLCDOW->Digits->value()), 2, 16, QChar('0'));
            // Virgule
            hex += QString("%1").arg(uchar(dev->uiLCDOW->Virgule->value()), 2, 16, QChar('0'));
            // calc crc
            QVector <uint8_t> scratchpad;
            for (int n=0; n<hex.length(); n+=2) scratchpad.append(uint8_t(hex.mid(n, 2).toUInt(&ok, 16)));
            uint8_t crccalc = calcCRC8(scratchpad);
            // add crc
            hex += QString("%1").arg(crccalc, 2, 16, QChar('0'));
            addtofifo(WriteValText, RomID, hex, "W");
            hex = writeMemoryLCD;
            // ID
            if (RomID.right(2) == "_A") hex += "00";
            if (RomID.right(2) == "_B") hex += "01";
            if (RomID.right(2) == "_C") hex += "02";
            if (RomID.right(2) == "_D") hex += "03";
            // Address
            hex += QString("%1").arg(uchar(8), 2, 16, QChar('0'));
            // CoefMult
            hex += QString("%1").arg(uchar(dev->uiLCDOW->CoefMult->value()), 2, 16, QChar('0'));
            // CoefDiv
            hex += QString("%1").arg(uchar(dev->uiLCDOW->CoefDiv->value()), 2, 16, QChar('0'));
            // 0
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            // 0
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            // 0
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            // 0
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            // 0
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            // 0
            hex += QString("%1").arg(uchar(0), 2, 16, QChar('0'));
            scratchpad.clear();
            for (int n=0; n<hex.length(); n+=2) scratchpad.append(uint8_t(hex.mid(n, 2).toUInt(&ok, 16)));
            crccalc = calcCRC8(scratchpad);
            // add crc
            hex += QString("%1").arg(crccalc, 2, 16, QChar('0'));
            addtofifo(WriteValText, RomID, hex, "W"); }
    }
}


void HA7netPlugin::writeSuffix()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            bool ok;
            //F5 + ID (1 Byte) + Address(1 Byte) + Data (8 Bytes) + crc(1 Bytes);
            QString hex = writeMemoryLCD;
            // ID
            if (RomID.right(2) == "_A") hex += "00";
            if (RomID.right(2) == "_B") hex += "01";
            if (RomID.right(2) == "_C") hex += "02";
            if (RomID.right(2) == "_D") hex += "03";
            // Address
            hex += QString("%1").arg(uchar(ADDR_Suffix), 2, 16, QChar('0'));
            // String
            QByteArray my;
            my.append(dev->uiLCDOW->suffixStr->text().toLatin1());
            int L = my.length();
            if (L > 7) my.chop(L-7);
            while (my.length() < 7) { my.append(char(0)); }
            // Add nullptr terminsaison
            hex += toHex(my) + "00";
            QVector <uint8_t> scratchpad;
            for (int n=0; n<hex.length(); n+=2) scratchpad.append(uint8_t(hex.mid(n, 2).toUInt(&ok, 16)));
            uint8_t crccalc = calcCRC8(scratchpad);
            // add crc
            hex += QString("%1").arg(crccalc, 2, 16, QChar('0'));
            addtofifo(WriteValText, RomID, hex, "W"); }
    }
}

void HA7netPlugin::writePrefix()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            bool ok;
        //F5 + ID (1 Byte) + Address(1 Byte) + Data (8 Bytes) + crc(1 Bytes);
        QString hex = writeMemoryLCD;
        // ID
        if (RomID.right(2) == "_A") hex += "00";
        if (RomID.right(2) == "_B") hex += "01";
        if (RomID.right(2) == "_C") hex += "02";
        if (RomID.right(2) == "_D") hex += "03";
        // Address
        hex += QString("%1").arg(uchar(ADDR_Prefix), 2, 16, QChar('0'));
        // String
            QByteArray my;
            my.append(dev->uiLCDOW->prefixStr->text().toLatin1());
            int L = my.length();
            if (L > 7) my.chop(L-7);
            while (my.length() < 7) { my.append(char(0)); }
         // Add nullptr terminsaison
            hex += toHex(my) + "00";
         // calc crc
            QVector <uint8_t> scratchpad;
            for (int n=0; n<hex.length(); n+=2) scratchpad.append(uint8_t(hex.mid(n, 2).toUInt(&ok, 16)));
            uint8_t crccalc = calcCRC8(scratchpad);
        // add crc
             hex += QString("%1").arg(crccalc, 2, 16, QChar('0'));
             addtofifo(WriteValText, RomID, hex, "W"); }
    }
}


void HA7netPlugin::writeValTxt()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) {
            bool ok;
            // AB + ID (1 Byte) + Length (max 20) + string + crc (1 Bytes) of all previous bytes including command AB
            QString hex = "AB";
            // ID
            if (RomID.right(2) == "_A") hex += "00";
            if (RomID.right(2) == "_B") hex += "01";
            if (RomID.right(2) == "_C") hex += "02";
            if (RomID.right(2) == "_D") hex += "03";
            QByteArray my;
            my.append(dev->uiLCDOW->textValue->text().toLatin1());
             // length
            hex += QString("%1").arg(my.length()+1, 2, 16, QChar('0'));
            // string
           hex += toHex(my) + "00";
           QVector <uint8_t> scratchpad;
           for (int n=0; n<hex.length(); n+=2) scratchpad.append(uint8_t(hex.mid(n, 2).toUInt(&ok, 16)));
           uint8_t crccalc = calcCRC8(scratchpad);
           // add crc
           hex += QString("%1").arg(crccalc, 2, 16, QChar('0'));
           hex = hex.toUpper();
           addtofifo(WriteValText, RomID, hex, "W");
           //textDisplay = textValue.text();
           //if (textValue.text() != textDisplay) textValue.text() = textDisplay;
        }
    }
}


void HA7netPlugin::close(QString RomID)
{
    // Command / Data / crc8Table
    //64 + data (1 Byte) + crc(1 Bytes);
    GenMsg("Open " + RomID);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    QString RomID16 = RomID.left(16);
    QString family = RomID16.right(2);
    if (family == familyLedOW) {  // Command / Data / crc8Table 62 + data (1 Byte) + crc(1 Bytes);
            QString hex = voletClose "00";
            addCRC8(hex);
            hex += "FF";
            addtofifo(WriteLed, RomID16, hex, "W"); }
}


void HA7netPlugin::addCRC8(QString &hex)
{
    bool ok;
    QVector <uint8_t> v;
    for (int n=0; n<hex.length(); n+=2) v.append(uint8_t(hex.mid(n, 2).toUInt(&ok, 16)));
    uint8_t crccalc = calcCRC8(v);
// add crc
    hex += QString("%1").arg(crccalc, 2, 16, QChar('0')).toUpper();
}



void HA7netPlugin::readDevice(const Dev_Data *dev)
{
    QString RomID = dev->RomID.left(16);
    QString family = RomID.right(2);
    if ((family == family1820) || (family == family18B20) || (family == family1822) || (family == family1825)) {
        if (mui->Global_Convert->isChecked()) {
            addtofifo(Reset);
            addtofifo(ConvertTemp); }
        else { addtofifo(ConvertTempRomID, dev->RomID, ""); }
        addtofifo(ReadTemp, dev->RomID, ""); }
    else if (family == family2406) {
        addtofifo(ChannelAccessRead, RomID, "");  }
    else if (family == family2408) {
        addtofifo(ReadPIO, RomID, ""); }
    else if (family == family2413) {
        addtofifo(ReadDualSwitch, RomID, "");  }
    else if (family == family3A2100H) {
        addtofifo(ReadDualSwitch, RomID, "");  }
    else if (family == family2423) {
            addtofifo(ReadCounter, dev->RomID, ""); }
    else if (family == family2438) {
        addtofifo(ConvertTempRomID, RomID, "");
        addtofifo(ConvertV, RomID, "");
        addtofifo(RecallMemPage00h, RomID, "");
        addtofifo(ReadPage00h, RomID, "");
        //addtofifo(RecallMemPage01h, RomID);
        //addtofifo(ReadPage01h, RomID);
    }
    else if (family == family2450) {
        addtofifo(ReadADCPage01h, RomID, "");
        addtofifo(ConvertADC, RomID, "");
        addtofifo(ReadADC, RomID, ""); }
    else if (family == familyLedOW) {
        addtofifo(ReadTemp, RomID, ""); }
    else if (family == familyLCDOW) {
        addtofifo(ReadCounter, RomID, ""); }
}



void HA7netPlugin::readDevice(const QString &RomID)
{
    QMutexLocker locker(&readDev);
    foreach (Dev_Data *dev, dev_Data)
        if (dev->RomID == RomID) readDevice(dev);
}


QString HA7netPlugin::getDeviceCommands(const QString &devRomID)
{
    QString family = devRomID.right(2);
    QString family4 = devRomID.right(4);
    if (devRomID.contains("_")) family = family4.left(2);
    if (family == family1820) return "read";
    else if (family == family18B20) return "read=" + tr("Read");
    else if (family == family1822) return "read=" + tr("Read");
    else if (family == family1825) return "read=" + tr("Read");
    else if (family == family2406) return "read=" + tr("Read");
    else if (family == family2408) return "read=" + tr("Read");
    else if ((family == family2413) || (family == family3A2100H)) return "read=" + tr("Read") + "|on=" + tr("On") + "|off=" + tr("Off");
    else if (family == family2423) return "read=" + tr("Read");
    else if (family == family2438) return "read=" + tr("Read");
    else if (family == family2450) return "read=" + tr("Read");
    else if (family == familyLedOW) return "read=" + tr("Read") + "|on=" + tr("On") + "|off=" + tr("Off") + "|dim=" + tr("Dim") + "|open=" + tr("Open") + "|close=" + tr("Close");
    else if (family == familyLCDOW) return "read=" + tr("Read");
    return "";
}


QString HA7netPlugin::getOffCommand(QString RomID)
{
    QString family4 = RomID.right(4);
    QString family = family4.left(2);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return "";
    if (family == family2408) {
        bool ok;
        int OLSR = dev->ScratchPad.mid(8,2).toInt(&ok, 16);
        if (family4 == family2408_A) OLSR |= 0x01;
        if (family4 == family2408_B) OLSR |= 0x02;
        if (family4 == family2408_C) OLSR |= 0x04;
        if (family4 == family2408_D) OLSR |= 0x08;
        if (family4 == family2408_E) OLSR |= 0x10;
        if (family4 == family2408_F) OLSR |= 0x20;
        if (family4 == family2408_G) OLSR |= 0x40;
        if (family4 == family2408_H) OLSR |= 0x80;
        QString hex = QString("%1").arg(uchar(OLSR), 2, 16, QChar('0')).toUpper();
        QString s = dev->ScratchPad.replace(8, 2, hex);
        dev->ScratchPad = s;
        hex += QString("%1").arg(uchar(~OLSR), 2, 16, QChar('0'));
        return hex; }
    if ((family == family2413) || (family == family3A2100H)) {
        bool ok;
        int OLSR = dev->ScratchPad.toInt(&ok, 16);
        if ((family4 == family2413_A) || (family4 == family3A2100H_A)) { OLSR |= 0x02; OLSR &= 0xDF; }
        if ((family4 == family2413_B) || (family4 == family3A2100H_B)) { OLSR |= 0x08; OLSR &= 0x7F; }
        QString cmd = QString("%1").arg(uchar(OLSR), 2, 16, QChar('0')).toUpper();
        dev->Command = cmd;
        OLSR &= 0x0F;
        int bit2 = (OLSR & 0x02) / 2;
        int bit4 = (OLSR & 0x08) / 4;
        int data = (bit2 + bit4) | 0xFC;
        QString hex = QString("%1").arg(uchar(data), 2, 16, QChar('0')).toUpper();
        hex += QString("%1").arg(uchar(~data), 2, 16, QChar('0')).toUpper();
        return hex; }
    return "";
}


QString HA7netPlugin::getOnCommand(QString RomID)
{
    QString family4 = RomID.right(4);
    QString family = family4.left(2);
    Dev_Data *dev = devOf(RomID);
    if (!dev) return "";
    if ((family == family2413) || (family == family3A2100H)) {
        bool ok;
        int OLSR = dev->ScratchPad.toInt(&ok, 16);
        if ((family4 == family2413_A) || (family4 == family3A2100H_A)) { OLSR &= 0xFD; OLSR |= 0x20; }
        if ((family4 == family2413_B) || (family4 == family3A2100H_B)) { OLSR &= 0xF7; OLSR |= 0x80; }
        QString cmd = QString("%1").arg(uchar(OLSR), 2, 16, QChar('0')).toUpper();
        dev->Command = cmd;
        OLSR &= 0x0F;
        int bit2 = (OLSR & 0x02) / 2;
        int bit4 = (OLSR & 0x08) / 4;
        int data = (bit2 + bit4) | 0xFC;
        QString hex = QString("%1").arg(uchar(data), 2, 16, QChar('0')).toUpper();
        hex += QString("%1").arg(uchar(~data), 2, 16, QChar('0')).toUpper();
        return hex; }
    if (family == family2408) {
        bool ok;
        int OLSR = dev->ScratchPad.mid(8,2).toInt(&ok, 16);
        if (family4 == family2408_A) OLSR &= 0xFE;
        if (family4 == family2408_B) OLSR &= 0xFD;
        if (family4 == family2408_C) OLSR &= 0xFB;
        if (family4 == family2408_D) OLSR &= 0xF7;
        if (family4 == family2408_E) OLSR &= 0xEF;
        if (family4 == family2408_F) OLSR &= 0xDF;
        if (family4 == family2408_G) OLSR &= 0xBF;
        if (family4 == family2408_H) OLSR &= 0x7F;
        QString hex = QString("%1").arg(uchar(OLSR), 2, 16, QChar('0')).toUpper();
        QString s = dev->ScratchPad.replace(8, 2, hex);
        dev->ScratchPad = s;
        hex += QString("%1").arg(uchar(~OLSR), 2, 16, QChar('0'));
        return hex; }
    return "";
}


void HA7netPlugin::ShowClick(QListWidgetItem *item)
{
    QStringList split = item->text().split(" ");
    QString devRomID = split.first();
    QString scratchpad = scratchpadOf(devRomID);
    if (lastW) {
        lastW->hide();
        mui->gridLayout->removeWidget(lastW);
        lastW = nullptr; }
    QString family = devRomID.right(2);
    QString family4 = devRomID.right(4);
    if (devRomID.contains("_")) family = family4.left(2);
    if (family == family1820) display1820(devRomID, scratchpad);
    else if (family == family18B20) display18b20(devRomID, scratchpad);
    else if (family == family1822) display1822(devRomID, scratchpad);
    else if (family == family1825) display1825(devRomID, scratchpad);
    else if (family == family2406) display2406(devRomID, scratchpad);
    else if (family == family2408) display2408(devRomID, scratchpad);
    else if (family == family2413) display2413(devRomID, scratchpad);
    else if (family == family3A2100H) display2413(devRomID, scratchpad);
    else if (family == family2423) display2423(devRomID, scratchpad);
    else if (family == family2438) display2438(devRomID, scratchpad);
    else if (family == family2450) display2450(devRomID, scratchpad);
    else if (family == familyLedOW) displayLedOW(devRomID, scratchpad);
    else if (family == familyLCDOW) displayLCDOW(devRomID, scratchpad);
    else mui->groupBox->setTitle("...");
    if (lastW) lastW->show();
    emit(deviceSelected(devRomID));
}


bool HA7netPlugin::setScratchpad1820(QString RomID, QString scratchpad)
{
    if (scratchpad == "FFFFFFFFFFFFFFFFFF") { return false; }
    if (scratchpad == "000000000000000000") { return false; }
    if (!checkCRC8(scratchpad)) { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
    double T = calcultemperature1820(scratchpad);
    if (check85(RomID, T)) { return false; }
    if ((T < -55) or (T > 125))  { return false; }
    setValue(RomID, scratchpad, T);
    return true;
}


bool HA7netPlugin::setScratchpad18b20(QString RomID, QString scratchpad)
{
    if (scratchpad == "FFFFFFFFFFFFFFFFFF") { return false; }
    if (scratchpad == "000000000000000000") { return false; }
    if (!checkCRC8(scratchpad)) { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
    double T = calcultemperature1822(scratchpad);
    if (check85(RomID, T)) { return false; }
    if ((T < -55) or (T > 125))  { return false; }
    setValue(RomID, scratchpad, T);
    return true;
}


bool HA7netPlugin::setScratchpad1822(QString RomID, QString scratchpad)
{
    if (scratchpad == "FFFFFFFFFFFFFFFFFF") { GenMsg(RomID + " no answer"); return false; }
    if (scratchpad == "000000000000000000") { return false; }
    if (!checkCRC8(scratchpad)) { return false; }
    double T = calcultemperature1822(scratchpad);
    if (check85(RomID, T)) { return false; }
    if ((T < -55) or (T > 125))  { return false; }
    setValue(RomID, scratchpad, T);
    return true;
}


bool HA7netPlugin::setScratchpad1825(QString RomID, QString scratchpad)
{
    if (scratchpad == "FFFFFFFFFFFFFFFFFF") { GenMsg(RomID + " no answer"); return false; }
    if (scratchpad == "000000000000000000") { GenMsg(RomID + " bus shorted"); return false; }
    if (!checkCRC8(scratchpad)) { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
    double T = calcultemperature1825(scratchpad);
    if (check85(RomID, T)) { return false; }
    if ((T < -55) or (T > 125))  { return false; }
    setValue(RomID, scratchpad, T);
    return true;
}


bool HA7netPlugin::setScratchpad2406(QString, QString)
{
    return false;
}


bool HA7netPlugin::setScratchpad2408(QString RomID, QString scratchpad)
{
    bool ok = false;
    Dev_Data *devA = devOf(RomID.left(16) + "_A"); if (!devA) return false;
    Dev_Data *devB = devOf(RomID.left(16) + "_B"); if (!devB) return false;
    Dev_Data *devC = devOf(RomID.left(16) + "_C"); if (!devC) return false;
    Dev_Data *devD = devOf(RomID.left(16) + "_D"); if (!devD) return false;
    Dev_Data *devE = devOf(RomID.left(16) + "_E"); if (!devE) return false;
    Dev_Data *devF = devOf(RomID.left(16) + "_F"); if (!devF) return false;
    Dev_Data *devG = devOf(RomID.left(16) + "_G"); if (!devG) return false;
    Dev_Data *devH = devOf(RomID.left(16) + "_H"); if (!devH) return false;
    if (scratchpad == "AA") {
        Dev_Data *dev = devOf(RomID);
        if (dev->Command != "AA") setScratchpad2413(RomID, dev->Command);
        addtofifo(ReadPIO, RomID, "R");
        return true; }
    if (scratchpad == "F08800FFFFFFFFFFFFFFFFFFFF") { GenMsg(RomID + " no answer"); return false; }
    if (scratchpad == "00000000000000000000000000") { GenMsg(RomID + " bus shorted"); return false; }
    if (!checkCRC16(scratchpad))  { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
    int LogicState = scratchpad.mid(6,2).toInt(&ok, 16);
    if (!ok) { GenMsg(RomID + " : Conversion error (" + scratchpad); return false; }
    int OLSR = scratchpad.mid(8,2).toInt(&ok, 16);
    if (!ok) { GenMsg(RomID + " : Conversion error (" + scratchpad); return false; }
    int State = 0;
    int OutputLatch = 0;
    int decimalValue = 0;
// B
    if (LogicState & 0x02) State = 1; else State = 0;
    //if (OutputLatch & 0x02) OutputLatch = 0x01; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x02) >> 1) ^ devB->ui2408->InvertOut->isChecked();
    if (devB->isOutput) {
        if (OutputLatch) decimalValue += 0x02;
        setValue(devB, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 2;
        setValue(devB, scratchpad, State); }
// C
    if (LogicState & 0x04) State = 1; else State = 0;
    //if (OutputLatch & 0x04) OutputLatch = 1; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x04) >> 2) ^ devC->ui2408->InvertOut->isChecked();
    if (devC->isOutput) {
        if (OutputLatch) decimalValue += 0x04;
        setValue(devC, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 0x04;
        setValue(devC, scratchpad, State); }
// D
    if (LogicState & 0x08) State = 1; else State = 0;
    //if (OutputLatch & 0x08) OutputLatch = 1; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x08) >> 3) ^ devD->ui2408->InvertOut->isChecked();
    if (devD->isOutput) {
        if (OutputLatch) decimalValue += 0x08;
        setValue(devD, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 0x08;
        setValue(devD, scratchpad, State); }
// E
    if (LogicState & 0x10) State = 1; else State = 0;
    //if (OutputLatch & 0x10) OutputLatch = 1; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x10) >> 4) ^ devE->ui2408->InvertOut->isChecked();
    if (devE->isOutput) {
        if (OutputLatch) decimalValue += 0x10;
        setValue(devE, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 0x10;
        setValue(devE, scratchpad, State); }
// F
    if (LogicState & 0x20) State = 1; else State = 0;
    //if (OutputLatch & 0x20) OutputLatch = 1; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x20) >> 5) ^ devF->ui2408->InvertOut->isChecked();
    if (devF->isOutput) {
        if (OutputLatch) decimalValue += 0x20;
        setValue(devF, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 0x20;
        setValue(devF, scratchpad, State); }
// G
    if (LogicState & 0x40) State = 1; else State = 0;
    //if (OutputLatch & 0x40) OutputLatch = 1; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x40) >> 6) ^ devG->ui2408->InvertOut->isChecked();
    if (devG->isOutput) {
        if (OutputLatch) decimalValue += 0x40;
        setValue(devG, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 0x40;
        setValue(devG, scratchpad, State); }
// H
    if (LogicState & 0x80) State = 1; else State = 0;
    //if (OutputLatch & 0x80) OutputLatch = 1; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x80) >> 7) ^ devH->ui2408->InvertOut->isChecked();
    if (devH->isOutput) {
        if (OutputLatch) decimalValue += 0x80;
        setValue(devH, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 0x80;
        setValue(devH, scratchpad, State); }
// A
    if (LogicState & 0x01) State = 1; else State = 0;
    //if (OutputLatch & 0x01) OutputLatch = 1; else OutputLatch = 0;
    OutputLatch = ((OLSR & 0x01)) ^ devA->ui2408->InvertOut->isChecked();
    if (devA->isOutput) {
        if (OutputLatch) decimalValue += 0x01;
        if (devA->ui2408->operateDecimal->isChecked()) setValue(devA, scratchpad, decimalValue);
        else setValue(devA, scratchpad, OutputLatch); }
    else {
        if (State) decimalValue += 1;
        if (devA->ui2408->operateDecimal->isChecked()) setValue(devA, scratchpad, decimalValue);
        else setValue(devA, scratchpad, State); }
    return true;
}


bool HA7netPlugin::setScratchpad2413(QString RomID, QString s)
{
    QString scratchpad = s.right(2);
    bool ok = false;
    int AState = 0;
    int BState = 0;
    int OutputLatchA = 0;
    int OutputLatchB = 0;
    int COMP = 0;
    Dev_Data *devA = devOf(RomID.left(16) + "_A");
    Dev_Data *devB = devOf(RomID.left(16) + "_B");
    if (!devA) return false;
    if (!devB) return false;
    if (scratchpad == "AA") {
        Dev_Data *dev = devOf(RomID);
        if (dev->Command != "AA") setScratchpad2413(RomID, dev->Command);
        addtofifo(ReadDualSwitch, RomID, "R");
        return true; }
    if (scratchpad == "FF") { GenMsg(RomID + " no answer"); return false; }
    if (scratchpad == "00") { GenMsg(RomID + " bus shorted"); return false; }
    int OLSR = scratchpad.toInt(&ok, 16) & 0x0F;
    if (!ok) { GenMsg(RomID + " : Conversion error (" + scratchpad); return false; }
    COMP = (scratchpad.toInt(&ok, 16) & 0xF0) / 0x10;
    if (!ok) { GenMsg(RomID + " : Conversion error (" + scratchpad); return false; }
    if ((OLSR + COMP) != 0x0F) { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
    int decimalValue = 0;
    if (OLSR & 0x01) AState = 1; else AState = 0;
    //if (OLSR & 0x02) { if (devA->ui2413->InvertOut->isChecked()) OutputLatchA = 0; else OutputLatchA = 1; } else { if (devA->ui2413->InvertOut->isChecked()) OutputLatchA = 1; else OutputLatchA = 0; }
    OutputLatchA = ((OLSR & 0x02) >> 1) ^ devA->ui2413->InvertOut->isChecked();
    if (OLSR & 0x04) BState = 1; else BState = 0;
    //if (OLSR & 0x08) { if (devB->ui2413->InvertOut->isChecked()) OutputLatchB = 0; else OutputLatchB = 1; } else { if (devB->ui2413->InvertOut->isChecked()) OutputLatchB = 1; else OutputLatchB = 0; }
    OutputLatchB = ((OLSR & 0x08) >> 3) ^ devB->ui2413->InvertOut->isChecked();
// B
    if (devB->isOutput) {
        if (OutputLatchB) decimalValue += 2;
        setValue(QString(RomID.left(16) + "_B"), scratchpad, OutputLatchB); }
    else {
        if (BState) decimalValue += 2;
        setValue(QString(RomID.left(16) + "_B"), scratchpad, BState); }
// A
    if (devA->isOutput) {
        if (OutputLatchA) decimalValue += 1;
        if (devA->ui2413->operateDecimal->isChecked()) setValue(QString(RomID.left(16) + "_A"), scratchpad, decimalValue);
        else setValue(QString(RomID.left(16) + "_A"), scratchpad, OutputLatchA);
        }
    else {
        if (AState) decimalValue += 1;
        if (devA->ui2413->operateDecimal->isChecked()) setValue(QString(RomID.left(16) + "_A"), scratchpad, decimalValue);
        else setValue(QString(RomID.left(16) + "_A"), scratchpad, AState); }
    return true;
}


bool HA7netPlugin::setScratchpad2423(QString RomID, QString scratchpad)
{
    bool ok = false;
    Dev_Data *devA = devOf(RomID);
    Dev_Data *devB = devOf(RomID);
    if ((!devA) and (!devB)) return false;
    if (scratchpad.isEmpty()) { GenMsg(RomID + "  scratchpad empty !!!"); return false; }
    if (devA and (scratchpad == "5ADF01FFFFFFFFFFFFFFFFFFFFFF")) { GenMsg(RomID + " No answer"); return false; }
    if (devB and (scratchpad == "A5FF01FFFFFFFFFFFFFFFFFFFFFF")) { GenMsg(RomID + " No answer"); return false; }
    if (scratchpad == "0000000000000000000000000000") { GenMsg(RomID + " bus shorted"); return false; }
    if (!checkCRC16(scratchpad)) { GenMsg(RomID + " CRC16 error"); return false; }
    Dev_Data *dev = devA;
    if (!dev) dev = devB;
    QString countLSB = scratchpad.mid(10,2) + scratchpad.mid(8,2);
    QString countMSB = scratchpad.mid(14,2) + scratchpad.mid(12,2);
    quint64 CLSB = quint64(countLSB.toInt(&ok,16));
    if (!ok) { GenMsg(RomID + " : Conversion error (" + scratchpad); return false; }
    quint64 CMSB = quint64(countMSB.toInt(&ok,16));
    if (!ok) { GenMsg(RomID + " : Conversion error (" + scratchpad); return false; }
    quint64 counter = (0x10000 * CMSB) + CLSB;
    QDateTime now = QDateTime::currentDateTime();
    double V = 0;
    if (!dev->lastCounter.valid) {
        dev->lastCounter.value = counter;
        dev->lastCounter.valid = true;
    }
    else if (counter < dev->lastCounter.value) { dev->lastCounter.value = counter; } // if device is unplugged it strart from 0 and count again, we must restart lastCounter
    dev->Delta = counter - dev->lastCounter.value;
    dev->lastCounter.value = counter;
    dev->count = counter;
    switch (dev->ui2423->counterMode->currentIndex()) {
        case absoluteMode :
            setValue(dev, scratchpad, counter);
        break;
        case relativeMode :
            if (dev->ui2423->Offset->value() != 0)
                if (dev->Delta > quint64(dev->ui2423->Offset->value())) { dev->Delta = 0; }
            V = dev->Delta;
            setValue(dev, scratchpad, V);
            // only if lastCounter was updated otherwise display zero for nothing
        break;
        case offsetMode :
        //qInfo() << dev->ui2423->nextOffsetAdjust->dateTime().toString();
                V = counter - quint64(dev->ui2423->Offset->value());
                if (!((dev->ui2423->countInit->isChecked()) and (dev->ui2423->SaveOnUpdate->isChecked()))) setValue(dev, scratchpad, V);
                if (dev->ui2423->countInit->isChecked()) {
                    if (now.secsTo(dev->ui2423->nextOffsetAdjust->dateTime()) < 0) {
                        if (dev->ui2423->SaveOnUpdate->isChecked()) setValue(dev, scratchpad, V);
                        dev->ui2423->Offset->setValue(int(counter));
                        qint64 interval = getSecs(dev->ui2423->Interval->currentIndex());
                        QDateTime next = dev->ui2423->nextOffsetAdjust->dateTime();
                        do  { next = next.addSecs(interval); } while (next.secsTo(now) >= 0);
                        dev->ui2423->nextOffsetAdjust->setDateTime(setDateTimeS0(next));
                    }
                }
        break;
        }
    return true;
}


bool HA7netPlugin::setScratchpad2438(QString RomID, QString scratchpad)
{
    QString str = getvalue("page01h", scratchpad);
    if (!str.isEmpty()) scratchpad = str;
    if (scratchpad == "FFFFFFFFFFFFFFFFFF") { GenMsg(RomID + " no answer"); return false; }
    if (scratchpad == "000000000000000000") { GenMsg(RomID + " bus shorted"); return false; }
    if (!checkCRC8(scratchpad))  { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
// page 0
    if (str.isEmpty()) {
        // Status/Configuration
        QString config = scratchpad.mid(0,2);
        bool ok;
        quint16 C = quint16(config.toInt(&ok, 16));
        setValue(QString(RomID + "_I"), scratchpad, C);
        // T°
        double Value = calcultemperature2438(scratchpad);
        if (check85(RomID, Value)) { GenMsg(RomID + " check 85 error"); return false; }
        if ((Value < -55) or (Value > 125))  { GenMsg(RomID + " Temperature out fo range");return false; }
        setValue(QString(RomID + "_T"), scratchpad, Value);
        // V
        Value = calculvoltage2438(scratchpad);
        setValue(QString(RomID + "_V"), scratchpad, Value);
        // A
        Value = calculcurrent2438(scratchpad);
        setValue(QString(RomID + "_A"), scratchpad, Value);
    }
    else {
// page 1
        //scratchpad = str;
        //QString ICA;
        //ICA = scratchpad.mid(8,2);
        //double Value = double(ICA.toInt(&ok, 16));
        //if (!ok)  { GenMsg(RomID + " ICA conversion error"); return false; }
        //setValue(QString(RomID + "_I"), scratchpad, Value);
    }
   return true;
}


bool HA7netPlugin::setScratchpad2450(QString, QString)
{
    return false;
}


bool HA7netPlugin::setScratchpadLedOW(QString RomID, QString scratchpad)
{
    Dev_Data *dev = devOf(RomID);
    bool ok;
    // "610054AA"
    if (scratchpad.length() == 8)
    {
       if (scratchpad.right(2) == "AA")
       {
           QString command = scratchpad.left(2);
           int data = scratchpad.mid(2,2).toInt(&ok, 16);
           if ((command == setPower) && ok) { if (dev->uiLedOW->radioButtonLED->isChecked()) dev->value = data; emit(newDeviceValue(RomID, QString("%1").arg(data))); }
           else if (command == setPowerOff) { if (dev->uiLedOW->radioButtonLED->isChecked()) dev->value = 0; emit(newDeviceValue(RomID, "0")); }
           //else if (command == setPowerOn) { }
           //else if (command == voletOpen) { voletState += data; if (voletState > TTV.value()) voletState = TTV.value(); if (ui.radioButtonVolet->isChecked()) setMainValue(voletState, true); }
           //else if (command == voletClose) { voletState -= data; if (voletState < 0) voletState = 0; if (ui.radioButtonVolet->isChecked()) setMainValue(voletState, true); }
           return true;
       }
       else { GenMsg(RomID + " wrong answer"); return true; }
    }
    if (scratchpad == "FFFFFFFFFFFFFFFFFF") { GenMsg(RomID + " no answer"); return false; }
    if (scratchpad == "000000000000000000") { GenMsg(RomID + " bus shorted"); return false; }
    if (!checkCRC8(scratchpad))  { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
    int ledState = scratchpad.mid(0,2).toInt(&ok, 16);
    if (!ok ) { GenMsg(RomID + " : Bad ID conversion " + scratchpad); return false; }
    int voletState = scratchpad.mid(2,2).toInt(&ok, 16);
    if (!ok ) { GenMsg(RomID + " : Bad ID conversion " + scratchpad); return false; }
    int ttv = scratchpad.mid(4,2).toInt(&ok, 16);
    if (dev->uiLedOW->TTV->value() != ttv) dev->uiLedOW->TTV->setValue(ttv);
    int voletManual = scratchpad.mid(14,2).toInt(&ok, 16) & 0x0001;
    if (dev) {
        if (ok && voletManual && (dev->lastManual != voletManual)) dev->manual = true;
        dev->lastManual = voletManual;
        if (dev->uiLedOW->radioButtonLED->isChecked()) setValue(RomID, scratchpad, ledState);
        else setValue(RomID, scratchpad, voletState);
    }
    //setValue(QString(RomID + "_L"), scratchpad, ledState);
    //setValue(QString(RomID + "_V"), scratchpad, voletState);
    return true;
}


bool HA7netPlugin::setScratchpadLCDOW(QString RomID, QString scratchpad)
{
    bool ok = false;
    if (scratchpad.isEmpty()) { GenMsg(RomID + "  scratchpad empty !!!"); return false; }
    if (scratchpad == "5ADF01FFFFFFFFFFFFFFFFFFFFFF") { GenMsg(RomID + " No answer"); return false; }
    if (scratchpad == "0000000000000000000000000000") { GenMsg(RomID + " bus shorted"); return false; }
    if (scratchpad.length() == 8) // 5A030000 Write value from HA7net
    {
        if (!checkCRC8(scratchpad.left(10)))  { GenMsg(RomID + " : Bad CRC " + scratchpad); return false; }
        int ID = scratchpad.mid(2,2).toInt(&ok, 16);
        if (!ok ) { GenMsg(RomID + " : Bad ID conversion " + scratchpad); return false; }
        if (ID == 0) { setLCDValue(QString(RomID + "_A"), scratchpad); return true; }
        if (ID == 1) { setLCDValue(QString(RomID + "_B"), scratchpad); return true; }
        if (ID == 2) { setLCDValue(QString(RomID + "_C"), scratchpad); return true; }
        if (ID == 3) { setLCDValue(QString(RomID + "_D"), scratchpad); return true; }
    }
    // FA00000011000000000100A1     ReadMemoryLCD return need to read ID and Address and check CRC
    if ((scratchpad.length() > 22) && (scratchpad.left(2) == readMemoryLCD))
    {
        bool crcOk = checkCRC8(scratchpad);
        int ID = scratchpad.mid(2,2).toInt(&ok, 16);
        if (!ok) { GenMsg(RomID + " ID Conversion error"); return false; }
        int Address = scratchpad.mid(4,2).toInt(&ok, 16);
        if (!ok) { GenMsg(RomID + " Address Conversion error"); return false; }
        QString Data = scratchpad.mid(6,16);
        if (Data == "FFFFFFFFFFFFFFFF") { GenMsg(RomID + " No answer"); return false; }
        if (!crcOk) { GenMsg(RomID + " Bad CRC setLCDConfig"); return false; }
        QString data = scratchpad.mid(6,16);
        if (ID == 0) setLCDConfig(QString(RomID + "_A"), Address, data);
        if (ID == 1) setLCDConfig(QString(RomID + "_B"), Address, data);
        if (ID == 2) setLCDConfig(QString(RomID + "_C"), Address, data);
        if (ID == 3) setLCDConfig(QString(RomID + "_D"), Address, data);
        if (Address == 0) ReadLCDConfig(RomID, ID, 8);
        else if (Address == 8) ReadLCDConfig(RomID, ID, 16);
        else if (Address == 16) ReadLCDConfig(RomID, ID, 32);
        else if (Address == 32) {
            if (ID < 3) ReadLCDConfig(RomID, ID+1, 0); }
        return true;
    }
    if (!checkCRC16(scratchpad)) { GenMsg(RomID + " CRC16 error"); return false; }
    int offset = 8;
    QString TStr;
    setLCDValue(QString(RomID + "_A"), QString(scratchpad.mid(2+offset,2) + scratchpad.mid(0+offset,2)));
    setLCDValue(QString(RomID + "_B"), QString(scratchpad.mid(6+offset,2) + scratchpad.mid(4+offset,2)));
    setLCDValue(QString(RomID + "_C"), QString(scratchpad.mid(10+offset,2) + scratchpad.mid(8+offset,2)));
    setLCDValue(QString(RomID + "_D"), QString(scratchpad.mid(14+offset,2) + scratchpad.mid(12+offset,2)));
    return true;
}


void HA7netPlugin::display1820(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS1820");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    dev->ui1820->RomID->setText(RomID);
    dev->ui1820->Resolution->setText("9 bits");
    lastW = dev->ui;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    dev->ui1820->T->setText(QString("%1").arg(dev->value, 0, 'f', 3));
    dev->ui1820->scratchpad->setText(scratchpad);
    dev->ui1820->TaH->setText(QString("%1").arg(calcultemperaturealarmeH(scratchpad), 0, 'f', 2));
    dev->ui1820->TaL->setText(QString("%1").arg(calcultemperaturealarmeB(scratchpad), 0, 'f', 2));
}


void HA7netPlugin::display18b20(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS18b20");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    dev->ui1820->RomID->setText(RomID);
    lastW = dev->ui;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    dev->ui1820->T->setText(QString("%1").arg(dev->value, 0, 'f', 3));
    dev->ui1820->scratchpad->setText(scratchpad);
    dev->ui1820->TaH->setText(QString("%1").arg(calcultemperaturealarmeH(scratchpad), 0, 'f', 2));
    dev->ui1820->TaL->setText(QString("%1").arg(calcultemperaturealarmeB(scratchpad), 0, 'f', 2));
    dev->ui1820->Resolution->setText(QString("%1 bits").arg(getresolution(scratchpad)));
}

void HA7netPlugin::display1822(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS1822");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    lastW = dev->ui;
    dev->ui1820->RomID->setText(RomID);
    dev->ui1820->Resolution->setText("9 bits");
    if (scratchpad.isEmpty()) {
        dev->ui1820->T->clear();
        dev->ui1820->scratchpad->clear();
        dev->ui1820->TaH->clear();
        dev->ui1820->TaL->clear();
        dev->ui1820->Resolution->clear(); }
    else {
        dev->ui1820->T->setText(QString("%1").arg(dev->value, 0, 'f', 3));
        dev->ui1820->scratchpad->setText(scratchpad);
        dev->ui1820->TaH->setText(QString("%1").arg(calcultemperaturealarmeH(scratchpad), 0, 'f', 2));
        dev->ui1820->TaL->setText(QString("%1").arg(calcultemperaturealarmeB(scratchpad), 0, 'f', 2));
        dev->ui1820->Resolution->setText(QString("%1 bits").arg(getresolution(scratchpad))); }
}


void HA7netPlugin::display1825(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS1825");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    lastW = dev->ui;
    dev->ui1820->RomID->setText(RomID);
    dev->ui1820->Resolution->setText("9 bits");
    if (scratchpad.isEmpty()) {
        dev->ui1820->T->clear();
        dev->ui1820->scratchpad->clear();
        dev->ui1820->TaH->clear();
        dev->ui1820->TaL->clear();
        dev->ui1820->Resolution->clear(); }
    else {
        dev->ui1820->T->setText(QString("%1").arg(dev->value, 0, 'f', 3));
        dev->ui1820->scratchpad->setText(scratchpad);
        dev->ui1820->TaH->setText(QString("%1").arg(calcultemperaturealarmeH(scratchpad), 0, 'f', 2));
        dev->ui1820->TaL->setText(QString("%1").arg(calcultemperaturealarmeB(scratchpad), 0, 'f', 2));
        dev->ui1820->Resolution->setText(QString("%1 bits").arg(getresolution(scratchpad))); }
}


void HA7netPlugin::display2406(QString RomID, QString)
{
    mui->groupBox->setTitle("DS2406");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    lastW = dev->ui;
    dev->ui2406->RomID->setText(RomID);
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
}


void HA7netPlugin::display2408(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS2408");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    lastW = dev->ui;
    dev->ui2408->RomID->setText(RomID);
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    bool ok;
    int State = 0;
    int LogicState = scratchpad.mid(6,2).toInt(&ok, 16);
    int OutputLatch = scratchpad.mid(8,2).toInt(&ok, 16);
    if (RomID.endsWith("_A")) {
        if (LogicState & 0x01) State = 1; else State = 0;
        if (OutputLatch & 0x01) OutputLatch = 1; else OutputLatch = 0; }
    if (RomID.endsWith("_B")) {
        if (LogicState & 0x02) State = 1; else State = 0;
        if (OutputLatch & 0x02) OutputLatch = 1; else OutputLatch = 0; }
    if (RomID.endsWith("_C")) {
        if (LogicState & 0x04) State = 1; else State = 0;
        if (OutputLatch & 0x04) OutputLatch = 1; else OutputLatch = 0; }
    if (RomID.endsWith("_D")) {
        if (LogicState & 0x08) State = 1; else State = 0;
        if (OutputLatch & 0x08) OutputLatch = 1; else OutputLatch = 0; }
    if (RomID.endsWith("_E")) {
        if (LogicState & 0x10) State = 1; else State = 0;
        if (OutputLatch & 0x10) OutputLatch = 1; else OutputLatch = 0; }
    if (RomID.endsWith("_F")) {
        if (LogicState & 0x20) State = 1; else State = 0;
        if (OutputLatch & 0x20) OutputLatch = 1; else OutputLatch = 0; }
    if (RomID.endsWith("_G")) {
        if (LogicState & 0x40) State = 1; else State = 0;
        if (OutputLatch & 0x40) OutputLatch = 1; else OutputLatch = 0; }
    if (RomID.endsWith("_H")) {
        if (LogicState & 0x80) State = 1; else State = 0;
        if (OutputLatch & 0x80) OutputLatch = 1; else OutputLatch = 0; }
    dev->ui2408->scratchpad->setText(scratchpad);
    if (dev) {
        if (dev->ScratchPad.isEmpty()) {
            dev->ui2408->Status->clear();
            dev->ui2408->Collecteur->clear(); }
        else {
            if (State) dev->ui2408->Status->setText("1  (High)"); else dev->ui2408->Status->setText("0  (Low)");
            if (OutputLatch) dev->ui2408->Collecteur->setText("1  (Opened)"); else dev->ui2408->Collecteur->setText("0  (Closed)"); }
    }
}


void HA7netPlugin::display2413(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS2413");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    dev->ui2413->RomID->setText(RomID);
    lastW = dev->ui;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    bool ok;
    int State = 0;
    int OutputLatch = 0;
    int OLSR = scratchpad.toInt(&ok, 16) & 0x0F;
    if (RomID.endsWith("_A")) {
        if (OLSR & 0x01) State = 1; else State = 0;
        if (OLSR & 0x02) OutputLatch = 1; else OutputLatch = 0;
    }
    if (RomID.endsWith("_B")) {
        if (OLSR & 0x04) State = 1; else State = 0;
        if (OLSR & 0x08) OutputLatch = 1; else OutputLatch = 0;
    }
    dev->ui2413->scratchpad->setText(scratchpad);
    if (dev) {
        if (dev->ScratchPad.isEmpty()) {
            dev->ui2413->Status->clear();
            dev->ui2413->Collecteur->clear(); }
        else {
            if (State) dev->ui2413->Status->setText("1  (High)"); else dev->ui2413->Status->setText("0  (Low)");
            if (OutputLatch) dev->ui2413->Collecteur->setText("1  (Opened)"); else dev->ui2413->Collecteur->setText("0  (Closed)"); }
    }
}


void HA7netPlugin::display2423(QString RomID, QString scratchpad)
{
    //qInfo() << "display2423 " + RomID + " " + scratchpad;
    mui->groupBox->setTitle("DS2423");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    dev->ui2423->RomID->setText(RomID);
    lastW = dev->ui;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    dev->ui2423->scratchpad->setText(scratchpad);
    if (scratchpad.isEmpty()) {
        dev->ui2423->Compteur->clear();
        dev->ui2423->Delta->clear();
        dev->ui2423->scratchpad->clear(); }
    else {
        dev->ui2423->Compteur->setText(QString("%1").arg(dev->count));
        dev->ui2423->Delta->setText(QString("%1").arg(dev->Delta));
         }
}


void HA7netPlugin::display2438(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS2438");
    Dev_Data *devT = devOf(RomID.left(16) + "_T");
    Dev_Data *devI = devOf(RomID.left(16) + "_I");
    if (!devT) return;
    if (!devI) return;
    mui->gridLayout->addWidget(devT->ui, 1, 0, 1, 1);
    devT->ui2438->RomID->setText(RomID);
    lastW = devT->ui;
    if (devT->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    devT->ui2438->temperature->setText(QString("%1").arg(devT->value, 0, 'f', 3));
    devT->ui2438->voltage->setText(QString("%1").arg(calculvoltage2438(devT->ScratchPad), 0, 'f', 3));
    devT->ui2438->current->setText(QString("%1").arg(calculcurrent2438(devT->ScratchPad), 0, 'f', 3));
    devT->ui2438->scratchpad->setText(devT->ScratchPad);
    bool ok;
    QString CONFIG = scratchpad.mid(0,2);
    int cfg = CONFIG.toInt(&ok, 16);
    if (ok) {
        devT->ui2438->IADButton->setChecked(cfg & 0x01);
        devT->ui2438->CAButton->setChecked(cfg & 0x02);
        devT->ui2438->EEButton->setChecked(cfg & 0x04);
        devT->ui2438->ADButton->setChecked(cfg & 0x08); }
}


void HA7netPlugin::display2450(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("DS2450");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    dev->ui2450->RomID->setText(RomID);
    lastW = dev->ui;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    if (scratchpad.isEmpty()) {
        dev->ui2450->T->clear();
        dev->ui2450->scratchpad->clear(); }
    else {
         }
}


void HA7netPlugin::displayLedOW(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("Led OneWire");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    bool ok;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    dev->uiLedOW->RomID->setText(RomID);
    lastW = dev->ui;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    int ledState = scratchpad.mid(0,2).toInt(&ok, 16);
    if (!ok ) { GenMsg(RomID + " : Bad ID conversion " + scratchpad);}
    int voletState = scratchpad.mid(2,2).toInt(&ok, 16);
    if (!ok ) { GenMsg(RomID + " : Bad ID conversion " + scratchpad);}
    if (scratchpad.isEmpty()) {
        dev->uiLedOW->Mode->setTitle("");
        dev->uiLedOW->pushButtonPRC->setText("---%");
    dev->uiLedOW->pushButtonVolet->setText("--"); }
    else {
        dev->uiLedOW->Mode->setTitle(scratchpad);
        dev->uiLedOW->pushButtonPRC->setText(QString("%1%").arg(ledState));
        dev->uiLedOW->pushButtonVolet->setText(QString("%1").arg(voletState));
    }
}


void HA7netPlugin::displayLCDOW(QString RomID, QString scratchpad)
{
    mui->groupBox->setTitle("LCD OneWire");
    Dev_Data *dev = devOf(RomID);
    if (!dev) return;
    mui->gridLayout->addWidget(dev->ui, 1, 0, 1, 1);
    dev->uiLCDOW->RomID->setText(RomID);
    lastW = dev->ui ;
    if (dev->ScratchPad.isEmpty()) { readDevice(RomID); return; }
    if (scratchpad.isEmpty()) {
        dev->uiLCDOW->scratchpad->clear(); }
    else {
            dev->uiLCDOW->scratchpad->setText(scratchpad);
         }
}


double HA7netPlugin::calcultemperaturealarmeH(const QString &scratch)
{
    double T;
    bool ok;
    T = scratch.mid(4,2).toInt(&ok,16);
    if (T > 128) T = 128 - T;
    return T;
}



bool HA7netPlugin::check85(QString RomID, double V)
{
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID)
        {
            if (!dev->skip85) return false;
            dev->check85Values.append(V);
            if (dev->check85Values.count() > 10) dev->check85Values.removeAt(0);
            double mean = 0;
            for (int n=0; n<dev->check85Values.count(); n++) mean += dev->check85Values[n];
            mean /= dev->check85Values.count();
            double dif = qAbs(mean - V);
            if ((AreSame(V, 85)) && (dif > 5)) return true;
            }
        }
    return false;
}


bool HA7netPlugin::AreSame(double a, double b)
{
    return std::fabs(a - b) < std::numeric_limits<double>::epsilon();
}


double HA7netPlugin::calcultemperaturealarmeB(const QString &scratch)
{
    double T;
    bool ok;
    T = scratch.mid(6,2).toInt(&ok,16);
    if (T > 128) T = 128 - T;
    return T;
}

int HA7netPlugin::getresolution(QString scratch)
{
    bool ok;
    //if (family == family1820) return 9;
    int Config = scratch.mid(8,2).toInt(&ok,16) & 0x60 >> 5;
    if ((Config == 3) and ok) return 12;
    if ((Config == 2) and ok) return 11;
    if ((Config == 1) and ok) return 10;
    if ((Config == 0) and ok) return 9;
    return 0;
}


void HA7netPlugin::SearchClick()
{
    addtofifo(Search);
}


void HA7netPlugin::IPEdited(QString ip)
{
    ipaddress = ip;
}


void HA7netPlugin::PortEdited(QString p)
{
    port = quint16(p.toInt());
}


void HA7netPlugin::setipaddress(const QString &adr)
{
    if (!adr.isEmpty()) mui->editIP->setText(adr);		// affichage de l'adresse IP
}


void HA7netPlugin::setport(const QString &p)
{
    if (!p.isEmpty()) mui->editPort->setText(p);		// affichage de l'adresse IP
}


void HA7netPlugin::log(QString str)
{
    if (mui->checkBoxLog->isChecked())
    {
        if (logStr.length() > 100000) logStr = logStr.mid(90000);
        mui->logTxt->append(str);
        mui->logTxt->moveCursor(QTextCursor::End);
    }
}


void HA7netPlugin::logEnable(int state)
{
    if (state) {
        mui->logTxt->show();
    }
    else {
        mui->logTxt->hide();
    }
}


void HA7netPlugin::setConfig(const QString &strsearch)
{
    bool Lock = false, GlobalConvert = false;
    if (getvalue("LockID", strsearch) == "1") Lock = true;
    if (getvalue("GlobalConvert", strsearch) == "1") GlobalConvert = true;
    mui->LockEnable->setChecked(Lock);
    mui->Global_Convert->setChecked(GlobalConvert);
    bool ok;
    double CD = getvalue("ConvertDelay", strsearch).toDouble(&ok);
    if ((ok) and (CD > 0)) mui->ConvertDelay->setValue(CD);
}

bool  HA7netPlugin::checkbusshorted(const QString &data)
{
// search buffer for    "Exception_String_0" TYPE="text" VALUE="Short detected on 1-Wire Bus"
    int i;
    QByteArray strsearch;
    strsearch = "\"Exception_String_0\" TYPE=\"text\" VALUE=\"Short detected on 1-Wire Bus\"";
    i = data.indexOf(strsearch);
    if (i != -1) return true; else return false;
}

bool  HA7netPlugin::checkLockIDCompliant(const QString &data)
{
// search buffer for   "A BLOCK of data may not be empty"
    int j, k;
    QByteArray strsearch;
    j = data.indexOf("Lock ID");
    k = data.indexOf("does not exist");
    if ((j != -1) and (k != -1)) return true;
    return false;
}

int HA7netPlugin::fifoListCount()
{
    QMutexLocker locker(&mutexFifoList);
    return mui->fifolist->count();
}

QString HA7netPlugin::getFifoString(const QString Order, const QString RomID, const QString Data)
{
    QString str;
    if (!Order.isEmpty()) str += "*" + Order + "#";
    if (!RomID.isEmpty()) str += "{" + RomID + "}";
    if (!Data.isEmpty()) str += "[" + Data + "]";
    return str;
}


bool HA7netPlugin::fifoListContains(const QString &str)
{
    QMutexLocker locker(&mutexFifoList);
    for (int n=0; n<mui->fifolist->count(); n++)
        if (mui->fifolist->item(n)->text().contains(str)) return true;
    return false;
}


void HA7netPlugin::addtofifo(int order)
{
    if (!OnOff) return;
    QMutexLocker locker(&mutex);
    TimerReqDone.stop();
    if (!(order < LastRequest)) return;
    QString Order = getFifoString(NetRequestMsg[order], "", "");
    if (fifoListCount() > fifomax)
    {
        //GenError(83, Order);
        TimerReqDone.start();
        return;		// si fifo trop plein on quitte
    }
    if (fifoListContains(Order)) {

        TimerReqDone.start();
        return;
    }
    fifoListAdd(Order);
    //GenMsg("addtofifo -> " + Order);
    TimerReqDone.start();
}


void HA7netPlugin::addtofifo(int order, const QString &RomID, QString priority)
{
    if (!OnOff) return;
    QMutexLocker locker(&mutex);
    TimerReqDone.stop();
    if (!(order < LastRequest)) return;
    QString Order = priority + getFifoString(NetRequestMsg[order], RomID, "");
    if (fifoListCount() > fifomax) {
        GenError(83, Order);
        TimerReqDone.start();
        return;	}	// si fifo trop plein on quitte
    if ((Order != NetRequestMsg[Reset]) and (fifoListContains(Order)))
    {
        TimerReqDone.start();
        return;
    }
    fifoListAdd(Order);
    //GenMsg("addtofifo -> " + Order);
    TimerReqDone.start();
}


void HA7netPlugin::addtofifo(int order, const QString &RomID, const QString &Data, QString priority)
{
    if (!OnOff) return;
    QMutexLocker locker(&mutex);
    TimerReqDone.stop();
    if (!(order < LastRequest)) return;
    QString Order = priority + getFifoString(NetRequestMsg[order], RomID, Data);
    if (fifoListCount() > fifomax) {
            GenError(83, Order);
            TimerReqDone.start();
            return;	}	// si fifo trop plein on quitte
    if ((Order != NetRequestMsg[Reset]) and (fifoListContains(Order)))
    {
        TimerReqDone.start();
        return;
    }
    fifoListAdd(Order);
    //GenMsg("addtofifo -> " + Order);
    TimerReqDone.start();
}


void HA7netPlugin::fifoListAdd(const QString &order)
{
    QMutexLocker locker(&mutexFifoList);
    QListWidgetItem *item = new QListWidgetItem(order);
    mui->fifolist->addItem(item);
}


void HA7netPlugin::convertSlot()
{
    convert();
}


void HA7netPlugin::convert()
{
    if (mui->Global_Convert->isChecked())
    {
        addtofifo(Reset);
        addtofifo(ConvertTemp);
    }
    else
    {
        //for (const auto& i : RomIDs) {
            //qDebug() << i;
        //}
        foreach (Dev_Data *dev, dev_Data) {
            QString family = dev->RomID.right(2);
            if ((family == family1820) || (family == family18B20) || (family == family1822) || (family == family1825))
                addtofifo(ConvertTempRomID, dev->RomID, "");
        }
    }
}

void HA7netPlugin::fifoListRemoveFirst()
{
    requestTry = 0;
    currentOrder.clear();
}


void HA7netPlugin::endofsearchrequest(const QString &data)
{
// search buffer for CLASS="HA7Value" NAME="Address_0" ID="ADDRESS_0" TYPE="text" VALUE=
QString match;
int pos, indexstringsearch = 0;
QByteArray strsearch;
    strsearch = "TYPE=\"text\" VALUE=\"";
    pos = data.indexOf(strsearch);
    do
    {
        if ((pos != -1) and (data.mid(pos + 35, 2)) == "\">")		// has found item
        {
            match = data.mid(pos + 19, 16);		// extract interesting data : RomID
            createNewDevice(match);
            indexstringsearch ++;
            strsearch = "TYPE=\"text\" VALUE=\"";
        }
        pos = data.indexOf(strsearch, pos + 1);
    } while(pos > 0);
    request = None;
    fifoListRemoveFirst();
    initialsearch = false;
    busy = false;
    mui->LockEnable->setEnabled(!busy);
}


void HA7netPlugin::createNewDevice(const QString &RomID)
{
    QStringList DevList;
    getDeviceList(RomID, DevList);
    foreach (QString dev, DevList)
        createDevice(dev);
}


void HA7netPlugin::getDeviceList(const QString RomID, QStringList &DevList)
{
    QString family = RomID.right(2);
    if (family == family2406)
    {
        DevList.append(RomID + "_A");
        //DevList.append(RomID + "_B");
    }
    else if (family == family2408)
    {
        DevList.append(RomID + "_A");
        DevList.append(RomID + "_B");
        DevList.append(RomID + "_C");
        DevList.append(RomID + "_D");
        DevList.append(RomID + "_E");
        DevList.append(RomID + "_F");
        DevList.append(RomID + "_G");
        DevList.append(RomID + "_H");
    }
    else if (isFamily2413)
    {
        DevList.append(RomID + "_A");
        DevList.append(RomID + "_B");
    }
    else if (family == family2423)
    {
        DevList.append(RomID + "_A");
        DevList.append(RomID + "_B");
    }
    else if (family == family2450)
    {
        DevList.append(RomID + "_A");
        DevList.append(RomID + "_B");
        DevList.append(RomID + "_C");
        DevList.append(RomID + "_D");
    }
    else if (family == family2438)
    {
        DevList.append(RomID + "_T");
        DevList.append(RomID + "_V");
        DevList.append(RomID + "_A");
        DevList.append(RomID + "_I");
    }
    else if (family == familyLCDOW)
    {
        DevList.append(RomID + "_A");
        DevList.append(RomID + "_B");
        DevList.append(RomID + "_C");
        DevList.append(RomID + "_D");
    }
    else if (family == familyLedOW)
    {
        DevList.append(RomID);
        //DevList.append(RomID + "_V");
        //DevList.append(RomID + "_L");
    }
    else DevList.append(RomID);
}

bool HA7netPlugin::fifoListEmpty()
{
    QMutexLocker locker(&mutexFifoList);
    if (mui->fifolist->count() != 0) return false;
    return true;
}


void HA7netPlugin::createDevice(QString newRomID)
{
    if (!scratchpadOf(newRomID).isEmpty()) return;
    if (devOf(newRomID)) {
        GenMsg(newRomID + (" already exists"));
        return;
    }
    QString RomID = newRomID;
    QString family = RomID.right(2);
    if (family.left(1) == "_")
    {
        RomID.chop(2);
        family = RomID.right(2);
    }
    // Check CRC for dallas devices
    if ((family == family18B20) or (family == family1822) or (family == family2408) or (family == family2406)
        or (isFamily2413) or (family == family2423) or (family == family2438)
        or (family == family2450) or (family == familyLCDOW))
    {
        bool ok;
        QString ID = RomID.right(14);
        QString CRC = RomID.left(2);
        uint8_t crc = uint8_t(CRC.toUInt(&ok, 16));
        QVector <uint8_t> v;
        for (int n=ID.length()-2; n>=0; n-=2) v.append(uint8_t(ID.mid(n, 2).toUInt(&ok, 16)));
        uint8_t crccalc = calcCRC8(v);
        if (crc != crccalc)
        {
            GenMsg(tr("New 1 Wire device wrong CRC") + " " + RomID + " " + tr("device not created"));
            GenMsg(QString("crc = %1").arg(crc));
            GenMsg(QString("crccalc = %1").arg(crccalc));
            return;
        }
        //else GenMsg(tr("New Dallas device CRC Ok") + " " + RomID);
    }
    RomID = newRomID;
    Dev_Data *d = nullptr;
    if ((family == family1820) || (family == family18B20) || (family == family1822) || (family == family1825))
        { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->ui1820 = new Ui::ds1820ui; d->ui1820->setupUi(d->ui);
        connect(d->ui1820->skip85, SIGNAL(stateChanged(int)), this, SLOT(skip85Changed(int)));
    }
    if (family == family2406) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->ui2406 = new Ui::ds2406ui; d->ui2406->setupUi(d->ui); }
    if (family == family2408) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->ui2408 = new Ui::ds2408ui; d->ui2408->setupUi(d->ui);
        connect(d->ui2408->On, SIGNAL(clicked()), this, SLOT(on()));
        connect(d->ui2408->Off, SIGNAL(clicked()), this, SLOT(off()));
        connect(d->ui2408->radioButtonInput, SIGNAL(toggled(bool)), this, SLOT(state2408(bool)));
        connect(d->ui2408->InvertOut, SIGNAL(stateChanged(int)), this, SLOT(read2408(int)));
        connect(d->ui2408->operateDecimal, SIGNAL(stateChanged(int)), this, SLOT(read2408(int)));
        if (!RomID.endsWith("_A")) d->ui2408->operateDecimal->hide(); }
    if ((family == family2413) || (family == family3A2100H)) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->ui2413 = new Ui::ds2413ui; d->ui2413->setupUi(d->ui);
        connect(d->ui2413->On, SIGNAL(clicked()), this, SLOT(on()));
        connect(d->ui2413->Off, SIGNAL(clicked()), this, SLOT(off()));
        connect(d->ui2413->radioButtonInput, SIGNAL(toggled(bool)), this, SLOT(state2413(bool)));
        connect(d->ui2413->InvertOut, SIGNAL(stateChanged(int)), this, SLOT(read2413(int)));
        connect(d->ui2413->operateDecimal, SIGNAL(stateChanged(int)), this, SLOT(read2413(int)));
        if (RomID.endsWith("_B")) d->ui2413->operateDecimal->hide(); }
    if (family == family2423) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->ui2423 = new Ui::ds2423ui; d->ui2423->setupUi(d->ui);
        d->ui2423->counterMode->addItem(tr("Absolute"));
        d->ui2423->counterMode->addItem(tr("Relative"));
        d->ui2423->counterMode->addItem(tr("Offset"));
        for (int n=0; n<lastInterval; n++) d->ui2423->Interval->addItem(getStrMode(n));
        d->ui2423->nextOffsetAdjust->setDisplayFormat("dd-MMM-yyyy  hh:mm");
        d->ui2423->nextOffsetAdjust->setDateTime(QDateTime::currentDateTime());
        QDateTime now = QDateTime::currentDateTime();
        connect(d->ui2423->counterMode, SIGNAL(currentIndexChanged(int)), this, SLOT(modeChanged(int)));
        connect(d->ui2423->countInit, SIGNAL(toggled(bool)), this, SLOT(OffsetModeChanged(bool)));
    }
    if (family == family2438) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->ui2438 = new Ui::ds2438ui; d->ui2438->setupUi(d->ui);
        d->ui2438->IADButton->setText("IAD");
        d->ui2438->IADButton->setToolTip("Current A/D Control Bit. 1 = the current A/D and the ICA are enabled, and current \n\
    measurements will be taken at the rate of 36.41 Hz; 0 = the current A/D and the ICA have been \n\
    disabled. The default value of this bit is a ?1? (current A/D and ICA are enabled).");
        connect(d->ui2438->IADButton, SIGNAL(clicked()), this, SLOT(setConfig2438()));
        d->ui2438->CAButton->setText("CA");
        d->ui2438->CAButton->setToolTip("Current Accumulator Configuration. 1 = CCA/DCA is enabled, and data will be stored and can\n\
    be retrieved from page 7, bytes 4-7; 0 = CCA/DCA is disabled, and page 7 can be used for general\n\
    EEPROM storage. The default value of this bit is a 1 (current CCA/DCA are enabled).");
        connect(d->ui2438->CAButton, SIGNAL(clicked()), this, SLOT(setConfig2438()));
        d->ui2438->EEButton->setText("EE");
        d->ui2438->EEButton->setToolTip("Current Accumulator Shadow Selector bit. 1 = CCA/DCA counter data will be shadowed to\n\
    EEPROM each time the respective register is incremented; 0= CCA/DCA counter data will not be\n\
    shadowed to EEPROM. The CCA/DCA could be lost as the battery pack becomes discharged. If the CA\n\
    bit in the status/configuration register is set to 0, the EE bit will have no effect on the DS2438\n\
    functionality. The default value of this bit is a 1 (current CCA/DCA data shadowed to EEPROM).");
        connect(d->ui2438->EEButton, SIGNAL(clicked()), this, SLOT(setConfig2438()));
        d->ui2438->ADButton->setText("AD");
        d->ui2438->ADButton->setToolTip("Voltage A/D Input Select Bit. 1 = the battery input (VDD) is selected as the input for the\n\
    DS2438 voltage A/D converter; 0 = the general purpose A/D input (VAD) is selected as the voltage\n\
    A/D input. For either setting, a Convert V command will initialize a voltage A/D conversion. The default\n\
    value of this bit is a 1 (VDD is the input to the A/D converter).");
        connect(d->ui2438->ADButton, SIGNAL(clicked()), this, SLOT(setConfig2438()));
    }
    if (family == family2450) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->ui2450 = new Ui::ds2450ui; d->ui2450->setupUi(d->ui); }
    if (family == familyLedOW) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->uiLedOW = new Ui::LedOWui; d->uiLedOW->setupUi(d->ui);
        connect(d->uiLedOW->On, SIGNAL(clicked()), this, SLOT(on()));
        connect(d->uiLedOW->Off, SIGNAL(clicked()), this, SLOT(off()));
        connect(d->uiLedOW->On100, SIGNAL(clicked()), this, SLOT(on100()));
        connect(d->uiLedOW->Dim, SIGNAL(clicked()), this, SLOT(dim()));
        connect(d->uiLedOW->Bright, SIGNAL(clicked()), this, SLOT(bright()));
        connect(d->uiLedOW->Open, SIGNAL(clicked()), this, SLOT(open()));
        connect(d->uiLedOW->Close, SIGNAL(clicked()), this, SLOT(close()));
        connect(d->uiLedOW->SetTTV, SIGNAL(clicked()), this, SLOT(setTTV()));
        connect(d->uiLedOW->SetVolet, SIGNAL(clicked()), this, SLOT(setVolet())); }
    if (family == familyLCDOW) { d = new Dev_Data; dev_Data.append(d); d->RomID = RomID; d->ScratchPad = ""; d->ui = new QWidget(); d->uiLCDOW = new Ui::LCDOWui; d->uiLCDOW->setupUi(d->ui);
        connect(d->uiLCDOW->ButtonWriteValText, SIGNAL(clicked()), this, SLOT(writeValTxt()));
        connect(d->uiLCDOW->ButtonWritePrefix, SIGNAL(clicked()), this, SLOT(writePrefix()));
        connect(d->uiLCDOW->ButtonWriteSuffix, SIGNAL(clicked()), this, SLOT(writeSuffix()));
        connect(d->uiLCDOW->ButtonReadConfig, SIGNAL(clicked()), this, SLOT(ReadLCDConfig()));
        connect(d->uiLCDOW->ButtonWriteConfig, SIGNAL(clicked()), this, SLOT(writeConfig()));
        if (RomID.right(2) == "_A") addtofifo(ReadMemoryLCD, RomID, "FA0000FFFFFFFFFFFFFFFFFF", "");
    }
    if (d) {
        GenMsg("New Device " + RomID);
        emit(newDevice(this, RomID));
        updateDevices(); }
}

void HA7netPlugin::ReadLCDConfig()
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (item) {
        QStringList split = item->text().split(" ");
        QString devRomID = split.first();
        ReadLCDConfig(devRomID, 0, 0); }
}



void HA7netPlugin::OffsetModeChanged(bool)
{
    modeChanged(0);
}


void HA7netPlugin::skip85Changed(int state)
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (item) {
        QStringList split = item->text().split(" ");
        QString devRomID = split.first();
        foreach (Dev_Data *dev, dev_Data) {
            if (dev->RomID == devRomID) {
                if (state) dev->skip85 = true; else dev->skip85 = false; }
        }
    }
}


void HA7netPlugin::modeChanged(Dev_Data *dev)
{
    switch (dev->ui2423->counterMode->currentIndex())
    {
    case absoluteMode :
        dev->ui2423->Offset->setEnabled(false);
        dev->ui2423->countInit->setEnabled(false);
        break;
    case relativeMode :
        dev->ui2423->Offset->setEnabled(true);
        dev->ui2423->Offset->setPrefix("Delta Max : ");
        dev->ui2423->Offset->setToolTip(tr("If different from 0, when delta (before being multiplied by coef) is higher then this value, relative value is reseted"));
        dev->ui2423->countInit->setEnabled(false);
        break;
    case offsetMode  :
        dev->ui2423->Offset->setEnabled(true);
        dev->ui2423->Offset->setPrefix("Offset : ");
        dev->ui2423->Offset->setToolTip("");
        dev->ui2423->countInit->setEnabled(true);
        dev->ui2423->nextOffsetAdjust->setDateTime(setDateTimeS0(QDateTime::currentDateTime()));
        break;
    }
}

void HA7netPlugin::modeChanged(int)
{
    QListWidgetItem *item = mui->devices->currentItem();
    if (!item) return;
    QStringList split = item->text().split(" ");
    QString RomID = split.first();
    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) modeChanged(dev);
    }
}



void HA7netPlugin::ReadLCDConfig(QString RomID, int ID, int address)
{
    //qInfo() << RomID + QString(" read config ID=%1 Address=%2").arg(ID).arg(address);
    //if ((RomID.right(2) == "_A") || (RomID.length() == 16)) {
//FA + ID (1 Byte) + Address(1 Byte) + Data (8 Bytes) + crc(1 Bytes);
        QString hex = readMemoryLCD;
        if (ID == 0) hex += "00";
        if (ID == 1) hex += "01";
        if (ID == 2) hex += "02";
        if (ID == 3) hex += "03";
        QString readRequest = "FFFFFFFFFFFFFFFFFF";        // 8 byte data + 1 byte crc
        hex += QString("%1").arg(uchar(address), 2, 16, QChar('0')) + readRequest;
        addtofifo(ReadMemoryLCD, RomID, hex, "");
}


QDateTime HA7netPlugin::setDateTimeS0(QDateTime T)
{
    return QDateTime(T.date(), QTime(T.time().hour(), T.time().minute(), 0));
}


qint64 HA7netPlugin::getSecs(int index)
{
    qint64 secs = 0;
    switch (index)
    {
        case M1 : secs = 60; break;
        case M2 : secs = 120; break;
        case M5 : secs = 300; break;
        case M10 : secs = 600; break;
        case M15 : secs = 900; break;
        case M20 : secs = 1200; break;
        case M30 : secs = 1800; break;
        case H1 : secs = 3600; break;
        case H2 : secs = 7200; break;
        case H3 : secs = 10800; break;
        case H4 : secs = 14400; break;
        case H6 : secs = 21600; break;
        case H12 : secs = 43200; break;
        case D1 : secs = 86400; break;
        case D2 : secs = 172800; break;
        case D5 : secs = 432000; break;
        case D10 : secs = 864000; break;
        case W1 : secs = 604800; break;
        case W2 : secs = 1209600; break;
    }
        return secs;
}


QString HA7netPlugin::getStrMode(int mode)
{
    QString str;
    switch (mode)
    {
        case M1 : str = tr("1 Minute"); break;
        case M2 : str = tr("2 Minutes"); break;
        case M5 : str = tr("5 Minutes"); break;
        case M10 : str = tr("10 Minutes"); break;
        case M15 : str = tr("15 Minutes"); break;
        case M20 : str = tr("20 Minutes"); break;
        case M30 : str = tr("30 Minutes"); break;
        case H1 : str = tr("1 Hour"); break;
        case H2 : str = tr("2 Hours"); break;
        case H3 : str = tr("3 Hours"); break;
        case H4 : str = tr("4 Hours"); break;
        case H6 : str = tr("6 Hours"); break;
        case H12 : str = tr("12 Hours"); break;
        case D1 : str = tr("1 Day"); break;
        case D2 : str = tr("2 Days"); break;
        case D5 : str = tr("5 Days"); break;
        case D10 : str = tr("10 Days"); break;
        case W1 : str = tr("1 Weeks"); break;
        case W2 : str = tr("2 Weeks"); break;
        case MT : str = tr("1 Month"); break;
        case AUTO : str = tr("Auto"); break;
    default : str = "";
    }
    return str;
}


void HA7netPlugin::updateDevices()
{
    mui->devices->clear();
    foreach (Dev_Data *dev, dev_Data) {
        QListWidgetItem *item = new QListWidgetItem(dev->RomID);
        mui->devices->addItem(item);
    }
}


QString HA7netPlugin::scratchpadOf(QString RomID)
{
    for (int i=0; i<dev_Data.count(); i++) {
        if (dev_Data.at(i)->RomID == RomID) return dev_Data.at(i)->ScratchPad;
    }
    return "";
}


Dev_Data *HA7netPlugin::devOf(QString RomID)
{
    for (int i=0; i<dev_Data.count(); i++) {
        if (dev_Data.at(i)->RomID == RomID) return dev_Data.at(i);
    }
    return nullptr;
}


QString HA7netPlugin::getRomID(const QString &str)
{
    int begin, end;
    QString Str;
    begin = str.indexOf("{");
    end = str.indexOf("}");
    if ((begin != -1) and (end > begin))
        Str = str.mid(begin + 1, end - begin - 1);
    return Str;
}


QString HA7netPlugin::fifoListNext()
{
    if (!currentOrder.isEmpty()) return currentOrder;
    QMutexLocker locker(&mutexFifoList);
    QListWidgetItem *item = nullptr;
    for (int i=0; i<mui->fifolist->count(); i++) {
        if (mui->fifolist->item(i)->text().startsWith("P")) {
            currentOrder = mui->fifolist->item(i)->text().remove(0,1);
            item = mui->fifolist->takeItem(i); }
    }
    if (!item) for (int i=0; i<mui->fifolist->count(); i++) {
        if (mui->fifolist->item(i)->text().startsWith("W")) {
            currentOrder = mui->fifolist->item(i)->text().remove(0,1);
            item = mui->fifolist->takeItem(i); }
    }
    if (!item) for (int i=0; i<mui->fifolist->count(); i++) {
        if (mui->fifolist->item(i)->text().startsWith("R")) {
            currentOrder = mui->fifolist->item(i)->text().remove(0,1);
            item = mui->fifolist->takeItem(i); }
    }
    if (!item) if (mui->fifolist->count() > 0)
    {
        currentOrder = mui->fifolist->item(0)->text();
        item = mui->fifolist->takeItem(0); }
    if (item) delete item;
    return currentOrder;
}


void HA7netPlugin::setrequest(const QString &req)
{
    settraffic(Waitingforanswer);
    QString reqstr = "http://" + ipaddress + QString(":%1").arg(port) + "/1Wire/" + req;
    QUrl url(reqstr);
    startRequest(url);
    GenMsg(reqstr);
}


void HA7netPlugin::settraffic(int state)
{
    switch (state)
    {
        case Disabled :
            mui->traffic->setPixmap(QPixmap(QString::fromUtf8(":/images/images/trafficlight_off.png")));
            mui->traffic->setToolTip(tr("Enabled"));
        break;

        case Waitingforanswer :
            mui->traffic->setPixmap(QPixmap(QString::fromUtf8(":/images/images/trafficlight_yellow.png")));
            mui->traffic->setToolTip(tr("Waiting for answer"));
        break;

        case Connecting :
            mui->traffic->setPixmap(QPixmap(QString::fromUtf8(":/images/images/trafficlight_red_yellow.png")));
            mui->traffic->setToolTip(tr("Waiting connection"));
        break;

        case Disconnected :
            mui->traffic->setPixmap(QPixmap(QString::fromUtf8(":/images/images/trafficlight_red.png")));
            mui->traffic->setToolTip(tr("Disconnected"));
        break;

        case Connected :
            mui->traffic->setPixmap(QPixmap(QString::fromUtf8(":/images/images/trafficlight_green.png")));
            mui->traffic->setToolTip(tr("Connected"));
        break;

        case Paused :
            mui->traffic->setPixmap(QPixmap(QString::fromUtf8(":/images/images/trafficlight_red_yellow.png")));
            mui->traffic->setToolTip(tr("Pausing"));
        break;

        default :
        break;
    }
}


void HA7netPlugin::startRequest(QUrl url)
{
    reply = qnam.get(QNetworkRequest(url));
    connect(reply, SIGNAL(finished()), this, SLOT(httpFinished()));
}


void HA7netPlugin::httpFinished()
{
    QNetworkReply* newreply = reply;
    reply = nullptr;
    busy = false;
    mui->LockEnable->setEnabled(!busy);
    if (newreply->error() != QNetworkReply::NoError)
    {
        GenError(51, newreply->errorString());
        if (httperrorretry < 3)
        {
            httperrorretry ++;
            LockID = "";
            QTimer::singleShot(5000, this, SLOT(fifonext()));
        }
        else
        {
            httperrorretry = 0;
            if (initialsearch == false) fifoListRemoveFirst();
            request = None;
            emit requestdone();
        }
        newreply->deleteLater();
        return;
    }
    else
    {
        httperrorretry = 0;
        if (OnOff) settraffic(Connected);
        QString data_rec = newreply->readAll();
        httpRequestAnalysis(data_rec);
        newreply->deleteLater();
    }
}


void HA7netPlugin::httpRequestAnalysis(const QString &data)
{
    if (data.isEmpty())
    {
        settraffic(Disconnected);
        GenMsg(tr("httpRequestAnalysis : No Data"));
        fifoListRemoveFirst();
        emit requestdone();
        return;
    }
    else if (checkbusshorted(data))
    {
        GenError(56, data);
        TimerPause.start(2000);
    }
    else if (checkLockIDCompliant(data))
    {
        int i = data.indexOf("Lock ID");
        QString str = "Actual Lock ID = " + LockID + "   Refused Lock ID : " + data.mid(i, 33);
        GenError(87, str);
        LockID = "";
        TimerPause.start(5000);
    }
    else
    {
        switch(request)
        {
            case None : simpleend(); break;
            case Reset : simpleend(); break;
            case SkipROM : simpleend(); break;
            case WriteEEprom : simpleend(); break;
            case RecallEEprom : simpleend(); break;
            case WriteScratchpad : simpleend(); break;
            case RecallMemPage00h : simpleend(); break;
            case WriteScratchpad00 : simpleend(); break;
            case CopyScratchpad00 : simpleend(); break;
            case ConvertTemp : convertendtemp(); break;
            case WriteValText	: simpleend(); emit requestdone(); break;
            case WriteLed : endofwriteled(data); emit requestdone(); break;
            case ReadMemoryLCD : endofreadmemorylcd(data); emit requestdone(); break;
            case ConvertTempRomID : convertendtemp(); break;
            case ConvertADC : convertendadc(data); break;
            case WriteMemory : endofwritememory(data); emit requestdone(); break;
            case GetLock : endofgetlock(data); emit requestdone(); break;
            case Search : endofsearchrequest(data); emit requestdone(); break;
            case ReleaseLock : endofreleaselock(data); emit requestdone(); break;
            case ReadPIO : endofreadpoirequest(data); emit requestdone(); break;
            case setChannelOn : endofsetchannel(data); emit requestdone(); break;
            case setChannelOff : endofsetchannel(data); emit requestdone(); break;
            case WritePIO : endofsetchannel(data); emit requestdone(); break;
            case ChannelAccessRead : endofchannelaccessread(data); emit requestdone(); break;
            case ChannelAccessWrite : endofchannelaccesswrite(data); emit requestdone(); break;
            case ReadTemp : endofreadtemprequest(data); emit requestdone();  break;
            case ReadCounter : endofreadcounter(data); emit requestdone(); break;
            case ReadDualSwitch : endofreaddualswitch(data); emit requestdone(); break;
            case ReadADC : endofreadadcrequest(data); emit requestdone();  break;
            case ReadADCPage01h : endofreadadcpage01h(data); emit requestdone();  break;
            case ReadPage : endofreadpage(data); emit requestdone();  break;
            case ReadPage00h : endofreadpage(data); emit requestdone();  break;
            case ReadPage01h : endofreadpage01h(data); emit requestdone();  break;
            default : simpleend();
        }
    }
}


void HA7netPlugin::convertendtemp()
{
    int convTime = getConvertTime(requestRomID);
    converttimer.start(convTime);
}


void HA7netPlugin::endofgetlock(const QString &data)
{
QString strsearch, match;
int i, l, n;
    // NAME="LockID_0" SIZE="10" MAXLENGTH="10" VALUE="668232839"
    strsearch = "NAME=\"LockID_0\" SIZE=\"10\" MAXLENGTH=\"10\" VALUE=\"";
    l = strsearch.length();
    i = data.indexOf(strsearch);
    if (i != -1)
    {
        n = data.indexOf("\"", i + l);
        match =  data.mid(i + l, n - i - l);
        GenMsg("Lock ID = " + match);
        LockID = "LockID=" + match;
        request = None;
        busy = false;
        mui->LockEnable->setEnabled(!busy);
    }
}


void HA7netPlugin::endofreleaselock(const QString&)
{
    GenMsg(LockID + "  Unlocked");
    LockID = "";
    request = None;
    busy = false;
    mui->LockEnable->setEnabled(!busy);
}


void HA7netPlugin::endofreadtemprequest(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE
    QString match;
    QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 54, 18);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
    }
    else GenMsg("html parsing error endofreadtemprequest, RomID : " + requestRomID);
    fifoListRemoveFirst();
}


void HA7netPlugin::endofwriteled(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE
    QString match;
    QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 8);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error endofwriteled RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
    }
    else GenMsg("html parsing error endofwriteled, RomID : " + requestRomID);
    fifoListRemoveFirst();
}


void HA7netPlugin::endofreadmemorylcd(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE
QString match;
QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 24);
        //qDebug() << "match = " + match;
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
    }
    else GenMsg("html parsing error endofreadmemorylcd, RomID : " + requestRomID);
    fifoListRemoveFirst();
}


void HA7netPlugin::convertendadc(const QString &data)
{
QString match;
QByteArray str, strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 10);
        GenMsg(tr("End Convert ADC : ") + match);
    }
    // CRC Check
    int convTime = getConvertTime(requestRomID);
    converttimer.start(convTime);
}


void HA7netPlugin::endofwritememory(const QString&)
{
    request = None;
    requestRomID.clear();
    fifoListRemoveFirst();
}


void HA7netPlugin::endofreadpoirequest(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="
QString match;
QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 26);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofreadpoirequest, RomID : " + requestRomID);
}

void HA7netPlugin::endofsetchannel(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="

QString match;
QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 8).right(2);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error endofsetchannel RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofsetchannel, RomID : " + requestRomID);
}


void HA7netPlugin::endofchannelaccessread(const QString &data)
{
    // search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="
    QString match;
    QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 14);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofchannelaccessread, RomID : " + requestRomID);
}


void HA7netPlugin::endofreadcounter(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE
    QString match;
    QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 28);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofreadcounter, RomID : " + requestRomID);
}


void HA7netPlugin::endofchannelaccesswrite(const QString &data)
{
    // search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="
    QString match;
    QByteArray strsearch;
    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 16);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofchannelaccesswrite, RomID : " + requestRomID);
}


void HA7netPlugin::endofreadadcrequest(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE

QString match;
    QByteArray strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 26);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofreadadcrequest, RomID : " + requestRomID);
}


void HA7netPlugin::endofreaddualswitch(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE
QString match;
    QByteArray strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 52, 4);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error endofreaddualswitch RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofreaddualswitch, RomID : " + requestRomID);
}


void HA7netPlugin::endofreadadcpage01h(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE

QString match;
    QByteArray strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = "page01h=(" + data.mid(i + 52, 26) + ")";
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofreadadcpage01h, RomID : " + requestRomID);
}


void HA7netPlugin::endofreadpage(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE

QString match;
QByteArray strsearch;

    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = data.mid(i + 56, 18);
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        //else GenMsg("setscratchpad RomID : " + requestRomID);
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofreadpage, RomID : " + requestRomID);
}

void HA7netPlugin::endofreadpage01h(const QString &data)
{
// search buffer for    NAME="ResultData_0" SIZE="56" MAXLENGTH="56" VALUE="BE

QString match;
QByteArray strsearch;

    strsearch = "NAME=\"ResultData_0\" SIZE=\"56\" MAXLENGTH=\"56\" VALUE=\"";
    int i = data.indexOf(strsearch);
    if (i != -1)
    {
        match = "page01h=(" + data.mid(i + 56, 18) + ")";
        if (!setscratchpad(requestRomID, match))
        {
            GenMsg("Set scratchpad error RomID : " + requestRomID + "   scratchpad : " + match);
            if (++ requestTry > retrymax) fifoListRemoveFirst();
            return;
        }
        fifoListRemoveFirst();
    }
    else GenMsg("html parsing error endofreadpage01h, RomID : " + requestRomID);
}


bool HA7netPlugin::setscratchpad(QString RomID, QString scratchpad)
{
    if (scratchpad.isEmpty()) {
        GenMsg("scratchpad empty");
        fifoListRemoveFirst();
        return false; }
    QString family = RomID.right(2);
    QString family4 = RomID.right(4);
    if (RomID.contains("_")) family = family4.left(2);
    QString RomID16 = RomID.left(16);
    bool ok = false;
    if (family == family1820) ok = setScratchpad1820(RomID, scratchpad);
    if (family == family18B20) ok = setScratchpad18b20(RomID, scratchpad);
    if (family == family1822) ok = setScratchpad1822(RomID, scratchpad);
    if (family == family1825) ok = setScratchpad1825(RomID, scratchpad);
    if (family == family2406) ok = setScratchpad2406(RomID, scratchpad);
    if (family == family2408) ok = setScratchpad2408(RomID, scratchpad);
    if (family == family2413) ok = setScratchpad2413(RomID, scratchpad);
    if (family == family3A2100H) ok = setScratchpad2413(RomID, scratchpad);
    if (family == family2423) ok = setScratchpad2423(RomID, scratchpad);
    if (family == family2438) ok = setScratchpad2438(RomID, scratchpad);
    if (family == family2450) ok = setScratchpad2450(RomID, scratchpad);
    if (family == familyLedOW) ok = setScratchpadLedOW(RomID, scratchpad);
    if (family == familyLCDOW) ok = setScratchpadLCDOW(RomID16, scratchpad);
    if (ok) GenMsg(("setscratchpad ok : ") + RomID  + " : " + scratchpad);
    else GenMsg(("setscratchpad failed : ") + RomID  + " : " + scratchpad);
    return ok;
}


int HA7netPlugin::getConvertTime(QString RomID)
{
    QString scratchpad = scratchpadOf(RomID);
    if (scratchpad.isEmpty()) return int(mui->ConvertDelay->value() * 1000.0);
    QString family = RomID.right(2);
    if (family == family1820) return 200;
    if ((family == family1822) or (family == family18B20) or (family == family1825))
    {
        bool ok;
        int resolution = scratchpad.mid(8, 2).toInt(&ok, 16);
        if (!ok) return 750;
        int P = resolution & 0x60;
        P /= 32;
        P = (3 - P);
        resolution = 750 / (2 ^ P);
        return resolution;
    }
    return 0;
}


void HA7netPlugin::setValue(Dev_Data *dev, QString scratchpad, double V)
{
    waitAnswer.stop();
    dev->value = V;
    dev->ScratchPad = scratchpad;
    QString Vstr = QString("%1").arg(V, 0, 'f', 3);
    GenMsg(dev->RomID + " = " + Vstr);
    /* La palette clique toutes seul 30/08/2022
     * QListWidgetItem *item = mui->devices->currentItem();
    if (item) {
        QStringList split = item->text().split(" ");
        QString devRomID = split.first();
        if (devRomID == dev->RomID) ShowClick();
    }*/
    for (int n=0; n<mui->devices->count(); n++) {
        if (mui->devices->item(n)->text().startsWith(dev->RomID)) {
            QString Vstr = QString("%1").arg(V, 0, 'f', 3);
            mui->devices->item(n)->setText(dev->RomID + " (" + Vstr + ")");
        }
    }
    emit(newDeviceValue(dev->RomID, Vstr));
}


void HA7netPlugin::setValue(QString RomID, QString scratchpad, double V)
{

    foreach (Dev_Data *dev, dev_Data) {
        if (dev->RomID == RomID) setValue(dev, scratchpad, V); }

}


void HA7netPlugin::setLCDValue(QString RomID, QString scratchpad)
{
    bool ok;
    Dev_Data *dev = devOf(RomID);
    if (!dev) { GenMsg(RomID + " : Unknown RomID "); return; }
    double V = calcultemperatureLCD(scratchpad);
    QString coef = dev->uiLCDOW->Coef->text();
    if (!coef.isEmpty()) {
        double C = coef.toDouble(&ok);
        if (!ok) { GenMsg(RomID + " : Wrong coef conversion " + RomID); return; }
        V *= C; }
    dev->assignedValue = V;
    dev->assignTry = 0;
    setValue(dev, scratchpad, V);
}


void HA7netPlugin::setLCDConfig(QString RomID, int Address, QString Data)
{
    bool ok;
/*  writeMemoryLCD map
    write address 0
        Rien
        FontSize
        XPos
        YPos
        XPosTxt
        YPosTxt
        Digits
        Decimal

    write address 16
        Prefix Bytes 0 - 7
     write address 32
        Suffix Bytes 0 - 7
    write address 40
        ValText Bytes 0 - ...       */
    Dev_Data *dev = devOf(RomID);
    if (!dev) { GenMsg(RomID + " : Unknown RomID (" + RomID); return; }
    if (Address == 0) {
        int v = Data.mid(2,2).toInt(&ok, 16);
        if (ok) {
            dev->uiLCDOW->Fontsize->setValue(v & 0x0F);
            dev->uiLCDOW->TextFontSize->setValue((v & 0xF0) >> 4); }
        v = Data.mid(4,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->XPos->setValue(v);
        v = Data.mid(6,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->YPos->setValue(v);
        v = Data.mid(8,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->XPosTxt->setValue(v);
        v = Data.mid(10,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->YPosTxt->setValue(v);
        v = Data.mid(12,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->Digits->setValue(v);
        v = Data.mid(14,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->Virgule->setValue(v); }
    if (Address == 8) {
        int v = Data.mid(0,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->CoefMult->setValue(v);
        v = Data.mid(2,2).toInt(&ok, 16);
        if (ok) dev->uiLCDOW->CoefDiv->setValue(v); }
    if (Address == 16) {
        dev->uiLCDOW->prefixStr->setText(fromHex(Data)); }
    if (Address == 32) {
        dev->uiLCDOW->suffixStr->setText(fromHex(Data)); }

}



QString HA7netPlugin::fromHex(const QString hexstr)
{
    bool ok;
    QString result;
    int L = hexstr.length() / 2;
    for (int n=0; n<L; n++)
    {
        char ch = char(hexstr.mid(n*2, 2).toInt(&ok, 16));
        if (ok && ch) result.append(ch);
    }
    return result;
}


double HA7netPlugin::calcultemperature1822(const QString &scratchpad)
{
    bool ok;
    bool s = false;
    int T = QString(scratchpad.mid(2,2) + scratchpad.mid(0,2)).toInt(&ok, 16);
    if (T & 0x8000)
    {
        T = 0xFFFF - T + 1;
        s = true;
    }
    if (s) return double(-T) / 16.0;
    else return double(T) / 16.0;
}

double HA7netPlugin::calcultemperature1825(const QString &THex)
{
    bool ok;
    bool s = false;
    /*if (MAX31850.isChecked())
    {
        short int T = THex.toInt(&ok, 16) & 0xFFFC;
        if (T & 0x8000)
        {
            T = 0xFFFF - T + 1;
            s = true;
        }
        if (s) return -(double)(T) / 16;
            else return (double)(T) / 16;
    }*/
    short int T = THex.toInt(&ok, 16);
    if (T & 0x8000)
    {
        T = 0xFFFF - T + 1;
        s = true;
    }
    if (s) return -double(T) / 16;
        else return double(T) / 16;
}

double HA7netPlugin::calculvoltage2438(const QString  &scratchpad)
{
    bool ok;
    QString ADCValue;
    ADCValue = scratchpad.mid(8,2) + scratchpad.mid(6,2);
    double v = double(ADCValue.toInt(&ok, 16)) / 100.0;
    return v;
}


double HA7netPlugin::calculcurrent2438(const QString  &scratchpad)
{
    bool ok;
    QString CURRENTValue;
    CURRENTValue = scratchpad.mid(12,2) + scratchpad.mid(10,2);
    double v = double(CURRENTValue.toInt(&ok, 16));
    return v;
}


double HA7netPlugin::calcultemperature2438(const QString  &scratchpad)
{
    QString THex = scratchpad.mid(4,2) + scratchpad.mid(2,2);
    bool ok;
    bool s = false;
    quint16 T = quint16(THex.toInt(&ok, 16));
    if (T & 0x8000)
    {
            T = 0xFFFF - T + 1;
            s = true;
    }
    if (s) return double(-T) / 256.0;
    else return double(T) / 256.0;
}

double HA7netPlugin::calcultemperature1820(const QString &scratchpad)
{
    bool ok;
    bool s = false;
    int T = QString(scratchpad.mid(2,2) + scratchpad.mid(0,2)).toInt(&ok, 16);
    if (T & 0x8000)
    {
        T = 0xFFFF - T + 1;
        s = true;
    }
    if (s) return double(-T) / 2;
        else return double(T) / 2;
}

double HA7netPlugin::calcultemperatureLCD(const QString &THex)
{
    bool ok;
    bool s = false;
    int T = THex.toInt(&ok, 16);
    if (T & 0x8000)
    {
        T = 0xFFFF - T + 1;
        s = true;
    }
    if (s) return double(-T);
        else return double(T);
}

int HA7netPlugin::getorder(QString &str)
{
    for (int n=0; n<LastRequest; n++)
        if (NetRequestMsg[n] == str) return n;
    return 0;
}

QString HA7netPlugin::getData(const QString &str)
{
    int begin, end;
    QString Str;
    begin = str.indexOf("[");
    end = str.indexOf("]");
    if ((begin != -1) and (end > begin))
        Str = str.mid(begin + 1, end - begin - 1);
    return Str;
}

QString HA7netPlugin::getOrder(const QString &str)
{
    int begin, end;
    QString Str;
    begin = str.indexOf("*");
    end = str.indexOf("#");
    if ((begin != -1) and (end > begin))
        Str = str.mid(begin + 1, end - begin - 1);
    return Str;
}


void HA7netPlugin::GenMsg(const QString str)
{
    log(str);
}


void HA7netPlugin::GenError(int, const QString)
{

}

void HA7netPlugin::Start()
{
    OnOff = true;
    fifonext();
}

void HA7netPlugin::Stop()
{
    OnOff = false;
    settraffic(Disconnected);
}


void HA7netPlugin::Clear()
{
    mui->fifolist->clear();
}


void HA7netPlugin::fifonext()
{
    if (!OnOff) return;
    QString order;
    if (busy) return;
    if (reply)
    {
        QTimer::singleShot(5000, this, SLOT(fifonext()));
        return;
    }
next:
    busy = true;
    mui->LockEnable->setEnabled(!busy);
    if (fifoListEmpty() && currentOrder.isEmpty())
    {
        // if locked unlock 1 wire bus
        if (!LockID.isEmpty())
        {
            request = ReleaseLock;
            QString reqstr = "ReleaseLock.html?" + LockID;
            setrequest(reqstr);
            return;
        }
        busy = false;
        mui->LockEnable->setEnabled(!busy);
        return;		// si le fifo est vide, on quitte
    }
    // if not locked lock 1 wire bus
    if (LockID.isEmpty() and (mui->LockEnable->isChecked()))
    {
        request = GetLock;
        setrequest("GetLock.html");
        return;
    }
    QString next = fifoListNext();
    order = getOrder(next);
    if (order.isEmpty())
    {
        GenMsg(next + " returns order empty");
        fifoListRemoveFirst();
        goto next;
    }
    request = getorder(order);
    if (request == 0)
    {
        fifoListRemoveFirst();
        goto next;
    }
    QString reqRomID = getRomID(next);
    QString Data = getData(next);
    requestRomID = reqRomID;
    QString reqstr;
    switch(request)
    {
        case None :
            simpleend();
            break;

        case Reset :			// commande reste 1 wire bus
            reqRomID = "";
            if (LockID.isEmpty()) reqstr = "Reset.html";
            else reqstr = "Reset.html?" + LockID;
            setrequest(reqstr);
            break;

        case Search :			// commande search 1 wire
             reqRomID = "";

            if (LockID.isEmpty()) reqstr = "Search.html";
            else reqstr = "Search.html?" + LockID;
            setrequest(reqstr);
            break;

        case SkipROM :			// complement commande conversion, skip ROM
            reqRomID = "";
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Data=CC";
            else reqstr = "WriteBlock.html?" + LockID + "&Data=CC";
            setrequest(reqstr);
            break;

        case ReadPIO :
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=F08800FFFFFFFFFFFFFFFFFFFF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=F08800FFFFFFFFFFFFFFFFFFFF";
            setrequest(reqstr);
            break;

        case setChannelOn :
            // http://IP address/1Wire/WriteBlock.html?Address=8D00000085C8C126&Data=5A PIO  /PIO FF
            if (!reqRomID.isEmpty())
            {
                QString data = getOnCommand(reqRomID);
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=5A" + data + "FF";
                else reqstr ="WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=5A" + data + "FF";
                setrequest(reqstr);
            }
            else GenError(28, "setChannelOn command aborted");
            break;

        case setChannelOff :
            // http://IP address/1Wire/WriteBlock.html?Address=8D00000085C8C126&Data=5A PIO /PIO FF
            if (!reqRomID.isEmpty())
            {
                QString data = getOffCommand(reqRomID);
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=5A" + data + "FF";
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=5A" + data + "FF";
                setrequest(reqstr);
            }
            else GenError(28, "setChannelOn command aborted");
            break;

        case ReadDualSwitch :
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=F5FF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=F5FF";
            setrequest(reqstr);
            break;

        case WritePIO :
            // http://IP address/1Wire/WriteBlock.html?Address=8D00000085C8C126&Data=5A PIO /PIO FF
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=5A" + Data + "FF";
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=5A" + Data + "FF";
                setrequest(reqstr);
            break;
        case ChannelAccessRead :
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=F545FFFFFFFFFF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=F545FFFFFFFFFF";
            setrequest(reqstr);
        break;
        case ChannelAccessWrite :
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=F5" + Data + "FFFF";
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=F5" + Data + "FFFF";
                setrequest(reqstr);
        break;
        case ConvertTemp :			// commande conversion
            reqRomID = "";
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Data=CC44";
            else reqstr = "WriteBlock.html?" + LockID + "&Data=CC44";
            setrequest(reqstr);
            break;

        case ConvertTempRomID :			// commande conversion
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=44";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=44";
            setrequest(reqstr);
            break;

        case RecallMemPage00h :
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=B800";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=B800";
            setrequest(reqstr);
            break;

        case RecallMemPage01h :
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=B801";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=B801";
            setrequest(reqstr);
            break;

        case ConvertV :
             if (reqRomID.isEmpty())
             {
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Data=CCB4";
                else reqstr = "WriteBlock.html?" + LockID + "&Data=CCB4";
                setrequest(reqstr);
            }
            else
            {
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=B4";
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=B4";
                setrequest(reqstr);
            }
             break;

        case ConvertADC :			// commande conversion
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=3C0F00FFFF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=3C0F00FFFF";
            setrequest(reqstr);
            break;

        case ReadADC :
            // http://IP address/1Wire/WriteBlock.html?Address=RomID&Data=AA0000FFFFFFFFFFFFFFFFFFFF
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=AA0000FFFFFFFFFFFFFFFFFFFF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=AA0000FFFFFFFFFFFFFFFFFFFF";
            setrequest(reqstr);
            break;

        case ReadADCPage01h :
            // http://IP address/1Wire/WriteBlock.html?Address=RomID&Data=AA0008FFFFFFFFFFFFFFFFFFFF
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=AA0800FFFFFFFFFFFFFFFFFFFF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=AA0800FFFFFFFFFFFFFFFFFFFF";
            setrequest(reqstr);
            break;

        case ReadPage :
        case ReadPage00h :
            // http://IP address/1Wire/WriteBlock.html?Address=8D00000085C8C126&Data=BE00FFFFFFFFFFFFFFFFFF
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=BE00FFFFFFFFFFFFFFFFFF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=BE00FFFFFFFFFFFFFFFFFF";
            setrequest(reqstr);
            break;

        case ReadPage01h :
            // http://IP address/1Wire/WriteBlock.html?Address=8D00000085C8C126&Data=BE00FFFFFFFFFFFFFFFFFF
            if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=BE01FFFFFFFFFFFFFFFFFF";
            else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=BE01FFFFFFFFFFFFFFFFFF";
            setrequest(reqstr);
            break;

            case WriteMemory :
                // http://' + IP addres + '/1Wire/WriteBlock.html?Address=' + RomID + '&Data=55' + DataToWrite);
                // http://192.168.0.250/1Wire/WriteBlock.html?Address=6E00000003925E20&Data=55080000FFFFFF
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=55" + Data;
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=55" + Data;
                setrequest(reqstr);
                break;

            case WriteScratchpad00 :
                // http://' + IP addres + '/1Wire/WriteBlock.html?Address=' + RomID + '&Data4E00=' + DataToWrite);
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=4E00" + Data;
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=4E00" + Data;
                setrequest(reqstr);
                break;

            case CopyScratchpad00 :
                // http://' + IP addres + '/1Wire/WriteBlock.html?Address=' + RomID + '&Data4E00=';
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=4800";
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=4800";
                setrequest(reqstr);
                break;

            case WriteValText :
                // http://' + IP addres + '/1Wire/WriteBlock.html?Address=' + RomID + '&Data=' + DataToWrite);
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=" + Data;
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=" + Data;
                setrequest(reqstr);
                break;

            case WriteLed :
                // http://' + IP addres + '/1Wire/WriteBlock.html?Address=' + RomID + '&Data=' + DataToWrite);
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=" + Data;
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=" + Data;
                setrequest(reqstr);
                break;

            case ReadMemoryLCD :
                // http://' + IP addres + '/1Wire/WriteBlock.html?Address=' + RomID + '&Data=' + DataToWrite);
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=" + Data;
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=" + Data;
                setrequest(reqstr);
                break;

            case ReadTemp :
                // http://IP address/1Wire/WriteBlock.html?Address=RomID&Data=BEFFFFFFFFFFFFFFFFFF'
                if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=BEFFFFFFFFFFFFFFFFFF";
                else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=BEFFFFFFFFFFFFFFFFFF";
                setrequest(reqstr);
                break;

            case ReadCounter :
                // http://IP address/1Wire/WriteBlock.html?Address=RomID&Data=A5DF01FFFFFFFFFFFFFFFFFFFFFF'
                if (LockID.isEmpty())
                {
                    if ((reqRomID.right(4) == family2423_A) or (reqRomID.right(2) == familyLCDOW))
                    {
                        reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=A5DF01FFFFFFFFFFFFFFFFFFFFFF";
                        setrequest(reqstr);
                    }
                    else if (reqRomID.right(4) == family2423_B)
                    {
                        reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=A5FF01FFFFFFFFFFFFFFFFFFFFFF";
                        setrequest(reqstr);
                    }
                    else simpleend();
                }
                else
                {
                    if ((reqRomID.right(4) == family2423_A) or (reqRomID.right(2) == familyLCDOW))
                    {
                        reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=A5DF01FFFFFFFFFFFFFFFFFFFFFF";
                        setrequest(reqstr);
                    }
                    else if (reqRomID.right(4) == family2423_B)
                    {
                        reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=A5FF01FFFFFFFFFFFFFFFFFFFFFF";
                        setrequest(reqstr);
                    }
                    else simpleend();
                }
                break;

            case WriteScratchpad :
                // http://' + IP addres + '/1Wire/WriteBlock.html?Address=' + RomID + '&Data=4E' + DataToWrite);
                    if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=4E" + Data;
                    else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=4E" + Data;
                    setrequest(reqstr);
                break;

            case WriteEEprom :
                // http://IP address/1Wire/WriteBlock.html?Address=RomID&Data=48'
                    if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=48";
                    else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=48";
                    setrequest(reqstr);
                break;

            case RecallEEprom :
                // http://IP address/1Wire/WriteBlock.html?Address=RomID&Data=B8'
                    if (LockID.isEmpty()) reqstr = "WriteBlock.html?Address=" + reqRomID.left(16) + "&Data=B8";
                    else reqstr = "WriteBlock.html?" + LockID + "&Address=" + reqRomID.left(16) + "&Data=B8";
                    setrequest(reqstr);
                break;

            default : simpleend();
    }
}



void HA7netPlugin::simpleend()
{
    request = None;
    fifoListRemoveFirst();
    if (((request != ConvertTemp) or (request != ConvertADC) or (request != ConvertV) or (request != ConvertTempRomID)) and (request != NetError)) emit requestdone();
}


uint8_t HA7netPlugin::calcCRC8(QVector <uint8_t> s)
{
    uint8_t crc = 0;
    foreach (uint8_t t, s) crc = crc8Table[(crc ^ t) & 0xff];
    return crc;
}


bool HA7netPlugin::checkCRC8(QString scratchpad)
{
    bool ok;
    QVector <uint8_t> v;
    for (int n=0; n<scratchpad.length(); n+=2) v.append(uint8_t(scratchpad.mid(n, 2).toUInt(&ok, 16)));
    if (calcCRC8(v)) return false;
    return true;
}


bool HA7netPlugin::checkCRC16(QString scratchpad)
{
    bool ok;
    //QString errorMsg = scratchpad + "   RomID = " + romid + "  " + name + "  ";
    QString CrcStr = scratchpad.right(4);
    QString Crc = CrcStr.right(2) + CrcStr.left(2);
    ulong L = ulong((scratchpad.length() / 2) - 2);
    if (L <= 0)
    {
        //if (master && WarnEnabled.isChecked()) master->GenError(26, errorMsg);
        //return IncWarning();
    }
    uint16_t crc = uint16_t(Crc.toUInt(&ok, 16));
    if (!ok)
    {
        //if (master && WarnEnabled.isChecked()) master->GenError(53, errorMsg);
        //return IncWarning();
    }
    QVector <uint8_t> v;
    for (int n=0; n<scratchpad.length()-4; n+=2) v.append(uint8_t(scratchpad.mid(n, 2).toUInt(&ok, 16)));
    uint16_t crccalc = calcCRC16(v);
    if (crc == crccalc)
    {
        //logMsg("CRC ok");
        return true;
    }
    //if (master && WarnEnabled.isChecked()) master->GenError(53, errorMsg);
    //qDebug() << "bad CRC16";
    return false;
}
uint16_t HA7netPlugin::calcCRC16(QVector <uint8_t> s)
{
    uint16_t crc = 0;
    foreach (uint8_t t, s)
        crc = crc16Table[(crc ^ t) & 0xff] ^ (crc>>8);
    crc ^= 0xffff;  // Final Xor Value 0xffff
    return crc;
}
const uint8_t HA7netPlugin::crc8Table[256] = {
0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53,};
const uint16_t HA7netPlugin::crc16Table[256] = {
0x0000,  0xc0c1,  0xc181,  0x0140,  0xc301,  0x03c0,  0x0280,  0xc241,
0xc601,  0x06c0,  0x0780,  0xc741,  0x0500,  0xc5c1,  0xc481,  0x0440,
0xcc01,  0x0cc0,  0x0d80,  0xcd41,  0x0f00,  0xcfc1,  0xce81,  0x0e40,
0x0a00,  0xcac1,  0xcb81,  0x0b40,  0xc901,  0x09c0,  0x0880,  0xc841,
0xd801,  0x18c0,  0x1980,  0xd941,  0x1b00,  0xdbc1,  0xda81,  0x1a40,
0x1e00,  0xdec1,  0xdf81,  0x1f40,  0xdd01,  0x1dc0,  0x1c80,  0xdc41,
0x1400,  0xd4c1,  0xd581,  0x1540,  0xd701,  0x17c0,  0x1680,  0xd641,
0xd201,  0x12c0,  0x1380,  0xd341,  0x1100,  0xd1c1,  0xd081,  0x1040,
0xf001,  0x30c0,  0x3180,  0xf141,  0x3300,  0xf3c1,  0xf281,  0x3240,
0x3600,  0xf6c1,  0xf781,  0x3740,  0xf501,  0x35c0,  0x3480,  0xf441,
0x3c00,  0xfcc1,  0xfd81,  0x3d40,  0xff01,  0x3fc0,  0x3e80,  0xfe41,
0xfa01,  0x3ac0,  0x3b80,  0xfb41,  0x3900,  0xf9c1,  0xf881,  0x3840,
0x2800,  0xe8c1,  0xe981,  0x2940,  0xeb01,  0x2bc0,  0x2a80,  0xea41,
0xee01,  0x2ec0,  0x2f80,  0xef41,  0x2d00,  0xedc1,  0xec81,  0x2c40,
0xe401,  0x24c0,  0x2580,  0xe541,  0x2700,  0xe7c1,  0xe681,  0x2640,
0x2200,  0xe2c1,  0xe381,  0x2340,  0xe101,  0x21c0,  0x2080,  0xe041,
0xa001,  0x60c0,  0x6180,  0xa141,  0x6300,  0xa3c1,  0xa281,  0x6240,
0x6600,  0xa6c1,  0xa781,  0x6740,  0xa501,  0x65c0,  0x6480,  0xa441,
0x6c00,  0xacc1,  0xad81,  0x6d40,  0xaf01,  0x6fc0,  0x6e80,  0xae41,
0xaa01,  0x6ac0,  0x6b80,  0xab41,  0x6900,  0xa9c1,  0xa881,  0x6840,
0x7800,  0xb8c1,  0xb981,  0x7940,  0xbb01,  0x7bc0,  0x7a80,  0xba41,
0xbe01,  0x7ec0,  0x7f80,  0xbf41,  0x7d00,  0xbdc1,  0xbc81,  0x7c40,
0xb401,  0x74c0,  0x7580,  0xb541,  0x7700,  0xb7c1,  0xb681,  0x7640,
0x7200,  0xb2c1,  0xb381,  0x7340,  0xb101,  0x71c0,  0x7080,  0xb041,
0x5000,  0x90c1,  0x9181,  0x5140,  0x9301,  0x53c0,  0x5280,  0x9241,
0x9601,  0x56c0,  0x5780,  0x9741,  0x5500,  0x95c1,  0x9481,  0x5440,
0x9c01,  0x5cc0,  0x5d80,  0x9d41,  0x5f00,  0x9fc1,  0x9e81,  0x5e40,
0x5a00,  0x9ac1,  0x9b81,  0x5b40,  0x9901,  0x59c0,  0x5880,  0x9841,
0x8801,  0x48c0,  0x4980,  0x8941,  0x4b00,  0x8bc1,  0x8a81,  0x4a40,
0x4e00,  0x8ec1,  0x8f81,  0x4f40,  0x8d01,  0x4dc0,  0x4c80,  0x8c41,
0x4400,  0x84c1,  0x8581,  0x4540,  0x8701,  0x47c0,  0x4680,  0x8641,
0x8201,  0x42c0,  0x4380,  0x8341,  0x4100,  0x81c1,  0x8081,  0x4040};



