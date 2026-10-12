# FFmpeg (LGPL build). FFMPEG_DIR = prefix containing include/ and lib/.

FFMPEG_DIR = $$(FFMPEG_DIR)

isEmpty(FFMPEG_DIR) {
    unix: CONFIG += link_pkgconfig
    unix: PKGCONFIG += libavformat libavcodec libavdevice libavfilter libswscale libswresample libavutil
    win32: warning("FFMPEG_DIR is not set; FFmpeg headers/libs will not be found")
} else {
    INCLUDEPATH += $$FFMPEG_DIR/include
    !contains(TEMPLATE, lib) {
        LIBS += -L$$FFMPEG_DIR/lib \
                -lavformat -lavcodec -lavdevice -lavfilter -lswscale -lswresample -lavutil
    }
}
