#pragma once
#include "SystemCollector.h"
#include <FluentQt/FluentQt.h>
#include <QVector>

class QLabel;
class QStackedWidget;
class QGridLayout;
class QTimer;
class QVBoxLayout;
class TrendGraph;

struct UsageTile {
    QLabel* value = nullptr;
    QLabel* detail = nullptr;
    fluent::status_info::ProgressBar* progress = nullptr;
};

struct DiskRow {
    QString path;
    QLabel* subtitle = nullptr;
    fluent::status_info::ProgressBar* progress = nullptr;
};

class MainWindow : public fluent::windowing::Window {
public:
    MainWindow();
    ~MainWindow() override;
private:
    QWidget* createSidebar();
    QWidget* createHeader();
    QWidget* createOverview();
    QWidget* createHardware();
    QWidget* createDisks();
    QWidget* createNetwork();
    QWidget* makePage(const QString& title, const QString& description, QVBoxLayout*& layout);
    UsageTile createUsageTile(QGridLayout* grid, int row, int column, const QString& heading,
                              const QString& iconCatalogName, const QString& badge, bool showProgress = true);
    QLabel* addInfoRow(QVBoxLayout* layout, const QString& title);
    void selectPage(int index);
    void refresh();
    void toggleMonitoring();
    void updateMonitoringControls();
    void exportSnapshot();
    void applyTheme(bool dark);
    void restoreWindowGeometry();
    QString formatBytes(quint64 bytes) const;
    void rebuildDiskCards(const QVector<DiskInfo>& disks);

    SystemCollector m_collector;
    QStackedWidget* m_pages = nullptr;
    QVector<fluent::basicinput::Button*> m_navigation;
    QLabel* m_pageTitle = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_device = nullptr;
    fluent::FontIcon* m_brandIcon = nullptr;
    fluent::basicinput::Button* m_themeButton = nullptr;
    fluent::basicinput::Button* m_pauseButton = nullptr;
    fluent::basicinput::Button* m_intervalButton = nullptr;
    UsageTile m_cpu;
    UsageTile m_memory;
    UsageTile m_disk;
    UsageTile m_uptime;
    QLabel* m_cpuText = nullptr;
    QLabel* m_gpuText = nullptr;
    QLabel* m_archText = nullptr;
    QLabel* m_osText = nullptr;
    QLabel* m_coresText = nullptr;
    QLabel* m_memoryText = nullptr;
    QLabel* m_localIp = nullptr;
    QLabel* m_downloadTotal = nullptr;
    QLabel* m_uploadTotal = nullptr;
    QLabel* m_networkMessage = nullptr;
    UsageTile m_down;
    UsageTile m_up;
    TrendGraph* m_cpuChart = nullptr;
    TrendGraph* m_memoryChart = nullptr;
    QVBoxLayout* m_diskList = nullptr;
    QVector<DiskRow> m_diskRows;
    QWidget* m_contentRoot = nullptr;
    bool m_dark = true;
    bool m_paused = false;
    bool m_hasSnapshot = false;
    int m_refreshIntervalMs = 2000;
    QString m_lastUpdateTime;
    QString m_sampledAtIso;
    SystemSnapshot m_lastSnapshot;
    QTimer* m_refreshTimer = nullptr;
};
