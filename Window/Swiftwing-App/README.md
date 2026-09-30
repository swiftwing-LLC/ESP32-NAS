# Swiftwing NAS 应用（基于 Nas_V1.9.zip）

这个客户端直接连接 ESP32 Master 正在提供的界面。它沿用现有登录、文件管理、系统状态和 Wi-Fi 页面，因此使用前 NAS 仍需开机并连接到可访问的网络；App 不会把 NAS 变成离线存储。

## 打开应用

| 设备 | 入口 | 使用方式 |
| --- | --- | --- |
| Windows 64 位 | `Windows/Release/Swiftwing NAS.exe` | 保留 `Release` 文件夹中的 DLL，一起复制到需要使用的电脑；双击 EXE。 |
| iPhone / iPad | `Apple/SwiftwingNAS.xcodeproj` | 在 Mac 的 Xcode 中选择 iOS target、签名团队和设备，然后运行安装。 |
| Mac | `Apple/SwiftwingNAS.xcodeproj` | 在 Mac 的 Xcode 中选择 macOS target，然后构建运行。 |

默认地址为 `http://swiftwingnas.tplinkdns.com/`。如果家中路由器不能从局域网访问该域名，可以在 App 的服务器设置中改为 Master 的局域网 IP，例如 `http://192.168.1.50/`。连接恢复热点 `Swiftwing-Bridge` 后可用 `http://192.168.4.1/`。Windows 和苹果端均提供地址设置及重试入口。

## 设计与兼容性

- 原始 V1.9 压缩包是五块 ESP32 板的固件和 Master 内嵌网页，没有原生应用工程。本项目新增独立客户端，没有改动固件与 API。
- App 加载设备上实时提供的网页；Master 网页更新后，App 会显示更新后的页面。登录 Cookie 由系统 WebView 保存，地址设置不保存密码。
- Windows 顶栏精简为品牌和随窗口伸缩的连接状态胶囊。胶囊有鼠标跟随高光，显示当前 NAS 地址与连接状态，点击后可重连、切换常用地址或设备热点、修改地址。网页滚动条隐藏；小窗口登录页会收紧布局并缩放到可视高度，长文件列表仍可用鼠标滚轮浏览。
- 登录失败提示限制为两行并降低红色饱和度；完整服务器错误保留在提示的悬停文本中，便于排查。
- 现有网页直接向存储节点的局域网 IP 上传、下载和管理文件。通过外网域名仅连接到 Master 时，文件操作可能无法抵达该私网 IP；请在同一局域网或能访问节点的 VPN 下使用。
- 当前固件使用 HTTP，登录与文件传输没有 TLS 加密。应在可信网络或 VPN 中使用。
- Windows 客户端需要 Microsoft Edge WebView2 Runtime。当前制作环境已安装；其他电脑如果缺少，可从微软安装。请复制整个 `Windows/Release` 文件夹，不要只复制 EXE。
- Windows 源码已在本机成功编译，EXE、图标和依赖文件已检查。在线地址的 `/login.html` 当前返回 HTTP 200，但 `/ui-version` 返回 `V1.2.9 COBALT MOTION FAN`，与本次提供的 V1.9 压缩包版本标记不一致；尚未提交账号密码，因此真实登录、上传和下载仍待在目标 NAS 上验证。用于视觉测试的本机静态预览不提供登录 POST 接口，必须从 EXE 默认地址或设备地址连接真实 NAS 才能登录。
- Apple 工程需要在 Mac 的 Xcode 中构建和签名。这里没有 Mac 或已连接的 ESP32，故苹果应用及真实硬件的上传下载尚未实机验证。

## 建议的实机验收

1. 连接与 Master 相同的局域网，打开 App，并登录。
2. 浏览文件夹，上传一个小文件，再下载并检查内容。
3. 测试新建、重命名和删除的确认/输入对话框，以及系统状态。
4. 在各端断开网络后确认错误与重试入口；连接 `Swiftwing-Bridge` 后测试恢复地址。

## 源码

- Windows：`Windows/Program.cs`、`Windows/build.ps1`。
- Apple：`Apple/SwiftwingNAS` 和 Xcode 工程。
- 图标：`Assets`，依据 V1.9 登录页中的轨道和箭头图形制作。
