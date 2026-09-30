# Swiftwing NAS Apple 客户端

这个 Xcode 工程包含两个原生应用目标：**Swiftwing NAS iOS**（iPhone、iPad）和 **Swiftwing NAS macOS**（Mac）。应用使用系统 WebKit 显示 NAS V1.9 固件提供的原有界面，不需要修改或重新刷写 NAS。登录、文件管理、系统状态和 Wi-Fi 页面仍由 NAS 提供。

## 在 Mac 上构建与安装

1. 将整个 `Apple` 文件夹复制到 Mac，双击 `SwiftwingNAS.xcodeproj`，使用 Xcode 15 或更新版本打开。
2. 在顶部 Scheme 菜单选择 **Swiftwing NAS iOS** 或 **Swiftwing NAS macOS**。
3. iPhone/iPad：在 Xcode 的目标设置中选择自己的 Team，并根据需要修改 Bundle Identifier；连接设备，选择设备后运行。Xcode 会安装应用。真机安装需要设备信任开发者并按系统提示启用开发者模式。
4. Mac：选择 **My Mac** 并运行。需要独立使用时，可从 Xcode 构建产物中复制 `Swiftwing NAS.app` 到“应用程序”文件夹。

此仓库在 Windows 环境中制作，无法在这里运行 Xcode、签名或生成 `.ipa` / `.app` 成品。首次在 Mac 上构建和在实机上测试仍是必需步骤。iOS 最低版本为 16，macOS 最低版本为 13。

## 连接方式

- 默认地址：`http://swiftwingnas.tplinkdns.com`
- 点击应用右上角齿轮，可修改并保存 Master 的地址。只需输入主机名或完整 `http://` / `https://` 地址；应用会从根路径打开。
- 若家庭 Wi-Fi 不可用，先将设备连接到 `Swiftwing-Bridge`，然后在连接错误界面点击“连接恢复热点”，访问 `http://192.168.4.1`。这个临时恢复入口不会覆盖保存的常用地址。
- 手机或电脑必须能连到 NAS。使用域名时，局域网 DNS 或远程访问配置必须正常；使用恢复地址时，设备必须连接恢复热点。

## 文件与登录

- 登录和会话由 NAS 网页及 WebKit 的持久化网站数据管理；应用没有内置账号或密码，也不会自行保存密码。
- Mac 的上传按钮打开系统文件选择器；下载保存在当前用户的“下载”文件夹，并可从底部提示定位。
- iPhone/iPad 的上传按钮使用系统文件选择器；下载保存在“文件”应用 →“我的 iPhone/iPad”→“Swiftwing NAS”→`Downloads`，也可在底部提示中点击“分享”。
- 网页的重命名、新建文件夹、删除等操作所用的 JavaScript 输入框和确认框由应用显示为系统对话框。

V1.9 固件以 HTTP 提供登录页面。账号密码和文件流量在网络上不加密；请在可信的局域网或 VPN 中使用。项目的 ATS 例外仅应用于 WebKit 网页流量，以允许当前固件的 HTTP 域名和存储节点地址。若日后固件支持 HTTPS，可改为 HTTPS 并收紧 `Info-*.plist` 中的例外。
