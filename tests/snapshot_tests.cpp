#include "SnapshotExport.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <cstdlib>
#include <iostream>

static void check(bool ok, const char* what) {
    if (!ok) { std::cerr << "FAILED: " << what << '\n'; std::exit(EXIT_FAILURE); }
}
#define CHECK(x) check((x), #x)

int main() {
    SystemSnapshot sample;
    sample.hostname = QStringLiteral("test-host");
    sample.systemName = QStringLiteral("TestOS");
    sample.cpuName = QStringLiteral("Unit Test CPU");
    sample.logicalCores = 8;
    sample.ipv4 = QStringLiteral("192.0.2.42");
    sample.memoryTotal = 16ULL * 1024 * 1024 * 1024;
    sample.memoryAvailable = 8ULL * 1024 * 1024 * 1024;
    sample.networkSupported = true;
    sample.downloadBps = 512.25;
    sample.downloadedTotal = 2048;
    sample.disks.append({QStringLiteral("C:/"), QStringLiteral("NTFS"), 1000, 250});

    const QJsonObject json = fsi::snapshotToJson(
        sample, QStringLiteral("0.4.0"), QStringLiteral("2026-10-24T00:00:00.000Z"),
        QStringLiteral("2026-10-24T00:00:01.000Z"));
    CHECK(json.value(QStringLiteral("schemaVersion")).toInt() == 1);
    CHECK(json.value(QStringLiteral("sampledAt")).toString() == QStringLiteral("2026-10-24T00:00:00.000Z"));
    const QJsonObject hardware = json.value(QStringLiteral("hardware")).toObject();
    CHECK(hardware.value(QStringLiteral("hostname")).toString() == QStringLiteral("test-host"));
    CHECK(hardware.value(QStringLiteral("ipv4")).toString() == QStringLiteral("192.0.2.42"));
    CHECK(hardware.value(QStringLiteral("logicalCores")).toInt() == 8);
    const QJsonObject resources = json.value(QStringLiteral("resources")).toObject();
    CHECK(resources.value(QStringLiteral("cpuPercent")).isNull());
    CHECK(resources.value(QStringLiteral("memoryTotalBytes")).toDouble() == 16.0 * 1024 * 1024 * 1024);
    const QJsonObject network = json.value(QStringLiteral("network")).toObject();
    CHECK(network.value(QStringLiteral("supported")).toBool());
    CHECK(network.value(QStringLiteral("downloadBytesPerSecond")).toDouble() == 512.25);
    const QJsonArray disks = json.value(QStringLiteral("disks")).toArray();
    CHECK(disks.size() == 1);
    CHECK(disks.at(0).toObject().value(QStringLiteral("usedBytes")).toDouble() == 750.0);
    CHECK(disks.at(0).toObject().value(QStringLiteral("usedPercent")).toDouble() == 75.0);

    QJsonParseError error{};
    const QJsonDocument decoded = QJsonDocument::fromJson(QJsonDocument(json).toJson(), &error);
    CHECK(error.error == QJsonParseError::NoError);
    CHECK(decoded.object() == json);

    sample.cpuPercent = 42.5;
    sample.disks.append({QStringLiteral("D:/"), QStringLiteral("NTFS"), 0, 0});
    const QJsonObject changed = fsi::snapshotToJson(sample, QStringLiteral("0.4.0"), {}, {});
    CHECK(changed.value(QStringLiteral("resources")).toObject().value(QStringLiteral("cpuPercent")).toDouble() == 42.5);
    CHECK(changed.value(QStringLiteral("disks")).toArray().at(1).toObject()
        .value(QStringLiteral("usedPercent")).toDouble() == 0.0);
    std::cout << "Snapshot export tests passed\n";
}
