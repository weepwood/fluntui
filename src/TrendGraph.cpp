#include "TrendGraph.h"
#include <QPainter>
#include <QLinearGradient>
#include <QSizePolicy>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

TrendGraph::TrendGraph(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
void TrendGraph::setDark(bool dark) { m_dark = dark; update(); }
void TrendGraph::append(double value) {
    if (!std::isfinite(value) || value < 0) return;
    m_samples.append(std::clamp(value, 0.0, 100.0));
    if (m_samples.size() > 48) m_samples.remove(0, m_samples.size() - 48);
    update();
}
void TrendGraph::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(13, 16, -16, -18);
    if (area.width() <= 0 || area.height() <= 0) return;
    const QColor grid = m_dark ? QColor(255, 255, 255, 22) : QColor(20, 32, 51, 22);
    const QColor line = m_dark ? QColor("#75B7FF") : QColor("#1676DE");
    const QColor text = m_dark ? QColor("#94A3B8") : QColor("#64748B");
    painter.setPen(QPen(grid, 1));
    for (int i = 0; i <= 4; ++i) {
        const auto y = area.top() + area.height() * i / 4.0;
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
    painter.setPen(text);
    const auto font = painter.font();
    QFont labelFont(font);
    labelFont.setPointSizeF(9);
    painter.setFont(labelFont);
    painter.drawText(QRectF(area.right() - 42, area.top() - 12, 45, 16), Qt::AlignRight, QStringLiteral("100%"));
    painter.drawText(QRectF(area.right() - 42, area.bottom() - 1, 45, 16), Qt::AlignRight, QStringLiteral("0%"));
    if (m_samples.isEmpty()) {
        painter.drawText(area, Qt::AlignCenter, QStringLiteral("等待采样数据 / Waiting for samples"));
        return;
    }
    QPainterPath path;
    const int count = m_samples.size();
    // Fixed 48 slots prevent short histories from stretching across the graph.
    const qreal step = area.width() / 47.0;
    for (int i = 0; i < count; ++i) {
        const qreal x = area.right() - step * (count - 1 - i);
        const qreal y = area.bottom() - area.height() * m_samples[i] / 100.0;
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }
    QPainterPath fill(path);
    fill.lineTo(area.right(), area.bottom());
    fill.lineTo(area.right() - step * (count - 1), area.bottom());
    fill.closeSubpath();
    QLinearGradient gradient(area.topLeft(), area.bottomLeft());
    QColor strong = line;
    strong.setAlpha(80);
    QColor clear = line;
    clear.setAlpha(0);
    gradient.setColorAt(0, strong);
    gradient.setColorAt(1, clear);
    painter.fillPath(fill, gradient);
    painter.setPen(QPen(line, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
    const qreal lastX = area.right();
    const qreal lastY = area.bottom() - area.height() * m_samples.last() / 100.0;
    painter.setBrush(line);
    painter.drawEllipse(QPointF(lastX, lastY), 3.5, 3.5);
}
