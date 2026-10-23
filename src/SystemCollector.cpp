#include "SystemCollector.h"
#include "Formatting.h"
#include <QFile>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSettings>
#include <QStorageInfo>
#include <QSysInfo>
#include <QThread>
#include <algorithm>
#include <cstdint>

#ifdef Q_OS_WIN
// Windows SDK netioapi declarations require ws2ipdef.h before iphlpapi.h.
// Include Winsock2 before windows.h to avoid legacy winsock.h conflicts.
#include <winsock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ifdef.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#endif

namespace {
#ifdef Q_OS_WIN
quint64 toInteger(const FILETIME& time) {
    return (static_cast<quint64>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}
#endif
#ifdef Q_OS_LINUX
QString procLine(const QString& filename, const QString& prefix) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(prefix)) return line;
    }
    return {};
}
quint64 memInfoKb(const QString& name) {
    const auto value = procLine(QStringLiteral("/proc/meminfo"), name + QStringLiteral(":"));
    const auto fields = value.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    return fields.size() >= 2 ? fields[1].toULongLong() : 0;
}
#endif
} // namespace

SystemCollector::SystemCollector() { m_clock.start(); }

SystemSnapshot SystemCollector::sample() {
    SystemSnapshot result;
    result.hostname = QSysInfo::machineHostName();
    result.systemName = QSysInfo::prettyProductName();
    result.cpuName = cpuModel();
    result.gpuName = gpuModel();
    result.architecture = QSysInfo::currentCpuArchitecture();
    result.logicalCores = QThread::idealThreadCount();
    result.cpuPercent = sampleCpu();
    sampleMemory(result);
    result.uptime = uptimeSeconds();

    for (const auto& network : QNetworkInterface::allInterfaces()) {
        if (!network.flags().testFlag(QNetworkInterface::IsUp) ||
            network.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        for (const auto& entry : network.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.ip().isLoopback()) {
                result.ipv4 = entry.ip().toString();
                break;
            }
        }
        if (!result.ipv4.isEmpty()) break;
    }
    sampleNetwork(result);
    for (const auto& volume : QStorageInfo::mountedVolumes()) {
        if (!volume.isValid() || !volume.isReady() || volume.bytesTotal() <= 0) continue;
        DiskInfo disk;
        disk.name = volume.rootPath();
        disk.filesystem = QString::fromUtf8(volume.fileSystemType());
        disk.total = static_cast<quint64>(volume.bytesTotal());
        disk.free = static_cast<quint64>(std::max<qint64>(0, volume.bytesAvailable()));
        result.disks.push_back(disk);
    }
    std::sort(result.disks.begin(), result.disks.end(), [](const auto& left, const auto& right) {
        return left.name.localeAwareCompare(right.name) < 0;
    });
    return result;
}

double SystemCollector::sampleCpu() {
    quint64 idle = 0;
    quint64 total = 0;
#ifdef Q_OS_WIN
    FILETIME idleTime{}, kernelTime{}, userTime{};
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return -1;
    idle = toInteger(idleTime);
    total = toInteger(kernelTime) + toInteger(userTime);
#elif defined(Q_OS_LINUX)
    const QString line = procLine(QStringLiteral("/proc/stat"), QStringLiteral("cpu "));
    const auto numbers = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (numbers.size() < 5) return -1;
    for (int index = 1; index < numbers.size(); ++index) total += numbers[index].toULongLong();
    idle = numbers[4].toULongLong();
    if (numbers.size() > 5) idle += numbers[5].toULongLong();
#else
    return -1;
#endif
    double usage = -1;
    if (m_hasCpu && total > m_cpuTotal && idle >= m_cpuIdle) {
        const quint64 delta = total - m_cpuTotal;
        const quint64 idleDelta = idle - m_cpuIdle;
        usage = fsi::percentage(delta > idleDelta ? delta - idleDelta : 0, delta);
    }
    m_cpuIdle = idle;
    m_cpuTotal = total;
    m_hasCpu = true;
    return usage;
}

void SystemCollector::sampleMemory(SystemSnapshot& result) {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        result.memoryTotal = status.ullTotalPhys;
        result.memoryAvailable = status.ullAvailPhys;
    }
#elif defined(Q_OS_LINUX)
    result.memoryTotal = memInfoKb(QStringLiteral("MemTotal")) * 1024;
    result.memoryAvailable = memInfoKb(QStringLiteral("MemAvailable")) * 1024;
#endif
}

void SystemCollector::sampleNetwork(SystemSnapshot& result) {
    quint64 rx = 0;
    quint64 tx = 0;
#ifdef Q_OS_WIN
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table) return;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const auto& row = table->Table[i];
        if (row.OperStatus != IfOperStatusUp || row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        rx += row.InOctets;
        tx += row.OutOctets;
    }
    FreeMibTable(table);
#elif defined(Q_OS_LINUX)
    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    while (!file.atEnd()) {
        const auto line = QString::fromUtf8(file.readLine());
        const int colon = line.indexOf(':');
        if (colon == -1 || line.left(colon).trimmed() == QStringLiteral("lo")) continue;
        const auto fields = line.mid(colon + 1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 16) continue;
        rx += fields[0].toULongLong();
        tx += fields[8].toULongLong();
    }
#else
    return;
#endif
    result.networkSupported = true;
    result.downloadedTotal = rx;
    result.uploadedTotal = tx;
    const qint64 now = m_clock.elapsed();
    if (m_hasNetwork) {
        const double seconds = static_cast<double>(now - m_lastNetworkMs) / 1000.0;
        result.downloadBps = fsi::deltaRate(rx, m_rx, seconds);
        result.uploadBps = fsi::deltaRate(tx, m_tx, seconds);
    }
    m_rx = rx;
    m_tx = tx;
    m_lastNetworkMs = now;
    m_hasNetwork = true;
}

QString SystemCollector::cpuModel() const {
#ifdef Q_OS_WIN
    const QSettings cpu(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), QSettings::NativeFormat);
    const QString name = cpu.value(QStringLiteral("ProcessorNameString")).toString().trimmed();
    if (!name.isEmpty()) return name;
#elif defined(Q_OS_LINUX)
    const QString line = procLine(QStringLiteral("/proc/cpuinfo"), QStringLiteral("model name"));
    if (line.contains(':')) return line.section(':', 1).trimmed();
#endif
    return QSysInfo::currentCpuArchitecture();
}

QString SystemCollector::gpuModel() const {
#ifdef Q_OS_WIN
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        if ((device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) == 0 &&
            (device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != 0) {
            return QString::fromWCharArray(device.DeviceString);
        }
        device = DISPLAY_DEVICEW{};
        device.cb = sizeof(device);
    }
#endif
    return QStringLiteral("未检测 / Not detected");
}

quint64 SystemCollector::uptimeSeconds() const {
#ifdef Q_OS_WIN
    return GetTickCount64() / 1000;
#elif defined(Q_OS_LINUX)
    QFile file(QStringLiteral("/proc/uptime"));
    if (file.open(QIODevice::ReadOnly)) {
        const auto first = file.readAll().split(' ').value(0);
        return static_cast<quint64>(first.toDouble());
    }
#endif
    return 0;
}
