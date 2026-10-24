#include "MainWindow.h"
#include "Formatting.h"
#include "TrendGraph.h"
#include <QColor>
#include <QDateTime>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

namespace {
// 内置 Fluent UI System Icons 目录名 → 字形；由库按主题颜色和光学尺寸绘制。
fluent::FontIcon* fontIcon(const QString& catalogName, QWidget* parent = nullptr, int size = 16) {
    auto* icon = new fluent::FontIcon(Typography::Icons::glyph(catalogName), parent);
    icon->setIconSize(size);
    return icon;
}
QLabel* label(const QString& content, const char* role, QWidget* parent = nullptr) {
    auto* result = new QLabel(content, parent);
    result->setObjectName(QString::fromUtf8(role));
    result->setWordWrap(true);
    return result;
}
fluent::layout::Card* card(QWidget* parent = nullptr) {
    auto* surface = new fluent::layout::Card(parent);
    surface->setAppearance(fluent::layout::Card::Layer);
    surface->setMinimumWidth(200);
    return surface;
}
QString textPercent(double percent) {
    return percent >= 0 ? QString::number(percent, 'f', 1) + QStringLiteral("%")
                        : QStringLiteral("采样中…");
}
QString bytes(quint64 value) { return QString::fromStdString(fsi::humanBytes(value)); }
QString rate(double value) { return QString::fromStdString(fsi::humanRate(value)); }
}

MainWindow::MainWindow() {
    setWindowTitle(QStringLiteral("Fluent System Info · 电脑信息"));
    setMinimumSize(900, 620);
    resize(1220, 810);
    restoreWindowGeometry();
    m_contentRoot = new QWidget;
    m_contentRoot->setObjectName(QStringLiteral("contentRoot"));
    auto* frame = new QHBoxLayout(m_contentRoot);
    frame->setSpacing(0);
    frame->setContentsMargins(0, 0, 0, 0);
    frame->addWidget(createSidebar());

    auto* content = new QWidget(m_contentRoot);
    content->setObjectName(QStringLiteral("mainContent"));
    auto* right = new QVBoxLayout(content);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    right->addWidget(createHeader());

    m_pages = new QStackedWidget(content);
    m_pages->setObjectName(QStringLiteral("pages"));
    m_pages->addWidget(createOverview());
    m_pages->addWidget(createHardware());
    m_pages->addWidget(createDisks());
    m_pages->addWidget(createNetwork());
    right->addWidget(m_pages, 1);
    frame->addWidget(content, 1);
    setContentWidget(m_contentRoot);
    for (int index = 0; index < 4; ++index) {
        auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(index + 1)), this);
        QObject::connect(shortcut, &QShortcut::activated, this, [this, index] { selectPage(index); });
    }

    QSettings settings;
    applyTheme(settings.value(QStringLiteral("darkTheme"), true).toBool());
    selectPage(0);
    refresh();
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(2000);
    QObject::connect(m_refreshTimer, &QTimer::timeout, this, [this] { refresh(); });
    m_refreshTimer->start();
}

MainWindow::~MainWindow() {
    QSettings().setValue(QStringLiteral("windowGeometry"), saveGeometry());
}

void MainWindow::restoreWindowGeometry() {
    const QByteArray geometry = QSettings().value(QStringLiteral("windowGeometry")).toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    // 显示器布局可能在两次启动之间变化；若恢复的位置已不可见，回退到主屏幕中央。
    const QRect bounds = this->geometry();
    const auto screens = QGuiApplication::screens();
    bool onScreen = false;
    for (const auto* screen : screens) {
        if (screen->availableGeometry().intersects(bounds)) { onScreen = true; break; }
    }
    if (!onScreen && !screens.isEmpty()) {
        const QRect workArea = screens.first()->availableGeometry();
        move(workArea.center() - QPoint(width() / 2, height() / 2));
    }
}

