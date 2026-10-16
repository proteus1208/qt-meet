#include "DesktopShell.h"

#include <QAbstractNativeEventFilter>
#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QEvent>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QWindow>

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace qm {

class DesktopShell::HotkeyFilter : public QAbstractNativeEventFilter
{
public:
    explicit HotkeyFilter(DesktopShell *owner)
        : m_owner(owner)
    {
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
    {
        return m_owner && m_owner->nativeEventFilter(eventType, message, result);
    }

    DesktopShell *m_owner = nullptr;
};

DesktopShell::DesktopShell(const QString &appTitle, QObject *parent)
    : QObject(parent)
    , m_title(appTitle)
{
    setupTray();
    m_filter = new HotkeyFilter(this);
    qApp->installNativeEventFilter(m_filter);
    registerHotkey();
}

DesktopShell::~DesktopShell()
{
    unregisterHotkey();
    if (m_filter) {
        qApp->removeNativeEventFilter(m_filter);
        delete m_filter;
        m_filter = nullptr;
    }
}

QString DesktopShell::showHotkey() const
{
    return m_showHotkey;
}

void DesktopShell::setShowHotkey(const QString &sequence)
{
    const QString trimmed = sequence.trimmed();
    if (trimmed == m_showHotkey)
        return;
    unregisterHotkey();
    m_showHotkey = trimmed;
    Q_EMIT showHotkeyChanged();
    if (!m_showHotkey.isEmpty() && !registerHotkey())
        Q_EMIT showHotkeyFailed(QStringLiteral("Could not register hotkey %1").arg(m_showHotkey));
}

bool DesktopShell::trayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void DesktopShell::attachWindow(QObject *windowObject)
{
    auto *window = qobject_cast<QWindow *>(windowObject);
    if (!window)
        window = windowObject ? windowObject->findChild<QWindow *>() : nullptr;
    if (!window)
        return;
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    // Close button hides to tray instead of quitting (avoids QQuickCloseEvent header).
    m_window->installEventFilter(this);
}

bool DesktopShell::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event && event->type() == QEvent::Close) {
        if (m_tray && m_tray->isVisible()) {
            event->ignore();
            hideToTray();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void DesktopShell::showWindow()
{
    if (!m_window)
        return;
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
#if defined(Q_OS_WIN)
    // Force foreground on Windows after hotkey / second-instance raise.
    if (m_window->winId())
        SetForegroundWindow(reinterpret_cast<HWND>(m_window->winId()));
#endif
}

void DesktopShell::hideToTray()
{
    if (m_window)
        m_window->hide();
}

void DesktopShell::quitApp()
{
    unregisterHotkey();
    if (m_tray)
        m_tray->hide();
    QCoreApplication::quit();
}

void DesktopShell::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_trayMenu = new QMenu;
    QAction *showAction = m_trayMenu->addAction(QStringLiteral("Show %1").arg(m_title));
    connect(showAction, &QAction::triggered, this, &DesktopShell::showWindow);
    m_trayMenu->addSeparator();
    QAction *quitAction = m_trayMenu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, this, &DesktopShell::quitApp);

    m_tray = new QSystemTrayIcon(makeIcon(), this);
    m_tray->setToolTip(m_title);
    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showWindow();
    });
    m_tray->show();
}

QIcon DesktopShell::makeIcon() const
{
    QPixmap px(64, 64);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(QStringLiteral("#3B82F6")));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(4, 4, 56, 56, 12, 12);
    p.setPen(Qt::white);
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(28);
    p.setFont(font);
    const QString letter = m_title.contains(QStringLiteral("Server"), Qt::CaseInsensitive)
                               ? QStringLiteral("S")
                               : QStringLiteral("Q");
    p.drawText(px.rect(), Qt::AlignCenter, letter);
    p.end();
    return QIcon(px);
}

bool DesktopShell::parseHotkey(const QString &sequence, int *modifiers, int *vk) const
{
#if defined(Q_OS_WIN)
    if (!modifiers || !vk)
        return false;
    *modifiers = 0;
    *vk = 0;
    const QKeySequence seq(sequence);
    if (seq.isEmpty() || seq.count() < 1)
        return false;
    const QKeyCombination comb = seq[0];
    const Qt::KeyboardModifiers mods = comb.keyboardModifiers();
    if (mods & Qt::ControlModifier)
        *modifiers |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier)
        *modifiers |= MOD_SHIFT;
    if (mods & Qt::AltModifier)
        *modifiers |= MOD_ALT;
    if (mods & Qt::MetaModifier)
        *modifiers |= MOD_WIN;
    const int qtKey = int(comb.key());
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        *vk = 'A' + (qtKey - Qt::Key_A);
    else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        *vk = '0' + (qtKey - Qt::Key_0);
    else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12)
        *vk = VK_F1 + (qtKey - Qt::Key_F1);
    else
        return false;
    return *vk != 0;
#else
    Q_UNUSED(sequence)
    Q_UNUSED(modifiers)
    Q_UNUSED(vk)
    return false;
#endif
}

bool DesktopShell::registerHotkey()
{
#if defined(Q_OS_WIN)
    int mods = 0;
    int vk = 0;
    if (!parseHotkey(m_showHotkey, &mods, &vk))
        return m_showHotkey.isEmpty();
    if (!RegisterHotKey(nullptr, m_hotkeyId, UINT(mods), UINT(vk)))
        return false;
    m_hotkeyRegistered = true;
    return true;
#else
    // Global hotkeys are Windows-first; other platforms still get tray + in-app raise.
    return true;
#endif
}

void DesktopShell::unregisterHotkey()
{
#if defined(Q_OS_WIN)
    if (m_hotkeyRegistered) {
        UnregisterHotKey(nullptr, m_hotkeyId);
        m_hotkeyRegistered = false;
    }
#endif
}

bool DesktopShell::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)
#if defined(Q_OS_WIN)
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;
    const MSG *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_HOTKEY && int(msg->wParam) == m_hotkeyId) {
        showWindow();
        return true;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
}

} // namespace qm
