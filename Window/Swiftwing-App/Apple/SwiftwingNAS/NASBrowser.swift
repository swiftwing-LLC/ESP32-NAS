import Foundation
import SwiftUI
import WebKit

#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

final class NASBrowser: NSObject, ObservableObject {
    static let defaultAddress = "http://swiftwingnas.tplinkdns.com"
    static let recoveryAddress = "http://192.168.4.1"

    @Published var isLoading = false
    @Published var currentHost: String?
    @Published var connectionError: String?
    @Published var lastDownloadedFile: URL?
    @Published var downloadError: String?

    let webView: WKWebView
    private var lastRequestedBaseURL: URL?
    private var downloadDestinations: [ObjectIdentifier: URL] = [:]

    override init() {
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = .default()
        webView = WKWebView(frame: .zero, configuration: configuration)
        super.init()
        webView.navigationDelegate = self
        webView.uiDelegate = self
        #if os(iOS)
        webView.allowsBackForwardNavigationGestures = true
        #endif
    }

    static func normalizedURL(from raw: String) -> URL? {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return nil }
        let withScheme = trimmed.contains("://") ? trimmed : "http://" + trimmed
        guard var parts = URLComponents(string: withScheme),
              let scheme = parts.scheme?.lowercased(),
              scheme == "http" || scheme == "https",
              let host = parts.host, !host.isEmpty,
              parts.user == nil, parts.password == nil else { return nil }
        // V1.9 serves its login and API routes from the Master origin root.
        parts.path = "/"
        parts.query = nil
        parts.fragment = nil
        return parts.url
    }

    func loadIfNeeded(_ address: String) {
        if webView.url == nil { load(address) }
    }

    func load(_ address: String) {
        guard let url = Self.normalizedURL(from: address) else {
            connectionError = "NAS 地址无效，请在设置中修改。"
            return
        }
        connectionError = nil
        lastRequestedBaseURL = url
        currentHost = url.host
        webView.load(URLRequest(url: url))
    }

    func reload() {
        let retryURL = connectionError == nil ? (webView.url ?? lastRequestedBaseURL)
            : (lastRequestedBaseURL ?? webView.url)
        connectionError = nil
        if let retryURL {
            webView.load(URLRequest(url: retryURL))
        } else {
            load(UserDefaults.standard.string(forKey: "nasAddress") ?? Self.defaultAddress)
        }
    }

    private func prepareDownload(_ download: WKDownload) {
        download.delegate = self
        downloadError = nil
    }

    private static func freeDestination(for suggestedFilename: String) throws -> URL {
        let manager = FileManager.default
        #if os(macOS)
        let directory = manager.urls(for: .downloadsDirectory, in: .userDomainMask)[0]
        #else
        let directory = manager.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Downloads", isDirectory: true)
        #endif
        try manager.createDirectory(at: directory, withIntermediateDirectories: true)

        let lastComponent = (suggestedFilename as NSString).lastPathComponent
        let cleaned = lastComponent
            .replacingOccurrences(of: "\\", with: "_")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        let name = cleaned.isEmpty || cleaned == "." || cleaned == ".." ? "download" : cleaned
        let file = directory.appendingPathComponent(name, isDirectory: false)
        if !manager.fileExists(atPath: file.path) { return file }

        let ext = file.pathExtension
        let stem = file.deletingPathExtension().lastPathComponent
        var suffix = 2
        while true {
            let candidateName = ext.isEmpty ? "\(stem) (\(suffix))" : "\(stem) (\(suffix)).\(ext)"
            let candidate = directory.appendingPathComponent(candidateName, isDirectory: false)
            if !manager.fileExists(atPath: candidate.path) { return candidate }
            suffix += 1
        }
    }
}

extension NASBrowser: WKNavigationDelegate {
    func webView(_ webView: WKWebView, didStartProvisionalNavigation navigation: WKNavigation!) {
        isLoading = true
        connectionError = nil
        currentHost = webView.url?.host ?? currentHost
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        isLoading = false
        connectionError = nil
        currentHost = webView.url?.host ?? currentHost
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
        navigationFailed(error)
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
        navigationFailed(error)
    }

    private func navigationFailed(_ error: Error) {
        isLoading = false
        if (error as NSError).code == NSURLErrorCancelled { return }
        connectionError = error.localizedDescription
    }

    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction,
                 decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        decisionHandler(navigationAction.shouldPerformDownload ? .download : .allow)
    }

    func webView(_ webView: WKWebView, decidePolicyFor navigationResponse: WKNavigationResponse,
                 decisionHandler: @escaping (WKNavigationResponsePolicy) -> Void) {
        let response = navigationResponse.response as? HTTPURLResponse
        let disposition = response?.value(forHTTPHeaderField: "Content-Disposition")?.lowercased() ?? ""
        let attachment = disposition.contains("attachment")
        let shouldDownload = navigationResponse.isForMainFrame &&
            (attachment || !navigationResponse.canShowMIMEType)
        decisionHandler(shouldDownload ? .download : .allow)
    }

    func webView(_ webView: WKWebView, navigationAction: WKNavigationAction,
                 didBecome download: WKDownload) {
        prepareDownload(download)
    }

    func webView(_ webView: WKWebView, navigationResponse: WKNavigationResponse,
                 didBecome download: WKDownload) {
        prepareDownload(download)
    }
}

