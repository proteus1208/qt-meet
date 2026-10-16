# libdatachannel (WebRTC transport). DATACHANNEL_DIR = prefix containing include/ and lib/.

DATACHANNEL_DIR = $$(DATACHANNEL_DIR)

isEmpty(DATACHANNEL_DIR) {
    warning("DATACHANNEL_DIR is not set; libdatachannel headers/libs will not be found")
} else {
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
        # Copy runtime DLLs next to the app (best-effort).
        win32:!isEmpty(DESTDIR) {
            QMAKE_POST_LINK += $$quote(cmd /c "if exist $$shell_quote($$shell_path($$DATACHANNEL_DIR/bin/libdatachannel.dll)) copy /y $$shell_quote($$shell_path($$DATACHANNEL_DIR/bin/libdatachannel.dll)) $$shell_quote($$shell_path($$DESTDIR)\\) >nul & exit /b 0") $$escape_expand(\\n\\t)
            QMAKE_POST_LINK += $$quote(cmd /c "if exist C:\\msys64\\mingw64\\bin\\libssl-3-x64.dll copy /y C:\\msys64\\mingw64\\bin\\libssl-3-x64.dll $$shell_quote($$shell_path($$DESTDIR)\\) >nul & exit /b 0") $$escape_expand(\\n\\t)
            QMAKE_POST_LINK += $$quote(cmd /c "if exist C:\\msys64\\mingw64\\bin\\libcrypto-3-x64.dll copy /y C:\\msys64\\mingw64\\bin\\libcrypto-3-x64.dll $$shell_quote($$shell_path($$DESTDIR)\\) >nul & exit /b 0") $$escape_expand(\\n\\t)
        }
    }
}