QWidget* MainWindow::createSidebar() {
    auto* sidebar = new QWidget(m_contentRoot);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(222);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(17, 24, 17, 22);
    layout->setSpacing(10);
    auto* brandRow = new QHBoxLayout;
    brandRow->setSpacing(8);
    m_brandIcon = new fluent::FontIcon(Typography::Icons::glyph(QStringLiteral("ic_fluent_gauge_20_regular")), sidebar);
    m_brandIcon->setIconSize(18);
    brandRow->addWidget(m_brandIcon);
    brandRow->addWidget(label(QStringLiteral("FLUENT / SYSTEM"), "brand", sidebar), 0, Qt::AlignVCenter);
    brandRow->addStretch();
    layout->addLayout(brandRow);
    auto* description = label(QStringLiteral("Hardware Monitor  ·  v") + QString::fromLatin1(FSI_APP_VERSION), "eyebrow", sidebar);
    layout->addWidget(description);
    layout->addSpacing(35);
    layout->addWidget(label(QStringLiteral("工作空间  /  WORKSPACE"), "eyebrow", sidebar));

    const QStringList names = {QStringLiteral("系统总览"), QStringLiteral("硬件详情"),
                               QStringLiteral("磁盘存储"), QStringLiteral("网络监控")};
    const QStringList icons = {QStringLiteral("ic_fluent_home_20_regular"),
                               QStringLiteral("ic_fluent_developer_board_20_regular"),
                               QStringLiteral("ic_fluent_hard_drive_20_regular"),
                               QStringLiteral("ic_fluent_globe_20_regular")};
    for (int index = 0; index < names.size(); ++index) {
        auto* button = new fluent::basicinput::Button(names[index], sidebar);
        button->setFluentStyle(fluent::basicinput::Button::Subtle);
        button->setIconGlyph(Typography::Icons::glyph(icons[index]), 20);
        button->setFluentLayout(fluent::basicinput::Button::IconBefore);
        button->setMinimumHeight(44);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QObject::connect(button, &QPushButton::clicked, this, [this, index] { selectPage(index); });
        m_navigation.append(button);
        layout->addWidget(button);
    }
    layout->addStretch();
    auto* footerRow = new QHBoxLayout;
    footerRow->setSpacing(8);
    footerRow->addWidget(fontIcon(QStringLiteral("ic_fluent_shield_20_regular"), sidebar, 14));
    auto* local = label(QStringLiteral("本地数据 · 无云端传输"), "sidebarFooter", sidebar);
    local->setWordWrap(false);
    footerRow->addWidget(local, 0, Qt::AlignVCenter);
    layout->addLayout(footerRow);
    return sidebar;
}

QWidget* MainWindow::createHeader() {
    auto* header = new QWidget;
    header->setObjectName(QStringLiteral("topbar"));
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(30, 18, 30, 18);
    auto* titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    m_pageTitle = label(QStringLiteral("系统总览"), "pageTitle", header);
    titles->addWidget(m_pageTitle);
    m_status = label(QStringLiteral("等待首次采样"), "eyebrow", header);
    titles->addWidget(m_status);
    layout->addLayout(titles);
    layout->addStretch();
    m_themeButton = new fluent::basicinput::Button(QStringLiteral("深色模式"), header);
    m_themeButton->setFluentLayout(fluent::basicinput::Button::IconBefore);
    layout->addWidget(m_themeButton);
    QObject::connect(m_themeButton, &QPushButton::clicked, this, [this] {
        applyTheme(!m_dark);
        QSettings().setValue(QStringLiteral("darkTheme"), m_dark);
    });
    auto* refreshButton = new fluent::basicinput::Button(QStringLiteral("刷新"), header);
    refreshButton->setFluentStyle(fluent::basicinput::Button::Accent);
    refreshButton->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_arrow_clockwise_20_regular")));
    refreshButton->setFluentLayout(fluent::basicinput::Button::IconBefore);
    layout->addWidget(refreshButton);
    QObject::connect(refreshButton, &QPushButton::clicked, this, [this] { refresh(); });
    return header;
}

