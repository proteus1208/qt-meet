#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QUrl>

#include "messages.pb.h"

namespace qm {

/// Aim: one directory listing of the server's hosted folder (folders first), with the
/// tokenized HTTP url used to play each file.
class RemoteFsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        IsDirRole,
        SizeRole,
        ModifiedRole,
        MimeRole,
        IsPlayableRole,
    };

    explicit RemoteFsModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setListing(const proto::DirListRes &listing, const QString &token);
    void clear();

    QString path() const;
    QUrl    urlOf(int row) const;
    bool    isDir(int row) const;
    QString nameOf(int row) const;

Q_SIGNALS:
    void pathChanged();

private:
    QList<proto::DirEntry> m_entries;
    QString                m_path;
    QString                m_urlPrefix;
    QString                m_token;
};

} // namespace qm
