# libdatachannel (WebRTC transport).
# Override with env DATACHANNEL_DIR = MinGW prefix (include/ + lib/ + bin/).
#
# Qt MinGW cannot load UCRT-built DLLs (error: nanosleep64).
# Reject known UCRT install paths. Prefer *-mingw64 / *-qtmingw only.

DATACHANNEL_DIR = $$(DATACHANNEL_DIR)

# Strip accidental UCRT prefixes even if set in the environment.
win32:!isEmpty(DATACHANNEL_DIR) {
    DATACHANNEL_DIR_NORM = $$replace(DATACHANNEL_DIR, \\\\, /)
    contains(DATACHANNEL_DIR_NORM, .*/libs/libdatachannel$)|equals(DATACHANNEL_DIR_NORM, C:/libs/libdatachannel)|equals(DATACHANNEL_DIR_NORM, D:/libs/libdatachannel)|equals(DATACHANNEL_DIR_NORM, C:/libs/libdatachannel/)|equals(DATACHANNEL_DIR_NORM, D:/libs/libdatachannel/) {
        warning("DATACHANNEL_DIR=$$DATACHANNEL_DIR looks like a UCRT install (nanosleep64). Ignoring it — use C:/libs/libdatachannel-mingw64")
        DATACHANNEL_DIR =
    }
}

win32:isEmpty(DATACHANNEL_DIR) {
    exists(C:/libs/libdatachannel-qtmingw/include/rtc): \
        DATACHANNEL_DIR = C:/libs/libdatachannel-qtmingw
    else: exists(C:/libs/libdatachannel-mingw64/include/rtc): \
        DATACHANNEL_DIR = C:/libs/libdatachannel-mingw64
    else: exists(D:/libs/libdatachannel-mingw64/include/rtc): \
        DATACHANNEL_DIR = D:/libs/libdatachannel-mingw64
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
        # Link import lib only — never -L msys2 ucrt64/mingw64 openssl (CRT clash).
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
