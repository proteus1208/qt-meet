# Google Protocol Buffers.
# PROTOBUF_DIR       install prefix containing include/, lib/, bin/protoc
# PROTOBUF_LIBS      optional override of the link line (protobuf >= 22 also needs abseil libs)

PROTOBUF_DIR = $$(PROTOBUF_DIR)
win32:isEmpty(PROTOBUF_DIR): PROTOBUF_DIR = D:/Apps/protobuf_21_mingw

isEmpty(PROTOBUF_DIR) {
    PROTOC = protoc
} else {
    INCLUDEPATH += $$PROTOBUF_DIR/include
    LIBS        += -L$$PROTOBUF_DIR/lib
    win32: PROTOC = $$PROTOBUF_DIR/bin/protoc.exe
    else:  PROTOC = $$PROTOBUF_DIR/bin/protoc
}

PROTOBUF_LIBS = $$(PROTOBUF_LIBS)
isEmpty(PROTOBUF_LIBS) {
    win32-msvc*: PROTOBUF_LIBS = -llibprotobuf
    else:        PROTOBUF_LIBS = -lprotobuf
}

!contains(TEMPLATE, lib): LIBS += $$PROTOBUF_LIBS
