include(../config/common.pri)
include(../config/libs.pri)

TEMPLATE = app
TARGET   = QtMeetServer
DESTDIR  = $$QM_BIN_DIR
QT      += core gui network sql concurrent quick qml

qmUseLib(qmrtc)
qmUseLib(qmcommon)
qmUseLib(qmproto)

include(../config/protobuf.pri)
include(../config/datachannel.pri)
include(../config/apm.pri)

INCLUDEPATH += $$PWD

HEADERS += \
    ServerApp.h \
    admin/AdminController.h \
    dispatch/Dispatcher.h \
    dispatch/IHandler.h \
    dispatch/RequestContext.h \
    streaming/HttpMediaServer.h \
    streaming/Sfu.h \
    net/Connection.h \
    net/ConnectionManager.h \
    net/NetWorker.h \
    net/SessionRegistry.h \
    services/AuthService.h \
    services/ChatService.h \
    services/FileService.h \
    services/FolderService.h \
    services/MeetingService.h \
    services/UserService.h \
    storage/StorageManager.h \
    storage/StorageTypes.h

SOURCES += \
    main.cpp \
    ServerApp.cpp \
    admin/AdminController.cpp \
    dispatch/Dispatcher.cpp \
    streaming/HttpMediaServer.cpp \
    streaming/Sfu.cpp \
    net/Connection.cpp \
    net/ConnectionManager.cpp \
    net/NetWorker.cpp \
    net/SessionRegistry.cpp \
    services/AuthService.cpp \
    services/ChatService.cpp \
    services/FileService.cpp \
    services/FolderService.cpp \
    services/MeetingService.cpp \
    services/UserService.cpp \
    storage/StorageManager.cpp

RESOURCES += server.qrc

# Ship TLS certs next to the server binary for local / offline LAN use.
CERT_SRC = $$shell_path($$QM_ROOT/server/certs)
CERT_DST = $$shell_path($$DESTDIR/certs)
win32 {
    QMAKE_POST_LINK += if not exist $$shell_quote($$CERT_DST) mkdir $$shell_quote($$CERT_DST) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$CERT_SRC\\server.crt) $$shell_quote($$CERT_DST\\server.crt) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$CERT_SRC\\server.key) $$shell_quote($$CERT_DST\\server.key) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$CERT_SRC\\ca.crt) $$shell_quote($$CERT_DST\\ca.crt) $$escape_expand(\\n\\t)
} else {
    QMAKE_POST_LINK += $$quote(mkdir -p $$CERT_DST && cp -f $$CERT_SRC/server.crt $$CERT_SRC/server.key $$CERT_SRC/ca.crt $$CERT_DST/)
}
