# Shared settings for every sub-project. Include first in each .pro.

QM_ROOT       = $$clean_path($$PWD/..)
QM_BUILD_ROOT = $$shadowed($$QM_ROOT)
QM_LIB_DIR    = $$QM_BUILD_ROOT/lib
QM_BIN_DIR    = $$QM_BUILD_ROOT/bin
QM_GEN_DIR    = $$QM_BUILD_ROOT/gen

CONFIG += c++17 warn_on
CONFIG -= debug_and_release debug_and_release_target

# =============================================================================
# App mode (this is the switch — your kit has no separate Debug/Release)
#   development  → allow opening client/server multiple times (2 clients on 1 PC)
#   production   → single-instance (second launch raises the first window)
# Change the value below, then rebuild. Runtime override: --dev / --production
# =============================================================================
QM_APP_MODE = development

equals(QM_APP_MODE, development) {
    DEFINES += QM_DEVELOPMENT
} else {
    DEFINES += QM_FORCE_PRODUCTION
}

# Third-party headers (protobuf/abseil, libdatachannel) clash with the Qt keyword macros.
DEFINES += QT_NO_KEYWORDS

INCLUDEPATH += $$QM_ROOT $$QM_GEN_DIR
DEPENDPATH  += $$QM_ROOT

win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8 /permissive- /Zc:__cplusplus
    DEFINES += NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
}

unix {
    QMAKE_CXXFLAGS += -Wno-unused-parameter
}
