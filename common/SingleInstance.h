#pragma once

#include <QObject>
#include <QString>

class QLocalServer;
class QSharedMemory;

namespace qm {

/// Aim: ensure only one process of this app runs. A second launch signals the first to raise
/// its window, then exits.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);
    ~SingleInstance() override;

    /// Returns true if this process is the primary instance.
    bool tryBecomePrimary();
    /// Ask the already-running primary to show its window (used by secondary).
    static bool notifyPrimary(const QString &key, const QByteArray &message = QByteArrayLiteral("raise"));

Q_SIGNALS:
    void activationRequested();

private:
    QString         m_key;
    QSharedMemory  *m_memory = nullptr;
    QLocalServer   *m_server = nullptr;
};

} // namespace qm
