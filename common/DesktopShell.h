#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QAction;
class QMenu;
class QSystemTrayIcon;
class QWindow;

namespace qm {

/// Aim: system tray + global show hotkey for client/server admin windows.
class DesktopShell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString showHotkey READ showHotkey WRITE setShowHotkey NOTIFY showHotkeyChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)

public:
    explicit DesktopShell(const QString &appTitle, QObject *parent = nullptr);
    ~DesktopShell() override;

    QString showHotkey() const;
    void    setShowHotkey(const QString &sequence);
    bool    trayAvailable() const;

    /// Bind the QML ApplicationWindow / QQuickWindow so raise/hide work.
    Q_INVOKABLE void attachWindow(QObject *windowObject);
    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideToTray();
    Q_INVOKABLE void quitApp();

Q_SIGNALS:
    void showHotkeyChanged();
    void showHotkeyFailed(const QString &reason);

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result);

private:
    class HotkeyFilter;

    void setupTray();
    QIcon makeIcon() const;
    bool  registerHotkey();
    void  unregisterHotkey();
    bool  parseHotkey(const QString &sequence, int *modifiers, int *vk) const;

    QString          m_title;
    QString          m_showHotkey = QStringLiteral("Ctrl+Shift+Q");
    QWindow         *m_window = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu           *m_trayMenu = nullptr;
    HotkeyFilter    *m_filter = nullptr;
    int              m_hotkeyId = 1;
    bool             m_hotkeyRegistered = false;
};

} // namespace qm
