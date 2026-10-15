TEMPLATE = subdirs

SUBDIRS = \
    proto \
    common \
    media \
    rtc \
    server \
    client

common.depends = proto
media.depends  = common
rtc.depends    = common
server.depends = proto common rtc
client.depends = proto common media rtc

OTHER_FILES += \
    config/common.pri \
    config/staticlib.pri \
    config/libs.pri \
    config/protobuf.pri \
    config/ffmpeg.pri \
    config/datachannel.pri \
    config/opus.pri \
    config/apm.pri
