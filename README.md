# Fluent System Info · C++ / Qt 6

> **v0.4.0** · 电脑硬件信息展示与实时资源监控 Demo。使用 **Qt 6 Widgets + C++17 + [Fluent-Qt](https://github.com/calvinhxx/Fluent-Qt)** 原生组件，不需要 Python、PySide6 或 psutil。

仓库：[weepwood/fluntui](https://github.com/weepwood/fluntui)。本项目可直接在 Qt Creator 中打开，采用 C++ Qt 6 原生组件。

## 功能

| 页面 | 内容 |
|---|---|
| **系统总览** | CPU 实时负载、内存占用、系统/根卷使用率、开机时长、48 点性能趋势图 |
| **硬件详情** | 系统名称和架构、逻辑核心、CPU 型号、显示适配器名称、总内存 |
| **磁盘存储** | 所有可用挂载卷容量、文件系统、已用/剩余空间、原生 FluentQt 进度条 |
| **网络监控** | 本机 IPv4、网络接口总上行/下行速率和计数器累计值 |

### FluentQt 原生组件

项目直接链接 `FluentQt::FluentQt`，使用 `fluent::windowing::Window`、`fluent::layout::Card`、`fluent::basicinput::Button`、`fluent::status_info::ProgressBar`、`fluent::FluentElement`。界面图标来自 FluentQt 内置的 **Fluent UI System Icons** 字体（`fluent::FontIcon` 与 `Button::setIconGlyph`），按主题自动适配颜色，跨平台渲染一致、缩放清晰，不依赖系统图标字体。折线图以 C++ QWidget/QPainter 实现，页面组织与布局基于 Qt Widgets。

- 深色/浅色切换与 `QSettings` 本地记忆；记住窗口位置和大小（显示器布局变化后自动回到可见区域）
- 可切换 **1 / 2 / 5 / 10 秒** 采样间隔（本地记忆）；暂停/继续监控、手动刷新
- 侧栏可导出 **JSON 快照**（硬件、资源、网络计数器、所有卷信息及采样时间）；原子保存到用户选择的位置
- Ctrl+1–4 快速切换页面、Ctrl+P 暂停/继续、Ctrl+E 导出快照
- 硬件静态信息（CPU/显卡/系统型号）仅在首次采样读取一次，后续采样只更新动态指标
- 采样只读取本机系统计数器，无网络请求和遥测；导出文件可能包含主机名、局域网 IPv4，请谨慎分享
- 系统总览优先显示系统/根卷，避免将其他挂载卷误认为系统盘
- 趋势图采用固定 48 槽时间轴，初始样本不会拉伸至整张图
- HiDPI 初始化；Qt Creator/CMake 开发流程
- `--smoke`：在 offscreen 模式初始化 Qt/FluentQt 基础控件并短暂运行事件循环，不创建依赖 Windows 原生窗口效果的主窗口；不能替代 GUI 视觉验收

## 支持范围

| 功能 | Windows 10/11 | Linux | macOS |
|---|---|---|---|
| Qt/FluentQt 界面 | 主目标 | 库支持 | 库支持 |
| CPU 负载 | GetSystemTimes | /proc/stat | 暂不提供 |
| 物理内存 | GlobalMemoryStatusEx | /proc/meminfo | 暂不提供 |
| 网络字节计数器 | GetIfTable2 | /proc/net/dev | 暂不提供 |
| 运行时间 | GetTickCount64 | /proc/uptime | 暂不提供 |
| 磁盘容量 / IPv4 | Qt API | Qt API | Qt API |
| GPU 显示适配器名称 | EnumDisplayDevicesW | 暂不提供 | 暂不提供 |

注意：显卡字段为 **当前显示适配器名称**，不是多 GPU 全量枚举；网络读数为已启用接口的总字节计数器，可能包含 VPN 或虚拟网卡。网络/CPU 第一次采样尚无差值时速率为 0 / CPU 显示“采样中”。平台不支持的指标会明确显示“不可用”或“暂不支持”，不会伪造数据。

## 监控操作

侧栏「监控控制」支持切换采样间隔、暂停/继续监控或导出 JSON 快照。
暂停后自动采样停止，手动「刷新」仍会更新一次快照。恢复监控时立即采集。
采样间隔通过本地 `QSettings` 保存；仅在用户明确点击导出后写入 JSON 文件，
不会上传服务器。容量单位为字节，网络速率为字节/秒，CPU 未完成首次采样时以
`null` 表示。导出采用原子写入，避免生成半截文件。

## Windows 编译（推荐）

1. 安装 **Qt 6.8.3 MSVC 2022 64-bit**（Qt Widgets、Qt Network），Visual Studio 2022 C++ 开发工具以及 CMake 3.21+。
2. 确保 Windows 构建环境可以访问 GitHub（首次构建通过 CMake `FetchContent` 下载并缓存 Fluent-Qt **v1.10.2**）。
3. 从项目根目录执行 PowerShell 命令：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release --parallel 4
ctest --test-dir build --build-config Release --output-on-failure
.\build\Release\fluent-system-info.exe
```

也可在 **Qt Creator → 打开项目 → CMakeLists.txt** 中选择 Qt 6 Kit 并构建。

对于已经下载好的 Fluent-Qt 源码，支持指定本地路径，避免构建时联网：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" `
  -DFLUENTQT_SOURCE_DIR="D:/dev/Fluent-Qt"
```

**注意**：本地 Fluent-Qt 源码版本应为 `v1.10.2`。该库本身还可能包含其内部构建所需的工具/依赖。

## 无 Qt 环境下执行核心单元测试

便于先校验计算逻辑（不会编译 GUI）：

```bash
cmake -S . -B build-core -DFSI_BUILD_APP=OFF -DFSI_BUILD_TESTS=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## GitHub Actions：自动编译与发布

- **持续集成**（`.github/workflows/ci.yml`）：向 `main` 推送代码、创建 PR 或手动运行时，执行 Linux 格式化逻辑测试、Windows MSVC/Qt 6/FluentQt 编译、CTest 和已部署 Qt 运行库的无界面 FluentQt 控件测试。
- **自动 Release**（`.github/workflows/release.yml`）：首次推送新版本号至 `main`，或推送与 CMake 版本号一致的 `vX.Y.Z` 标签，会执行 Windows 编译、测试、`windeployqt` 运行库部署、ZIP 打包、SHA256 校验和 GitHub Release 发布。支持手动运行 workflow。**已有版本的 Release 不会被覆盖**。
- **手动打包**（`.github/workflows/build-windows.yml`）：生成 Actions Artifacts，**不创建 Release**，适合验收构建。

### 首次发版和后续发版

`CMakeLists.txt` 中的版本 `0.4.0` 对应 `v0.4.0` Release。将新版本号推送至 `main` 后会自动发布：

`FluentSystemInfo-v0.4.0-Windows-x64.zip` 和 `SHA256SUMS.txt`。

后续版本发布，先修改 `project(FluentSystemInfo VERSION 0.4.1 ...)` 的版本号并推送 `main`，系统自动创建 `v0.4.1` Release；也可以将相同版本的标签推送到远端。

```bash
git tag v0.4.1
git push origin v0.4.1
```

ZIP 为便携目录，包含 EXE、Qt 运行库和必要平台插件，不是单文件 EXE 或安装器。尚未代码签名；Windows SmartScreen 可能提示。首次 CI 编译能否通过取决于 FluentQt 上游版本与 Qt SDK 的实际兼容性，请以 Actions 结果为准。**Release 只有在 Windows 编译、单元测试和已部署 Qt 的无界面控件初始化检查成功后才会创建。完整窗口仍需 Windows 实机验收。**

## 项目结构

```text
FluentSystemInfo/
├─ CMakeLists.txt
├─ src/
│  ├─ main.cpp
│  ├─ MainWindow.{h,cpp}         # FluentQt 界面与页面
│  ├─ SystemCollector.{h,cpp}    # Win32/Qt/Linux 只读采集
│  ├─ TrendGraph.{h,cpp}         # QPainter 趋势图
│  └─ Formatting.{h,cpp}         # 独立于 Qt 的数据格式化
├─ tests/format_tests.cpp
├─ .github/workflows/
│  ├─ ci.yml
│  ├─ release.yml
│  └─ build-windows.yml
```

## 许可证

本项目采用 MIT 许可证；FluentQt 及 Qt 有各自的许可证要求，二进制发布前请核查许可证和第三方声明。
