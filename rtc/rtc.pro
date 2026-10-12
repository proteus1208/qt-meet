include(../config/staticlib.pri)
include(../config/protobuf.pri)
include(../config/datachannel.pri)
include(../config/apm.pri)

TARGET = qmrtc
QT    += core network

HEADERS += \
    AudioProcessor.h \
    JitterBuffer.h \
    RtcConfig.h \
    RtcPeer.h \
    RtpDepacketizer.h \
    RtpPacketizer.h \
    RtpUtil.h

SOURCES += \
    AudioProcessor.cpp \
    JitterBuffer.cpp \
    RtcPeer.cpp \
    RtpDepacketizer.cpp \
    RtpPacketizer.cpp \
    RtpUtil.cpp
