# Opus audio codec (used directly or through FFmpeg libopus). OPUS_DIR = prefix.

OPUS_DIR = $$(OPUS_DIR)

isEmpty(OPUS_DIR) {
    unix: CONFIG += link_pkgconfig
    unix: PKGCONFIG += opus
} else {
    INCLUDEPATH += $$OPUS_DIR/include
    !contains(TEMPLATE, lib): LIBS += -L$$OPUS_DIR/lib -lopus
}
