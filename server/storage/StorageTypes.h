#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include "common/Ids.h"

namespace qm {

struct UserRecord
{
    UserId     id = 0;
    QString    username;
    QString    displayName;
    QByteArray passwordHash;
    QByteArray salt;
    qint64     createdMs = 0;
};

struct ConversationRecord
{
    enum class Kind { Direct, Meeting };

    ConversationId id = 0;
    Kind           kind = Kind::Direct;
    QList<UserId>  members;
    qint64         createdMs = 0;
};

struct MessageRecord
{
    MessageId      id = 0;
    ConversationId conversationId = 0;
    UserId         fromUser = 0;
    QString        clientMsgId;
    QString        text;
    QString        fileId;
    qint64         timestampMs = 0;
};

struct FileRecord
{
    QString fileId;
    QString name;
    QString mime;
    QString sha256;
    QString blobPath;
    quint64 size = 0;
    quint64 received = 0;
    UserId  owner = 0;
    bool    complete = false;
    qint64  createdMs = 0;
};

struct MeetingRecord
{
    QString        meetingId;
    QString        code;
    QString        title;
    UserId         host = 0;
    ConversationId conversationId = 0;
    bool           active = true;
    qint64         createdMs = 0;
};

} // namespace qm
