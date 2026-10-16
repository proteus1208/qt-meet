include(../config/staticlib.pri)
include(../config/protobuf.pri)

TARGET = qmcommon
QT    += core network gui widgets quick

HEADERS += \
    AppConfig.h \
    DesktopShell.h \
    EnvelopeFactory.h \
    FrameCodec.h \
    Ids.h \
    Logger.h \
    ResourceMonitor.h \
    SingleInstance.h

SOURCES += \
    AppConfig.cpp \
    DesktopShell.cpp \
    EnvelopeFactory.cpp \
    FrameCodec.cpp \
    Ids.cpp \
    Logger.cpp \
    ResourceMonitor.cpp \
    SingleInstance.cpp
