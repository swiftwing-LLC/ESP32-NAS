import SwiftUI

#if os(macOS)
import AppKit
#endif

struct ContentView: View {
    @AppStorage("nasAddress") private var nasAddress = NASBrowser.defaultAddress
    @StateObject private var browser = NASBrowser()
    @State private var showingAddressSettings = false

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                Image(systemName: "externaldrive.connected.to.line.below")
                    .foregroundStyle(.blue)
                VStack(alignment: .leading, spacing: 2) {
                    Text("Swiftwing NAS").font(.headline)
                    Text(browser.currentHost ?? "正在连接…")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                }
                Spacer(minLength: 8)
                if browser.isLoading {
                    ProgressView().controlSize(.small)
                }
                Button {
                    browser.reload()
                } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .help("重新加载")
                Button {
                    showingAddressSettings = true
                } label: {
                    Image(systemName: "gearshape")
                }
                .help("NAS 地址")
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 9)

            Divider()

            ZStack {
                NASWebView(browser: browser)

                if let message = browser.connectionError {
                    VStack(spacing: 12) {
                        Image(systemName: "wifi.exclamationmark")
                            .font(.system(size: 32))
                        Text("无法连接 NAS").font(.title3.bold())
                        Text(message)
                            .multilineTextAlignment(.center)
                            .foregroundStyle(.secondary)
                        HStack {
                            Button("重试") { browser.reload() }
                                .buttonStyle(.borderedProminent)
                            Button("连接恢复热点") {
                                browser.load(NASBrowser.recoveryAddress)
                            }
                            .buttonStyle(.bordered)
                        }
                        Text("使用恢复入口时，请先连接 Swiftwing-Bridge Wi-Fi。")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .padding(24)
                    .frame(maxWidth: 420)
                    .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 18))
                    .padding(16)
                }
            }

            if let file = browser.lastDownloadedFile {
                Divider()
                HStack(spacing: 10) {
                    Image(systemName: "arrow.down.circle.fill")
                        .foregroundStyle(.green)
                    Text("已下载：\(file.lastPathComponent)")
                        .lineLimit(1)
                        .truncationMode(.middle)
                    Spacer(minLength: 4)
                    #if os(iOS)
                    ShareLink(item: file) {
                        Label("分享", systemImage: "square.and.arrow.up")
                    }
                    #elseif os(macOS)
                    Button("显示文件") {
                        NSWorkspace.shared.activateFileViewerSelecting([file])
                    }
                    #endif
                    Button {
                        browser.lastDownloadedFile = nil
                    } label: {
                        Image(systemName: "xmark")
                    }
                    .buttonStyle(.plain)
                    .help("关闭下载提示")
                }
                .font(.caption)
                .padding(10)
            }

            if let notice = browser.downloadError {
                Divider()
                HStack {
                    Text(notice).foregroundStyle(.red)
                    Spacer()
                    Button("关闭") { browser.downloadError = nil }
                }
                .font(.caption)
                .padding(10)
            }
        }
        .frame(minWidth: 320, minHeight: 420)
        .onAppear {
            browser.loadIfNeeded(nasAddress)
        }
        .sheet(isPresented: $showingAddressSettings) {
            AddressSettingsView(address: $nasAddress) { address in
                browser.load(address)
            }
        }
    }
}

private struct AddressSettingsView: View {
    @Binding var address: String
    let connect: (String) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var draft: String
    @State private var validationError = false

    init(address: Binding<String>, connect: @escaping (String) -> Void) {
        _address = address
        self.connect = connect
        _draft = State(initialValue: address.wrappedValue)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("NAS 地址").font(.title2.bold())
            TextField("http://swiftwingnas.tplinkdns.com", text: $draft)
                .textFieldStyle(.roundedBorder)
                #if os(iOS)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)
                #endif

            if validationError {
                Text("请输入有效的 http:// 或 https:// 地址。")
                    .foregroundStyle(.red)
                    .font(.caption)
            }

            Text("常用地址：\(NASBrowser.defaultAddress)")
                .font(.caption)
                .foregroundStyle(.secondary)
            Text("恢复入口：\(NASBrowser.recoveryAddress)（先连接 Swiftwing-Bridge Wi-Fi）")
                .font(.caption)
                .foregroundStyle(.secondary)
            Text("当前 NAS 固件通过 HTTP 登录；请在可信的局域网或 VPN 内使用。")
                .font(.caption)
                .foregroundStyle(.secondary)

            HStack {
                Button("使用常用地址") { draft = NASBrowser.defaultAddress }
                Spacer()
                Button("取消") { dismiss() }
                Button("保存并连接") {
                    guard let url = NASBrowser.normalizedURL(from: draft) else {
                        validationError = true
                        return
                    }
                    address = url.absoluteString
                    connect(address)
                    dismiss()
                }
                .buttonStyle(.borderedProminent)
            }
        }
        .padding(20)
        .frame(maxWidth: 480)
    }
}
