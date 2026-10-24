# FluentChat · 原生 FluentQt 聊天客户端

**技术：C++17 + Qt 6 Widgets + FluentQt 1.10.2 + Qt Network。**

这是一个独立的微信式桌面应用，不是 Electron、WebView 或 Vue 套壳；
也不使用真实微信账户或非公开接口。与已有 `fluent-system-info` 分成两个独立可执行程序。

## 已实现的客户端功能

- FluentQt 原生窗口与按钮，微信式左侧导航 / 会话列表 / 消息面板三栏布局
- 注册、登录、退出；好友申请 / 接受 / 拒绝 / 搜索联系人
- 私聊、创建群聊、近 50 条消息历史、未读数、文字发送
- 动态列表、发布文字动态、点赞 / 取消点赞
- QSettings 记忆浅色 / 深色主题；Ctrl+Enter 发送消息
- 每 2.5 秒同步一次会话和当前聊天消息（**轮询，尚非 WebSocket 推送**）
- 消息以纯文本渲染，不将聊天内容作为 HTML 执行
- 客户端认证令牌仅驻留内存；不保存密码、令牌和聊天内容到本地配置

## 运行服务端

第一阶段沿用此前交付的 WeaveChat v0.1.0 服务端，
它提供账户、好友、群聊、消息和动态 API，并在服务端 SQLite 中保存数据。
服务端源码位于该 ZIP 的 `weavechat/apps/server/`。

解压之前交付的 `WeaveChat-v0.1.0-source.zip`，然后：

~~~bash
cd weavechat
node apps/server/src/index.mjs
~~~

需 Node.js **22.16+**，默认监听 `127.0.0.1:8787`。
测试服务端：`node --test apps/server/test/*.test.mjs`。

> 此 GitHub PR **不包含服务端源码**；使用此前独立交付的服务端。
> 两个本机客户端登录不同账户即可测试聊天流程。
> 固定仅调用 loopback HTTP，不要直接作为公网 IM 服务使用。

## 编译 Windows 客户端

安装 Visual Studio 2022（MSVC）、Qt 6.8.3 MSVC 2022 x64、CMake >= 3.21：

~~~powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" `
  -DFSI_BUILD_APP=ON -DFSI_BUILD_CHAT=ON
cmake --build build --config Release --target fluent-chat --parallel 4
.\build\apps\fluent-chat\Release\fluent-chat.exe
~~~

首次 CMake 配置下载 FluentQt v1.10.2 源码，
也可以通过 `FLUENTQT_SOURCE_DIR` 指向现有本地源码。
GitHub Actions 的 **Native FluentChat Windows** 工作流构建、执行
offscreen FluentQt 控件冒烟测试并上传便携 ZIP。

## 功能边界与后续

首版重点是原生交互与复用可用服务端，并未实现图片、文件传输、
语音视频、端到端加密、WebSocket 实时推送、消息分页、离线缓存或托盘通知。
基于本地开发 HTTP 的认证方案不能直接部署公网；
正式发布需增加 HTTPS/WSS、可信服务端配置、令牌安全存储、断线重连、
网络错误恢复、设备会话管理及安全审计。
