#ifndef HA7NETPLUGIN_H
#define HA7NETPLUGIN_H

#include <QObject>
#include <QtPlugin>
#include <QTabWidget>
#include <QtNetwork/QtNetwork>
#include <QListWidgetItem>
#include "ui_HA7Net.h"
#include "ui_ds1820.h"
#include "ui_ds2406.h"
#include "ui_ds2408.h"
#include "ui_ds2413.h"
#include "ui_ds2423.h"
#include "ui_ds2438.h"
#include "ui_ds2450.h"
#include "ui_LedOW.h"
#include "ui_LCDOW.h"


#include "../interface.h"

enum counterMode { absoluteMode, relativeMode, offsetMode };

enum mode { M1, M2, M5, M10, M15, M20, M30, H1, H2, H3, H4, H6, H12, D1, D2, D5, D10, W1, W2, MT, AUTO, lastInterval };

enum NetRequest
    {
        None, GetLock, ReleaseLock, NetError, Reset, Search, MatchRom, SkipROM, ConvertTemp, ConvertTempRomID,
        ReadTemp, ReadDualSwitch, ReadDualAdrSwitch, WriteScratchpad, WriteScratchpad00, CopyScratchpad00, SendCmd,
        ReadCounter, ReadLCD, ReadPIO, ChannelAccessRead, ChannelAccessWrite, setChannelOn, setChannelOff, WritePIO,
        ConvertADC, ReadADC, ReadADCPage01h, ConvertV, ReadPage, ReadPage00h, ReadPage01h , WriteMemory, WriteMemoryLCD, WriteValText, ReadMemoryLCD, WriteLed,
        RecallMemPage00h, RecallMemPage01h, RecallEEprom, WriteEEprom, LastRequest
    };

static const QString NetRequestMsg[LastRequest] =
    {	"None", "GetLock", "ReleaseLock", "Error", "Reset", "Search", "MatchRom", "SkipROM", "CVT", "ConvertTempRomID",
        "ReadTemp", "ReadDualSwitch", "ReadDualAdrSwitch", "WriteScratchpad", "WriteScratchpad00", "CopyScratchpad00", "SendCmd",
        "ReadCounter", "ReadLCD", "ReadPIO",  "ChannelAccessRead", "ChannelAccessWrite", "setChannelOn", "setChannelOff", "WritePIO",
        "ConvertADC", "ReadADC", "ReadADCPage01h", "ConvertV", "ReadPage", "ReadPage00h", "ReadPage01h", "WriteMemory", "WriteMemoryLCD", "WriteValText", "ReadMemoryLCD", "WriteLed",
        "RecallMemPage00h", "RecallMemPage01h", "RecallEEprom", "WriteEEprom"
    };

enum NetTraffic
{
    Disabled, Connecting, Waitingforanswer, Disconnected, Connected, Paused
};

struct Dquint64
{
    quint64 value = 0;
    bool valid = false;
};

struct Dev_Data
{
    QString RomID;
    QString ScratchPad;
    QString Command;
    double value = 0;
    QList <double> check85Values;
    bool skip85 = false;
    double assignedValue = 0;
    bool assignTry = 0;
    bool isOutput = false;
    bool manual = false, lastManual = false;
    quint64 count = 0;
    Dquint64 lastCounter;
    quint64 Delta;
    QWidget *ui = nullptr;
    Ui::ds1820ui *ui1820 = nullptr;
    Ui::ds2406ui *ui2406 = nullptr;
    Ui::ds2408ui *ui2408 = nullptr;
    Ui::ds2413ui *ui2413 = nullptr;
    Ui::ds2423ui *ui2423 = nullptr;
    Ui::ds2438ui *ui2438 = nullptr;
    Ui::ds2450ui *ui2450 = nullptr;
    Ui::LedOWui *uiLedOW = nullptr;
    Ui::LCDOWui *uiLCDOW = nullptr;
};

#define fifomax 1000
#define retrymax 5
#define readMemoryLCD "FA"
#define writeMemoryLCD "F5"
#define writePIO "5A"
#define readMemoryLCD "FA"
#define ADDR_Suffix 32
#define ADDR_Prefix 16

