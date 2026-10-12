include(../config/staticlib.pri)
include(../config/ffmpeg.pri)

TARGET = qmmedia
QT    += core multimedia

HEADERS += \
    AudioOutput.h \
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
