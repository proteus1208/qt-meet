include(../config/staticlib.pri)
include(../config/protobuf.pri)

TARGET = qmcommon
QT    += core network

HEADERS += \
    AppConfig.h \
    EnvelopeFactory.h \
    FrameCodec.h \
    Ids.h \
    Logger.h \
    ResourceMonitor.h

SOURCES += \
    AppConfig.cpp \
    EnvelopeFactory.cpp \
    FrameCodec.cpp \
    Ids.cpp \
    Logger.cpp \
    ResourceMonitor.cpp