QWidget* MainWindow::makePage(const QString& title, const QString& description, QVBoxLayout*& layout) {
    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("pageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* host = new QWidget;
    host->setObjectName(QStringLiteral("scrollContent"));
    layout = new QVBoxLayout(host);
    layout->setContentsMargins(30, 28, 30, 40);
    layout->setSpacing(22);
    layout->addWidget(label(title, "sectionTitle", host));
    layout->addWidget(label(description, "secondary", host));
    scroll->setWidget(host);
    return scroll;
}

UsageTile MainWindow::createUsageTile(QGridLayout* grid, int row, int column, const QString& heading,
                                     const QString& iconCatalogName, const QString& badge, bool showProgress) {
    auto* surface = card();
    auto* vertical = new QVBoxLayout(surface);
    vertical->setContentsMargins(22, 20, 22, 21);
    vertical->setSpacing(10);
    auto* top = new QHBoxLayout;
    top->setSpacing(8);
    top->addWidget(fontIcon(iconCatalogName, surface), 0, Qt::AlignVCenter);
    top->addWidget(label(heading, "metricTitle", surface));
    top->addStretch();
    top->addWidget(label(badge, "metricBadge", surface));
    vertical->addLayout(top);
    UsageTile tile;
    tile.value = label(QStringLiteral("--"), "metricValue", surface);
    vertical->addWidget(tile.value);
    tile.detail = label(QStringLiteral("等待数据"), "secondary", surface);
    vertical->addWidget(tile.detail);
    if (showProgress) {
        tile.progress = new fluent::status_info::ProgressBar(surface);
        tile.progress->setRange(0, 100);
        tile.progress->setValue(0);
        vertical->addSpacing(6);
        vertical->addWidget(tile.progress);
    }
    grid->addWidget(surface, row, column);
    grid->setColumnStretch(column, 1);
    return tile;
}

QLabel* MainWindow::addInfoRow(QVBoxLayout* layout, const QString& title) {
    auto* group = new QWidget;
    auto* row = new QHBoxLayout(group);
    row->setContentsMargins(0, 9, 0, 9);
    row->addWidget(label(title, "infoLabel", group), 0);
    row->addStretch(1);
    auto* value = label(QStringLiteral("读取中…"), "infoValue", group);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(value, 3, Qt::AlignRight);
    layout->addWidget(group);
    return value;
}

QWidget* MainWindow::createOverview() {
    QVBoxLayout* page;
    auto* scroll = makePage(QStringLiteral("你的电脑，一目了然。"),
        QStringLiteral("使用原生 Qt 6 + FluentQt 组件实时观察设备状态。每 2 秒刷新一次。"), page);
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 3, 0, 0);
    grid->setHorizontalSpacing(17);
    grid->setVerticalSpacing(17);
    m_cpu = createUsageTile(grid, 0, 0, QStringLiteral("处理器 / CPU"),
        QStringLiteral("ic_fluent_flash_20_regular"), QStringLiteral("实时负载"));
    m_memory = createUsageTile(grid, 0, 1, QStringLiteral("系统内存 / RAM"),
        QStringLiteral("ic_fluent_memory_16_regular"), QStringLiteral("已使用"));
    m_disk = createUsageTile(grid, 1, 0, QStringLiteral("系统磁盘 / DISK"),
        QStringLiteral("ic_fluent_hard_drive_20_regular"), QStringLiteral("首个卷"));
    m_uptime = createUsageTile(grid, 1, 1, QStringLiteral("系统运行时间"),
        QStringLiteral("ic_fluent_history_20_regular"), QStringLiteral("UPTIME"), false);
    page->addLayout(grid);
    page->addWidget(label(QStringLiteral("性能趋势  /  PERFORMANCE"), "sectionTitle"));
    auto* charts = new QHBoxLayout;
    charts->setSpacing(17);
    auto* cpuCard = card();
    auto* cpuLayout = new QVBoxLayout(cpuCard);
    cpuLayout->setContentsMargins(22, 18, 22, 21);
    cpuLayout->addWidget(label(QStringLiteral("CPU 负载 · 最近 48 次采样"), "metricTitle", cpuCard));
    m_cpuChart = new TrendGraph(cpuCard);
    cpuLayout->addWidget(m_cpuChart);
    charts->addWidget(cpuCard);
    auto* memoryCard = card();
    auto* memoryLayout = new QVBoxLayout(memoryCard);
    memoryLayout->setContentsMargins(22, 18, 22, 21);
    memoryLayout->addWidget(label(QStringLiteral("内存占用 · 最近 48 次采样"), "metricTitle", memoryCard));
    m_memoryChart = new TrendGraph(memoryCard);
    memoryLayout->addWidget(m_memoryChart);
    charts->addWidget(memoryCard);
    page->addLayout(charts);
    m_device = label(QStringLiteral("正在识别设备"), "secondary");
    page->addWidget(m_device);
    page->addStretch();
    return scroll;
}