class HA7netPlugin : public QWidget, LogisDomInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "logisdom.network.LogisDomInterface/1.0" FILE "HA7NetPlugin.json")
    Q_INTERFACES(LogisDomInterface)
public:
    HA7netPlugin( QWidget *parent = nullptr );
    ~HA7netPlugin() override;
    QObject* getObject() override;
    QWidget *widgetUi() override;
    QWidget *getDevWidget(QString RomID) override;
    void setConfigFileName(const QString) override;
    void readDevice(const QString &device) override;
    QString getDeviceCommands(const QString &device) override;
    void saveConfig() override;
    void readConfig() override;
    void setLockedState(bool) override;
    QString getDeviceConfig(QString) override;
    void setDeviceConfig(const QString&, const QString&) override;
    QString getName() override;
    void setStatus(const QString) override;
    bool acceptCommand(const QString) override;
    bool isDimmable(const QString) override;
    bool isManual(const QString) override;
    double getMaxValue(const QString) override;
signals:
    void newDeviceValue(QString, QString) override;
    void newDevice(LogisDomInterface*, QString) override;
    void deviceSelected(QString) override;
    void updateInterfaceName(LogisDomInterface*, QString) override;
    void connectionStatus(QAbstractSocket::SocketState) override;
private:
    QTabWidget *interfaces;
    QWidget *ui, *lastW = nullptr;
    QString configFileName;
    void saveSetup();
    void loadSetup();
    Ui::HA7NetUI *mui;
    bool initialsearch = true;
    bool busy = false;
    QString LockID;
    int request = -1;
    int requestTry = 0;
    void readDevice(const Dev_Data *dev);
    QNetworkReply* reply;
    QNetworkAccessManager qnam;
    QTimer converttimer, TimerPause;
    void setConfig(const QString &strsearch);
    bool checkbusshorted(const QString &data);
    bool checkLockIDCompliant(const QString &data);
    void addtofifo(int order);
    void addtofifo(int order, const QString &RomID, QString priority);
    void addtofifo(int order, const QString &RomID, const QString &Data, QString priority);
    QTimer TimerReqDone;
    QString currentOrder;
    bool OnOff = true;
    void setipaddress(const QString &adr);
    QString ipaddress;
    void setport(const QString &adr);
    quint16 port = 80;
    int fifoListCount();
    QMutex mutex, mutexFifoList, readDev;
    QString getFifoString(const QString Order, const QString RomID, const QString Data);
    bool fifoListContains(const QString &str);
    void fifoListAdd(const QString &order);
    void convert();
    void endofsearchrequest(const QString &data);
    void fifoListRemoveFirst();
    bool fifoListEmpty();
    QString fifoListNext();
    void setrequest(const QString &req);
    int getorder(QString &str);
    QString getOrder(const QString &str);
    void GenMsg(const QString);
    void GenError(int error, const QString);
    QString getData(const QString &str);
    static QString getRomID(const QString &str);
    QString requestRomID;
    QList <Dev_Data*> dev_Data;
    QString scratchpadOf(QString);
    Dev_Data *devOf(QString);
    void simpleend();
    void createNewDevice(const QString &RomID);
    void getDeviceList(const QString RomID, QStringList &DevList);
    void createDevice(QString RomID);
    void startRequest(QUrl url);
    int httperrorretry = 0;
    void httpRequestAnalysis(const QString &data);
    void log(QString);
    QString logStr;
    QString lastStatus;
    QTimer waitAnswer;
    void ReadLCDConfig(QString, int, int);
    int getConvertTime(QString RomID);
    QString fromHex(const QString hexstr);
    QString toHex(QByteArray data);
    qint64 getSecs(int);
    QString getStrMode(int mode);
    QDateTime setDateTimeS0(QDateTime);
    static uint8_t calcCRC8(QVector <uint8_t> s);
    static uint16_t calcCRC16(QVector <uint8_t> s);
    bool checkCRC8(QString);
    bool checkCRC16(QString);
    void addCRC8(QString &hex);
    static const uint8_t  crc8Table[256];   /**< Preinitialize CRC-8 table. */
    static const uint16_t crc16Table[256];  /**< Preinitialize CRC-16 table. */
    void endofreadtemprequest(const QString &data);
    void endofwriteled(const QString &data);
    void endofreadmemorylcd(const QString &data);
    void convertendadc(const QString &data);
    void endofwritememory(const QString&);
    void endofreadpoirequest(const QString &data);
    void endofsetchannel(const QString &data);
    void endofchannelaccessread(const QString &data);
    void endofchannelaccesswrite(const QString &data);
    void endofreadcounter(const QString &data);
    void endofreaddualswitch(const QString &data);
    void endofreadadcrequest(const QString &data);
    void endofreadadcpage01h(const QString &data);
    void endofreadpage(const QString &data);
    void endofreadpage01h(const QString &data);
    bool setscratchpad(QString RomID, QString scratchpad);
    void endofgetlock(const QString &);
    void endofreleaselock(const QString&);
    double calcultemperature1820(const QString &);
    double calcultemperature1822(const QString &);
    double calcultemperature1825(const QString &);
    double calcultemperature2438(const QString  &);
    double calculvoltage2438(const QString  &scratchpad);
    double calculcurrent2438(const QString  &scratchpad);
    double calcultemperaturealarmeH(const QString &);
    double calcultemperaturealarmeB(const QString &);
    double calcultemperatureLCD(const QString &);
    bool check85(QString RomID, double V);
    bool AreSame(double a, double b);
    int getresolution(QString scratch);
    void setValue(QString, QString, double);
    void setValue(Dev_Data *, QString, double);
    void setLCDValue(QString RomID, QString scratchpad);
    void setLCDConfig(QString, int , QString);
    void display1820(QString, QString);
    void display18b20(QString, QString);
    void display1822(QString, QString);
    void display1825(QString, QString);
    void display2406(QString, QString);
    void display2408(QString, QString);
    void display2413(QString, QString);
    void display2423(QString, QString);
    void display2438(QString, QString);
    void display2450(QString, QString);
    void displayLedOW(QString RomID, QString scratchpad);
    void displayLCDOW(QString RomID, QString scratchpad);

    bool setScratchpad1820(QString, QString);
    bool setScratchpad18b20(QString, QString);
    bool setScratchpad1822(QString, QString);
    bool setScratchpad1825(QString, QString);
    bool setScratchpad2406(QString, QString);
    bool setScratchpad2408(QString, QString);
    bool setScratchpad2413(QString, QString);
    bool setScratchpad2423(QString, QString);
    bool setScratchpad2438(QString, QString);
    bool setScratchpad2450(QString, QString);
    bool setScratchpadLedOW(QString RomID, QString scratchpad);
    bool setScratchpadLCDOW(QString RomID, QString scratchpad);
    void ShowClick(QListWidgetItem *item);
    QString getOffCommand(QString);
    QString getOnCommand(QString);
    void on(QString);
    void on100(QString);
    void off(QString);
    void dim(QString);
    void bright(QString);
    void open(QString);
    void close(QString);
    void setTTV(QString);
    void setVolet(QString);
    void setValue(QString RomID, double v);
    void modeChanged(Dev_Data *dev);
private slots:
    void Start();
    void Stop();
    void Clear();
    void fifonext();
    void httpFinished();
    void Ha7Netconfig();
    void timerconvertout();
    void noRequest();
    void convertSlot();
    void IPEdited(QString);
    void PortEdited(QString);
    void logEnable(int);
    void emitReqDone();
    void settraffic(int);
    void convertendtemp();
    void updateDevices();
    void SearchClick();
    void ReadClick();
    void ReadClick(QListWidgetItem *);
    void ReadLCDConfig();
    void ShowClick();
    void on();
    void on100();
    void off();
    void dim();
    void bright();
    void open();
    void close();
    void setTTV();
    void setVolet();
    void setConfig2438();
    void writeValTxt();
    void writePrefix();
    void writeSuffix();
    void writeConfig();
    void read2408(int);
    void read2413(int);
    void state2408(bool);
    void state2413(bool);
    void modeChanged(int);
    void OffsetModeChanged(bool);
    void skip85Changed(int);
signals:
    void requestdone();
};

#endif


