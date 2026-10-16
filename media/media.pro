include(../config/staticlib.pri)
include(../config/ffmpeg.pri)

TARGET = qmmedia
QT    += core gui multimedia

# FFmpeg C APIs need these on some toolchains.
DEFINES += __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS

HEADERS += \
    AudioOutput.h \
    BandwidthProfile.h \
    CaptureSource.h \
    Decoder.h \
    Encoder.h \
    FrameConverter.h \
    HwDetector.h \
    MediaClock.h \
    MediaPlayer.h \
    MediaSource.h \
    MediaTypes.h \
    Muxer.h \
    PacketSink.h \
    Resampler.h \
    Scaler.h

SOURCES += \
    AudioOutput.cpp \
    BandwidthProfile.cpp \
    CaptureSource.cpp \
    Decoder.cpp \
    Encoder.cpp \
    FrameConverter.cpp \
    HwDetector.cpp \
    MediaClock.cpp \
    MediaPlayer.cpp \
    MediaSource.cpp \
    MediaTypes.cpp \
    Muxer.cpp \
    Resampler.cpp \
    Scaler.cpp