QWidget* MainWindow::createHardware() {
    QVBoxLayout* page;
    auto* scroll = makePage(QStringLiteral("设备规格"),
        QStringLiteral("查看操作系统、处理器、显卡及内存信息；不可用字段会明确标记。"), page);
    auto* systemCard = card();
    auto* systemRows = new QVBoxLayout(systemCard);
    systemRows->setContentsMargins(24, 21, 24, 23);
    systemRows->addWidget(label(QStringLiteral("计算机与系统"), "sectionTitle", systemCard));
    m_osText = addInfoRow(systemRows, QStringLiteral("操作系统"));
    m_archText = addInfoRow(systemRows, QStringLiteral("系统架构"));
    m_coresText = addInfoRow(systemRows, QStringLiteral("逻辑处理器"));
    page->addWidget(systemCard);
    auto* hardwareCard = card();
    auto* hardwareRows = new QVBoxLayout(hardwareCard);
    hardwareRows->setContentsMargins(24, 21, 24, 23);
    hardwareRows->addWidget(label(QStringLiteral("核心硬件"), "sectionTitle", hardwareCard));
    m_cpuText = addInfoRow(hardwareRows, QStringLiteral("处理器"));
    m_gpuText = addInfoRow(hardwareRows, QStringLiteral("显示适配器"));
    m_memoryText = addInfoRow(hardwareRows, QStringLiteral("物理内存"));
    page->addWidget(hardwareCard);
    page->addStretch();
    return scroll;
}

QWidget* MainWindow::createDisks() {
    QVBoxLayout* page;
    auto* scroll = makePage(QStringLiteral("磁盘与存储"),
        QStringLiteral("列出已挂载的卷；展示文件系统、可用容量和已使用空间。"), page);
    auto* holder = new QWidget;
    m_diskList = new QVBoxLayout(holder);
    m_diskList->setContentsMargins(0, 0, 0, 0);
    m_diskList->setSpacing(14);
    page->addWidget(holder);
    page->addStretch();
    return scroll;
}

QWidget* MainWindow::createNetwork() {
    QVBoxLayout* page;
    auto* scroll = makePage(QStringLiteral("网络活动"),
        QStringLiteral("采集本机网络接口计数器；速率仅代表采样间隔内的平均值。"), page);
    auto* grid = new QGridLayout;
    grid->setSpacing(17);
    m_down = createUsageTile(grid, 0, 0, QStringLiteral("接收 / DOWNLOAD"),
        QStringLiteral("ic_fluent_arrow_download_20_regular"), QStringLiteral("每秒"), false);
    m_up = createUsageTile(grid, 0, 1, QStringLiteral("发送 / UPLOAD"),
        QStringLiteral("ic_fluent_arrow_upload_20_regular"), QStringLiteral("每秒"), false);
    page->addLayout(grid);
    auto* details = card();
    auto* detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(24, 21, 24, 23);
    detailsLayout->addWidget(label(QStringLiteral("接口信息"), "sectionTitle", details));
    m_localIp = addInfoRow(detailsLayout, QStringLiteral("本机 IPv4"));
    m_downloadTotal = addInfoRow(detailsLayout, QStringLiteral("累计接收"));
    m_uploadTotal = addInfoRow(detailsLayout, QStringLiteral("累计发送"));
    page->addWidget(details);
    m_networkMessage = label(QStringLiteral("首个采样间隔结束后开始计算速率。"), "secondary");
    page->addWidget(m_networkMessage);
    page->addStretch();
    return scroll;
}

void MainWindow::selectPage(int index) {
    if (!m_pages || index < 0 || index >= m_pages->count()) return;
    m_pages->setCurrentIndex(index);
    const QStringList titles = {QStringLiteral("系统总览"), QStringLiteral("硬件详情"),
                                QStringLiteral("磁盘存储"), QStringLiteral("网络监控")};
    m_pageTitle->setText(titles.value(index));
    for (int i = 0; i < m_navigation.size(); ++i) {
        m_navigation[i]->setFluentStyle(i == index ? fluent::basicinput::Button::Accent
                                                    : fluent::basicinput::Button::Subtle);
    }
}

