#pragma once
#include <QColor>
#include <QVector>
#include <QWidget>

class TrendGraph : public QWidget {
public:
    explicit TrendGraph(QWidget* parent = nullptr);
    void append(double value);
    void setDark(bool dark);
    QSize sizeHint() const override { return {420, 190}; }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QVector<double> m_samples;
    bool m_dark = true;
};
