# libdatachannel (WebRTC transport). DATACHANNEL_DIR = prefix containing include/ and lib/.

DATACHANNEL_DIR = $$(DATACHANNEL_DIR)

isEmpty(DATACHANNEL_DIR) {
    warning("DATACHANNEL_DIR is not set; libdatachannel headers/libs will not be found")
} else {
    INCLUDEPATH += $$DATACHANNEL_DIR/include
    !contains(TEMPLATE, lib) {
        LIBS += -L$$DATACHANNEL_DIR/lib -ldatachannel
        unix: LIBS += -lssl -lcrypto
        win32: LIBS += -lws2_32 -lbcrypt
    }
}
