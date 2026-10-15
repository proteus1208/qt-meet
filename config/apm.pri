# webrtc-audio-processing 1.x (AEC / NS / AGC). APM_DIR = prefix.

APM_DIR = $$(APM_DIR)

isEmpty(APM_DIR) {
    unix: CONFIG += link_pkgconfig
    unix: PKGCONFIG += webrtc-audio-processing-1
    win32: warning("APM_DIR is not set; webrtc-audio-processing headers/libs will not be found")
} else {
    INCLUDEPATH += $$APM_DIR/include/webrtc-audio-processing-1
    !contains(TEMPLATE, lib): LIBS += -L$$APM_DIR/lib -lwebrtc-audio-processing-1
}
