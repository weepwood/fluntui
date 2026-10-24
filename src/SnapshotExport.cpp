#include "SnapshotExport.h"
#include "Formatting.h"
#include <QJsonArray>
#include <QJsonValue>
#include <algorithm>

namespace fsi {
QJsonObject snapshotToJson(const SystemSnapshot& s, const QString& appVersion,
                           const QString& sampledAtUtc, const QString& exportedAtUtc) {
    QJsonArray volumes;
    for (const DiskInfo& disk : s.disks) {
        const quint64 used = disk.total - std::min(disk.free, disk.total);
        volumes.append(QJsonObject{
            {QStringLiteral("path"), disk.name},
            {QStringLiteral("filesystem"), disk.filesystem},
            {QStringLiteral("totalBytes"), static_cast<double>(disk.total)},
            {QStringLiteral("availableBytes"), static_cast<double>(disk.free)},
            {QStringLiteral("usedBytes"), static_cast<double>(used)},
            {QStringLiteral("usedPercent"), percentage(used, disk.total)}
        });
    }
    return QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("appVersion"), appVersion},
        {QStringLiteral("sampledAt"), sampledAtUtc},
        {QStringLiteral("exportedAt"), exportedAtUtc},
        {QStringLiteral("hardware"), QJsonObject{
            {QStringLiteral("hostname"), s.hostname},
            {QStringLiteral("systemName"), s.systemName},
            {QStringLiteral("architecture"), s.architecture},
            {QStringLiteral("processor"), s.cpuName},
            {QStringLiteral("graphicsAdapter"), s.gpuName},
            {QStringLiteral("logicalCores"), s.logicalCores},
            {QStringLiteral("ipv4"), s.ipv4}
        }},
        {QStringLiteral("resources"), QJsonObject{
            {QStringLiteral("cpuPercent"), s.cpuPercent < 0
                ? QJsonValue(QJsonValue::Null) : QJsonValue(s.cpuPercent)},
            {QStringLiteral("memoryTotalBytes"), static_cast<double>(s.memoryTotal)},
            {QStringLiteral("memoryAvailableBytes"), static_cast<double>(s.memoryAvailable)},
            {QStringLiteral("uptimeSeconds"), static_cast<double>(s.uptime)}
        }},
        {QStringLiteral("network"), QJsonObject{
            {QStringLiteral("supported"), s.networkSupported},
            {QStringLiteral("downloadBytesPerSecond"), s.downloadBps},
            {QStringLiteral("uploadBytesPerSecond"), s.uploadBps},
            {QStringLiteral("downloadedTotalBytes"), static_cast<double>(s.downloadedTotal)},
            {QStringLiteral("uploadedTotalBytes"), static_cast<double>(s.uploadedTotal)}
        }},
        {QStringLiteral("disks"), volumes}
    };
}
} // namespace fsi
