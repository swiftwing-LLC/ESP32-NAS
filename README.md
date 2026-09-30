# Swiftwing NAS 文件目录

想打开或分享 Windows 应用：使用 [`Apps/Windows/单文件版/Swiftwing NAS.exe`](Apps/Windows/单文件版/Swiftwing%20NAS.exe)。只需复制这一个 EXE，保存后双击即可；接收方电脑仍需 .NET Framework 4.8、Microsoft Edge WebView2 Runtime，并能连接到 NAS。旧版五文件 ZIP 保留在 `Apps/Windows`，供已有用户继续使用。

| 文件夹 | 内容 |
| --- | --- |
| [`Apps`](Apps) | 按 Windows、macOS、iOS、Linux 分开的应用入口和状态。 |
| [`Firmware`](Firmware) | V1.9 固件源码、C3 修复包和旧版源码归档。 |
| [`hardware`](hardware) | 三个不同版本的 Power5 PCB 工程，保留可编辑设计。 |
| [`Documents`](Documents) | 接线图、网络检查、论文和作品集等资料。 |
| [`Developer`](Developer) | 应用源码、构建脚本、图标生成脚本及整理前文件哈希清单。平时打开应用不用进入这里。 |

Windows 已有单文件 EXE；Linux 已有 x64/ARM64 的 `.deb` 和单文件 AppImage；macOS 已有 Apple Silicon/Intel 的 `.app` 压缩包；iOS 已有主屏幕 WebClip 安装文件，原生 `.ipa` 仍需要 Xcode 与 Apple 签名。各平台的选择与安装说明见 [`Apps/README.md`](Apps/README.md)。Linux/Mac 包已核验结构，但当前电脑无法完成这些系统的实机验收。

固件源码、硬件工程和 Python 生成脚本是后续修改所需的原稿，不属于 Windows 应用运行文件。可再生成的临时预览、编译缓存和确认相同的重复包集中在 [`Developer/待清理_2026-09-30`](Developer/待清理_2026-09-30)；自动执行策略阻止了本轮直接删除，因此这些文件仍占磁盘空间。
