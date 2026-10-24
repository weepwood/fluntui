#pragma once
#include "SystemCollector.h"
#include <QJsonObject>
#include <QString>

namespace fsi {
// QtCore-only serializer: no dialog, filesystem or UI side effects.
QJsonObject snapshotToJson(const SystemSnapshot& sample, const QString& appVersion,
                           const QString& sampledAtUtc, const QString& exportedAtUtc);
} // namespace fsi
