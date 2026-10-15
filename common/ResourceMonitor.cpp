#include "ResourceMonitor.h"

#include <QTimer>

namespace qm {

ResourceMonitor::ResourceMonitor(QObject *parent)
    : QObject(parent)
{
}

ResourceMonitor::~ResourceMonitor() = default;

void ResourceMonitor::start(int intervalMs)
{
    Q_UNUSED(intervalMs)
}

void ResourceMonitor::stop()
{
}

double ResourceMonitor::cpuLoad() const
{
    return m_cpuLoad;
}

bool ResourceMonitor::gpuAvailable() const
{
    return m_gpuAvailable;
}

ResourceMonitor::Pressure ResourceMonitor::pressure() const
{
    return m_pressure;
}

void ResourceMonitor::sample()
{
}

double ResourceMonitor::readCpuLoad()
{
    return 0.0;
}

} // namespace qm
