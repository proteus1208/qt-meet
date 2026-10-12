# FFmpeg (LGPL, shared build). FFMPEG_DIR = install prefix containing include/, lib/, bin/.

FFMPEG_DIR = $$(FFMPEG_DIR)
win32:isEmpty(FFMPEG_DIR): FFMPEG_DIR = D:/Apps/FFmpeg_9_Mingw/install_mingw

FFMPEG_LIBS = avformat avcodec avdevice avfilter swscale swresample avutil

isEmpty(FFMPEG_DIR) {
    unix: CONFIG += link_pkgconfig
    unix: PKGCONFIG += libavformat libavcodec libavdevice libavfilter libswscale libswresample libavutil
} else {
    !exists($$FFMPEG_DIR/include/libavcodec/avcodec.h): warning("FFmpeg headers not found under $$FFMPEG_DIR")
    INCLUDEPATH += $$FFMPEG_DIR/include

    !contains(TEMPLATE, lib) {
        LIBS += -L$$FFMPEG_DIR/lib
        for(lib, FFMPEG_LIBS): LIBS += -l$$lib

        # Copy the FFmpeg DLLs next to the executable so it runs from Qt Creator.
        win32:!isEmpty(DESTDIR) {
            FFMPEG_DLLS = $$files($$FFMPEG_DIR/bin/*.dll)
            for(dll, FFMPEG_DLLS) {
                QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$shell_path($$dll)) $$shell_quote($$shell_path($$DESTDIR)) $$escape_expand(\\n\\t)
            }
        }
    }
}