extension NASBrowser: WKDownloadDelegate {
    func download(_ download: WKDownload, decideDestinationUsing response: URLResponse,
                  suggestedFilename: String, completionHandler: @escaping (URL?) -> Void) {
        do {
            let destination = try Self.freeDestination(for: suggestedFilename)
            downloadDestinations[ObjectIdentifier(download)] = destination
            completionHandler(destination)
        } catch {
            downloadError = "无法保存下载：\(error.localizedDescription)"
            completionHandler(nil)
        }
    }

    func downloadDidFinish(_ download: WKDownload) {
        lastDownloadedFile = downloadDestinations.removeValue(forKey: ObjectIdentifier(download))
        isLoading = false
    }

    func download(_ download: WKDownload, didFailWithError error: Error, resumeData: Data?) {
        downloadDestinations.removeValue(forKey: ObjectIdentifier(download))
        downloadError = "下载失败：\(error.localizedDescription)"
        isLoading = false
    }
}

extension NASBrowser: WKUIDelegate {
    func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration,
                 for navigationAction: WKNavigationAction, windowFeatures: WKWindowFeatures) -> WKWebView? {
        if navigationAction.targetFrame == nil {
            webView.load(navigationAction.request)
        }
        return nil
    }

    #if os(macOS)
    func webView(_ webView: WKWebView, runOpenPanelWith parameters: WKOpenPanelParameters,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping ([URL]?) -> Void) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = parameters.allowsMultipleSelection
        panel.begin { result in
            completionHandler(result == .OK ? panel.urls : nil)
        }
    }

    private func show(_ alert: NSAlert, in webView: WKWebView,
                      completion: @escaping (NSApplication.ModalResponse) -> Void) {
        if let window = webView.window {
            alert.beginSheetModal(for: window, completionHandler: completion)
        } else {
            completion(alert.runModal())
        }
    }

    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
        let alert = NSAlert()
        alert.messageText = frame.request.url?.host ?? "Swiftwing NAS"
        alert.informativeText = message
        alert.addButton(withTitle: "好")
        show(alert, in: webView) { _ in completionHandler() }
    }

    func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (Bool) -> Void) {
        let alert = NSAlert()
        alert.messageText = frame.request.url?.host ?? "Swiftwing NAS"
        alert.informativeText = message
        alert.addButton(withTitle: "确定")
        alert.addButton(withTitle: "取消")
        show(alert, in: webView) { result in completionHandler(result == .alertFirstButtonReturn) }
    }

    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String,
                 defaultText: String?, initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping (String?) -> Void) {
        let alert = NSAlert()
        alert.messageText = frame.request.url?.host ?? "Swiftwing NAS"
        alert.informativeText = prompt
        let field = NSTextField(string: defaultText ?? "")
        field.frame = NSRect(x: 0, y: 0, width: 320, height: 24)
        alert.accessoryView = field
        alert.addButton(withTitle: "确定")
        alert.addButton(withTitle: "取消")
        show(alert, in: webView) { result in
            completionHandler(result == .alertFirstButtonReturn ? field.stringValue : nil)
        }
    }
    #elseif os(iOS)
    private func presenter(for webView: WKWebView) -> UIViewController? {
        var controller = webView.window?.rootViewController
        while let presented = controller?.presentedViewController { controller = presented }
        return controller
    }

    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
        guard let presenter = presenter(for: webView) else { completionHandler(); return }
        let alert = UIAlertController(title: frame.request.url?.host ?? "Swiftwing NAS",
                                      message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "好", style: .default) { _ in completionHandler() })
        presenter.present(alert, animated: true)
    }

    func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (Bool) -> Void) {
        guard let presenter = presenter(for: webView) else { completionHandler(false); return }
        let alert = UIAlertController(title: frame.request.url?.host ?? "Swiftwing NAS",
                                      message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "取消", style: .cancel) { _ in completionHandler(false) })
        alert.addAction(UIAlertAction(title: "确定", style: .default) { _ in completionHandler(true) })
        presenter.present(alert, animated: true)
    }

    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String,
                 defaultText: String?, initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping (String?) -> Void) {
        guard let presenter = presenter(for: webView) else { completionHandler(nil); return }
        let alert = UIAlertController(title: frame.request.url?.host ?? "Swiftwing NAS",
                                      message: prompt, preferredStyle: .alert)
        alert.addTextField { $0.text = defaultText }
        alert.addAction(UIAlertAction(title: "取消", style: .cancel) { _ in completionHandler(nil) })
        alert.addAction(UIAlertAction(title: "确定", style: .default) { _ in
            completionHandler(alert.textFields?.first?.text)
        })
        presenter.present(alert, animated: true)
    }
    #endif
}

#if os(iOS)
struct NASWebView: UIViewRepresentable {
    let browser: NASBrowser

    func makeUIView(context: Context) -> WKWebView { browser.webView }
    func updateUIView(_ uiView: WKWebView, context: Context) {}
}
#elseif os(macOS)
struct NASWebView: NSViewRepresentable {
    let browser: NASBrowser

    func makeNSView(context: Context) -> WKWebView { browser.webView }
    func updateNSView(_ nsView: WKWebView, context: Context) {}
}
#endif
