# Swiftwing NAS Windows 应用

双击 [Release/Swiftwing NAS.exe](Release/Swiftwing%20NAS.exe) 启动。请把 `Release` 文件夹中的 EXE 和三个 DLL 保持在同一目录。应用直接连接 V1.9 Master 提供的网页界面，所以 NAS 必须在网络上可访问。

## 使用

- 首次启动默认打开 `http://swiftwingnas.tplinkdns.com/`。
- 顶栏只保留 Swiftwing 品牌和连接状态胶囊。胶囊随窗口伸缩，显示当前 NAS 地址和连接状态；鼠标悬停时液态玻璃高光跟随指针，点击胶囊会打开连接选项。
- 连接选项提供重新连接、连接常用地址、连接设备热点和更改 NAS 地址；设备热点地址为 `http://192.168.4.1/`。
- 网页滚动条隐藏。小窗口中的登录页会收紧布局并缩放到可视高度；文件列表较长时仍可用鼠标滚轮浏览。
- 文件选择使用 WebView2 的系统文件选择器；下载会弹出 Windows 保存对话框；网页的 JavaScript 提示框由 WebView2 显示。
- 网页跳转到 NAS 节点的局域网地址时，仍在应用内打开。

主页地址保存在 `%APPDATA%\Swiftwing NAS\settings.txt`，文件只含主页 URL。WebView2 的网站 Cookie、缓存等浏览器数据由运行时保存在 `%LOCALAPPDATA%\Swiftwing NAS\WebView2`；应用关闭密码自动保存与通用自动填充。应用代码不保存账号或密码。

## 系统要求

- Windows 10/11 x64
- .NET Framework 4.8
- Microsoft Edge WebView2 Runtime（此电脑已安装；其他电脑可从 [Microsoft 官方页面](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)安装）

此版本按原设备地址使用 HTTP。经过公共网络登录时，HTTP 本身不加密；应由设备端提供 HTTPS 或通过可信 VPN 访问。应用仅承载设备当前提供的网页，不会在断网时替代 NAS 服务。

## 重新构建

在 PowerShell 中运行：

```powershell
& '.\build.ps1' -Clean
```

`build.ps1` 使用本机 .NET Framework 编译器和固定版本 `Microsoft.Web.WebView2 1.0.4258.31`。若同目录没有 NuGet 包，会从官方 NuGet 下载。共享的 `..\Assets\swiftwing.ico` 存在时，会嵌入 EXE。输出在 `Release` 文件夹；该文件夹只包含 EXE、运行所需 DLL 和 WebView2 SDK 许可证。源码在 `Program.cs`，WebView2 SDK 的再分发条款见 `Release/WEBVIEW2-SDK-LICENSE.txt`。

可选：命令行传入一个 HTTP/HTTPS 地址用于临时打开页面，例如 `& '.\Release\Swiftwing NAS.exe' 'http://127.0.0.1:8765/'`。此参数不会保存。
