include(../config/common.pri)
include(../config/libs.pri)

TEMPLATE = app
TARGET   = QtMeetServer
DESTDIR  = $$QM_BIN_DIR
QT      += core gui network sql concurrent quick qml

qtHaveModule(httpserver) {
    QT      += httpserver
    DEFINES += QM_HAVE_HTTPSERVER
} else {
    warning("Qt HttpServer module not installed; hosted-folder streaming is disabled (install 'Qt HTTP Server' via Qt Maintenance Tool)")
}

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