QString MainWindow::formatBytes(quint64 value) const { return bytes(value); }

void MainWindow::rebuildDiskCards(const QVector<DiskInfo>& disks) {
    bool namesChanged = disks.size() != m_diskRows.size();
    if (!namesChanged) {
        for (int i = 0; i < disks.size(); ++i) {
            if (disks[i].name != m_diskRows[i].path) { namesChanged = true; break; }
        }
    }
    if (namesChanged) {
        m_diskRows.clear();
        while (m_diskList->count() > 0) {
            auto* item = m_diskList->takeAt(0);
            if (item->widget()) delete item->widget();
            delete item;
        }
        for (const auto& disk : disks) {
            auto* surface = card();
            auto* layout = new QVBoxLayout(surface);
            layout->setContentsMargins(22, 20, 22, 21);
            layout->setSpacing(10);
            layout->addWidget(label(disk.name, "sectionTitle", surface));
            auto* detail = label(QString(), "secondary", surface);
            layout->addWidget(detail);
            auto* progress = new fluent::status_info::ProgressBar(surface);
            progress->setRange(0, 100);
            layout->addWidget(progress);
            m_diskList->addWidget(surface);
            m_diskRows.push_back({disk.name, detail, progress});
        }
    }
    for (int i = 0; i < disks.size(); ++i) {
        const auto& disk = disks[i];
        const auto used = disk.total - std::min(disk.free, disk.total);
        const double percent = fsi::percentage(used, disk.total);
        m_diskRows[i].subtitle->setText(QStringLiteral("%1  ·  已用 %2 / %3  ·  剩余 %4  (%5%)")
            .arg(disk.filesystem.isEmpty() ? QStringLiteral("未知文件系统") : disk.filesystem,
                 bytes(used), bytes(disk.total), bytes(disk.free), QString::number(percent, 'f', 1)));
        m_diskRows[i].progress->setValue(percent);
    }
}

