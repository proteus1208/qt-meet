#pragma once

#include <QObject>

class QTimer;

namespace qm {

/// Aim: sample CPU load and GPU availability so media code can choose GPU/CPU paths
/// and degrade resolution / frame rate under pressure.
class ResourceMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double cpuLoad READ cpuLoad NOTIFY loadChanged)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable CONSTANT)
    Q_PROPERTY(Pressure pressure READ pressure NOTIFY pressureChanged)

public:
    enum class Pressure { Low, Medium, High };
    Q_ENUM(Pressure)

    explicit ResourceMonitor(QObject *parent = nullptr);
    ~ResourceMonitor() override;

    void start(int intervalMs = 1000);
    void stop();

    double   cpuLoad() const;       // 0.0 .. 1.0
    bool     gpuAvailable() const;
    Pressure pressure() const;

Q_SIGNALS:
    void loadChanged();
    void pressureChanged(qm::ResourceMonitor::Pressure pressure);

private:
    void   sample();
    double readCpuLoad();

    QTimer  *m_timer = nullptr;
    double   m_cpuLoad = 0.0;
    bool     m_gpuAvailable = false;
    Pressure m_pressure = Pressure::Low;
    quint64  m_prevIdle = 0;
    quint64  m_prevTotal = 0;
};

} // namespace qm
