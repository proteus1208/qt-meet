# libdatachannel (WebRTC transport).
# Override with env DATACHANNEL_DIR = prefix containing include/ and lib/.

DATACHANNEL_DIR = $$(DATACHANNEL_DIR)

# Windows defaults (same pattern as PROTOBUF_DIR / FFMPEG_DIR). First existing wins.
win32:isEmpty(DATACHANNEL_DIR) {
    exists(D:/libs/libdatachannel/include/rtc): DATACHANNEL_DIR = D:/libs/libdatachannel
    else: exists(D:/libdatachannel/include/rtc): DATACHANNEL_DIR = D:/libdatachannel
    else: exists(D:/Apps/libdatachannel/include/rtc): DATACHANNEL_DIR = D:/Apps/libdatachannel
    else: exists(C:/libs/libdatachannel-qtmingw/include/rtc): DATACHANNEL_DIR = C:/libs/libdatachannel-qtmingw
    else: exists(C:/libs/libdatachannel-mingw64/include/rtc): DATACHANNEL_DIR = C:/libs/libdatachannel-mingw64
}

isEmpty(DATACHANNEL_DIR) {
    warning("DATACHANNEL_DIR is not set; libdatachannel headers/libs will not be found")
} else {
    !exists($$DATACHANNEL_DIR/include/rtc) {
        warning("DATACHANNEL_DIR=$$DATACHANNEL_DIR has no include/rtc — WebRTC disabled")
        DATACHANNEL_DIR =
    }
}

isEmpty(DATACHANNEL_DIR) {
    # still empty after validation
} else {
    message("libdatachannel: $$DATACHANNEL_DIR")
    DEFINES += QM_HAVE_DATACHANNEL
    INCLUDEPATH += $$DATACHANNEL_DIR/include
    !contains(TEMPLATE, lib) {
        # Shared libdatachannel: link only the import lib. Do NOT add MSYS2
        # -L.../mingw64|ucrt64/lib for OpenSSL — that pulls a foreign CRT into
        # Qt MinGW links (_gnu_exception_handler / __emutls). Copy libssl/libcrypto
        # DLLs next to the exe at runtime instead.
        LIBS += -L$$DATACHANNEL_DIR/lib -ldatachannel
        unix: LIBS += -lssl -lcrypto
        win32: LIBS += -lws2_32 -lbcrypt
        win32:!isEmpty(DESTDIR) {
            QMAKE_POST_LINK += $$quote(cmd /c "if exist $$shell_quote($$shell_path($$DATACHANNEL_DIR/bin/libdatachannel.dll)) copy /y $$shell_quote($$shell_path($$DATACHANNEL_DIR/bin/libdatachannel.dll)) $$shell_quote($$shell_path($$DESTDIR)\\) >nul & exit /b 0") $$escape_expand(\\n\\t)
            QMAKE_POST_LINK += $$quote(cmd /c "if exist C:\\msys64\\mingw64\\bin\\libssl-3-x64.dll copy /y C:\\msys64\\mingw64\\bin\\libssl-3-x64.dll $$shell_quote($$shell_path($$DESTDIR)\\) >nul & exit /b 0") $$escape_expand(\\n\\t)
            QMAKE_POST_LINK += $$quote(cmd /c "if exist C:\\msys64\\mingw64\\bin\\libcrypto-3-x64.dll copy /y C:\\msys64\\mingw64\\bin\\libcrypto-3-x64.dll $$shell_quote($$shell_path($$DESTDIR)\\) >nul & exit /b 0") $$escape_expand(\\n\\t)
        }
    }
}