void MainWindow::refresh() {
    const SystemSnapshot s = m_collector.sample();
    const quint64 usedMemory = s.memoryTotal > s.memoryAvailable ? s.memoryTotal - s.memoryAvailable : 0;
    const double memoryPercent = fsi::percentage(usedMemory, s.memoryTotal);
    m_cpu.value->setText(textPercent(s.cpuPercent));
    m_cpu.detail->setText(s.cpuName);
    m_cpu.progress->setValue(std::max(0.0, s.cpuPercent));
    m_memory.value->setText(s.memoryTotal ? textPercent(memoryPercent) : QStringLiteral("不可用"));
    m_memory.detail->setText(bytes(usedMemory) + QStringLiteral(" / ") + bytes(s.memoryTotal));
    m_memory.progress->setValue(memoryPercent);
    m_uptime.value->setText(QString::fromStdString(fsi::humanUptime(s.uptime)));
    m_uptime.detail->setText(QStringLiteral("自上次启动以来"));
    if (!s.disks.isEmpty()) {
        const DiskInfo& disk = s.disks.first();
        const quint64 used = disk.total - std::min(disk.free, disk.total);
        const double percent = fsi::percentage(used, disk.total);
        m_disk.value->setText(textPercent(percent));
        m_disk.detail->setText(disk.name + QStringLiteral("   ") + bytes(used) + QStringLiteral(" / ") + bytes(disk.total));
        m_disk.progress->setValue(percent);
    } else {
        m_disk.value->setText(QStringLiteral("无可用卷"));
        m_disk.detail->clear();
        m_disk.progress->setValue(0);
    }
    m_cpuChart->append(s.cpuPercent);
    if (s.memoryTotal > 0) m_memoryChart->append(memoryPercent);
    m_device->setText(QStringLiteral("● %1   ·   %2   ·   %3")
                      .arg(s.hostname, s.systemName, s.architecture));
    m_osText->setText(s.systemName);
    m_archText->setText(s.architecture);
    m_coresText->setText(QString::number(s.logicalCores));
    m_cpuText->setText(s.cpuName);
    m_gpuText->setText(s.gpuName);
    m_memoryText->setText(bytes(s.memoryTotal));
    rebuildDiskCards(s.disks);
    m_localIp->setText(s.ipv4.isEmpty() ? QStringLiteral("无可用 IPv4") : s.ipv4);
    m_down.value->setText(s.networkSupported ? rate(s.downloadBps) : QStringLiteral("暂不支持"));
    m_up.value->setText(s.networkSupported ? rate(s.uploadBps) : QStringLiteral("暂不支持"));
    m_down.detail->setText(QStringLiteral("各已启用接口汇总"));
    m_up.detail->setText(QStringLiteral("各已启用接口汇总"));
    m_downloadTotal->setText(s.networkSupported ? bytes(s.downloadedTotal) : QStringLiteral("不可用"));
    m_uploadTotal->setText(s.networkSupported ? bytes(s.uploadedTotal) : QStringLiteral("不可用"));
    m_networkMessage->setText(s.networkSupported
        ? QStringLiteral("注意：接口计数器可能包含 VPN / 虚拟网卡，网络切换时速率可能暂时显示 0。")
        : QStringLiteral("该平台尚未实现网络字节计数器。"));
    m_status->setText(QStringLiteral("●  正常运行  ·  最近更新 %1")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

void MainWindow::applyTheme(bool dark) {
    m_dark = dark;
    fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    if (m_themeButton) {
        m_themeButton->setText(dark ? QStringLiteral("深色模式") : QStringLiteral("浅色模式"));
        m_themeButton->setIconGlyph(Typography::Icons::glyph(dark
            ? QStringLiteral("ic_fluent_weather_moon_20_regular")
            : QStringLiteral("ic_fluent_weather_sunny_20_regular")));
    }
    if (m_brandIcon) m_brandIcon->setColor(dark ? QColor(QStringLiteral("#75B7FF")) : QColor(QStringLiteral("#1676DE")));
    if (m_cpuChart) m_cpuChart->setDark(dark);
    if (m_memoryChart) m_memoryChart->setDark(dark);
    const QString foreground = dark ? QStringLiteral("#E7EDF6") : QStringLiteral("#152234");
    const QString muted = dark ? QStringLiteral("#92A1B5") : QStringLiteral("#64748B");
    const QString background = dark ? QStringLiteral("#111820") : QStringLiteral("#F5F7FA");
    const QString sidebar = dark ? QStringLiteral("#0C1118") : QStringLiteral("#E9EDF3");
    const QString stroke = dark ? QStringLiteral("#293342") : QStringLiteral("#DBE2EA");
    m_contentRoot->setStyleSheet(QStringLiteral(R"(
      QWidget#contentRoot, QWidget#mainContent, QStackedWidget#pages,
      QWidget#scrollContent, QScrollArea#pageScroll, QScrollArea#pageScroll > QWidget > QWidget {
        background: %1;
      }
      QWidget#sidebar { background: %2; border-right: 1px solid %3; }
      QWidget#topbar { background: %1; border-bottom: 1px solid %3; }
      QLabel { color: %4; background: transparent; border: none; }
      QLabel#brand { font-size: 13px; font-weight: 800; letter-spacing: 1px; }
      QLabel#eyebrow, QLabel#sidebarFooter { color: %5; font-size: 11px; }
      QLabel#sidebarFooter { font-size: 10px; }
      QLabel#pageTitle { font-size: 21px; font-weight: 700; }
      QLabel#sectionTitle { font-size: 19px; font-weight: 700; }
      QLabel#secondary, QLabel#infoLabel { color: %5; font-size: 12px; }
      QLabel#metricTitle { font-size: 13px; font-weight: 600; }
      QLabel#metricBadge { color: %5; font-size: 10px; }
      QLabel#metricValue { font-size: 31px; font-weight: 750; }
      QLabel#infoValue { font-size: 13px; font-weight: 600; }
      QScrollBar:vertical { width: 8px; background: transparent; }
      QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 28px; }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    )").arg(background, sidebar, stroke, foreground, muted));
}
