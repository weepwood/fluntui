#pragma once
#include <QElapsedTimer>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct DiskInfo {
    QString name;
    QString filesystem;
    quint64 total = 0;
    quint64 free = 0;
};

struct SystemSnapshot {
    QString hostname;
    QString systemName;
    QString cpuName;
    QString gpuName;
    QString architecture;
    QString ipv4;
    int logicalCores = 0;
    double cpuPercent = -1;
    quint64 memoryTotal = 0;
    quint64 memoryAvailable = 0;
    quint64 uptime = 0;
    double uploadBps = 0;
    double downloadBps = 0;
    quint64 uploadedTotal = 0;
    quint64 downloadedTotal = 0;
    bool networkSupported = false;
    QVector<DiskInfo> disks;
};

class SystemCollector {
public:
    SystemCollector();
    SystemSnapshot sample();
private:
    double sampleCpu();
    void sampleMemory(SystemSnapshot& result);
    void sampleNetwork(SystemSnapshot& result);
    QString cpuModel() const;
    QString gpuModel() const;
    quint64 uptimeSeconds() const;
    quint64 m_cpuIdle = 0;
    quint64 m_cpuTotal = 0;
    bool m_hasCpu = false;
    quint64 m_rx = 0;
    quint64 m_tx = 0;
    bool m_hasNetwork = false;
    QElapsedTimer m_clock;
    qint64 m_lastNetworkMs = 0;
};
