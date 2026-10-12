include(../config/common.pri)
include(../config/libs.pri)

TEMPLATE = app
TARGET   = QtMeetClient
DESTDIR  = $$QM_BIN_DIR
QT      += core gui network multimedia quick qml quickcontrols2 concurrent

qmUseLib(qmrtc)
qmUseLib(qmmedia)
qmUseLib(qmcommon)
qmUseLib(qmproto)

include(../config/protobuf.pri)
include(../config/ffmpeg.pri)
include(../config/datachannel.pri)
include(../config/opus.pri)
include(../config/apm.pri)

INCLUDEPATH += $$PWD

HEADERS += \
    ClientApp.h \
    controllers/CallController.h \
    controllers/ChatController.h \
    controllers/ExplorerController.h \
    controllers/SessionController.h \
    core/CallEngine.h \
    core/FileTransfer.h \
    core/NetworkClient.h \
    core/NetworkWorker.h \
    models/ChatModel.h \
    models/ConversationModel.h \
    models/ParticipantModel.h \
    models/RemoteFsModel.h \
    models/TransferModel.h \
    models/UserListModel.h

SOURCES += \
    main.cpp \
    ClientApp.cpp \
    controllers/CallController.cpp \
    controllers/ChatController.cpp \
    controllers/ExplorerController.cpp \
    controllers/SessionController.cpp \
    core/CallEngine.cpp \
    core/FileTransfer.cpp \
    core/NetworkClient.cpp \
    core/NetworkWorker.cpp \
    models/ChatModel.cpp \
    models/ConversationModel.cpp \
    models/ParticipantModel.cpp \
    models/RemoteFsModel.cpp \
    models/TransferModel.cpp \
    models/UserListModel.cpp

RESOURCES += qml.qrc
