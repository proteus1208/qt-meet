# Shared settings for every sub-project. Include first in each .pro.

QM_ROOT       = $$clean_path($$PWD/..)
QM_BUILD_ROOT = $$shadowed($$QM_ROOT)
QM_LIB_DIR    = $$QM_BUILD_ROOT/lib
QM_BIN_DIR    = $$QM_BUILD_ROOT/bin
QM_GEN_DIR    = $$QM_BUILD_ROOT/gen

CONFIG += c++17 warn_on
CONFIG -= debug_and_release debug_and_release_target

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
