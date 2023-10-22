TEMPLATE        = lib
CONFIG         += plugin
QT             += widgets
HEADERS         = HA7NetPlugin.h
SOURCES         = HA7NetPlugin.cpp
TARGET          = HA7NetPlugin
QT += network
FORMS += HA7Net.ui \
    ds1820.ui \
    LCDOW.ui \
    LedOW.ui \
    ds2406.ui \
    ds2408.ui \
    ds2413.ui \
    ds2423.ui \
    ds2438.ui \
    ds2450.ui
RESOURCES += HA7NetPlugin.qrc

