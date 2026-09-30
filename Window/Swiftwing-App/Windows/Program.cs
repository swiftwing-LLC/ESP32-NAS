using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Globalization;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

namespace SwiftwingNas
{
    internal static class Program
    {
        [STAThread]
        private static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }

    internal sealed class MainForm : Form
    {
        private const string DefaultBaseUrl = "http://swiftwingnas.tplinkdns.com/";
        private const string AccessPointUrl = "http://192.168.4.1/";
        private const string NativePagePolishScript =
            "(function(){function apply(){try{if(!document.documentElement){setTimeout(apply,0);return;}" +
            "if(document.getElementById('swiftwing-native-polish'))return;" +
            "var s=document.createElement('style');s.id='swiftwing-native-polish';" +
            "s.textContent='html,body,*{scrollbar-width:none!important}' +" +
            "'*::-webkit-scrollbar{display:none!important;width:0!important;height:0!important;background:transparent!important}' +" +
            "'html{overscroll-behavior:none!important}' +" +
            "'html.swiftwing-native-compact .wrap{min-height:100vh;padding:8px;box-sizing:border-box}' +" +
            "'html.swiftwing-native-compact .card{min-height:0;grid-template-columns:1fr;max-width:520px}' +" +
            "'html.swiftwing-native-compact .loginPlate{display:none!important}' +" +
            "'html.swiftwing-native-compact .loginForm{padding:14px 24px}' +" +
            "'html.swiftwing-native-compact .formSub{margin-bottom:8px;font-size:11px}' +" +
            "'html.swiftwing-native-compact .authModes{margin-bottom:6px}' +" +
            "'html.swiftwing-native-compact .meta{display:none!important}' +" +
            "'html.swiftwing-native-compact .authForm label{margin:7px 0 3px}' +" +
            "'html.swiftwing-native-compact .authForm input{min-height:34px;padding:7px 10px}' +" +
            "'html.swiftwing-native-compact .authForm button{min-height:34px;margin-top:9px}' +" +
            "'html.swiftwing-native-tiny .formSub,html.swiftwing-native-tiny .formEyebrow{display:none!important}' +" +
            "'html.swiftwing-native-tiny .formTitle{margin:0 0 5px;font-size:20px}' +" +
            "'html.swiftwing-native-tiny .loginForm{padding:10px 20px}' +" +
            "'html.swiftwing-native-tiny .authModes{margin-bottom:4px}' +" +
            "'html.swiftwing-native-tiny .authForm label{margin:5px 0 2px}' +" +
            "'html.swiftwing-native-tiny .authForm input{min-height:30px;padding:5px 9px}' +" +
            "'html.swiftwing-native-tiny .authForm button{min-height:32px;margin-top:6px}' +" +
            "'.authMessage[data-kind=\"error\"]{color:#d28b96!important;max-height:2.9em;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;overflow-wrap:anywhere}' +" +
            "':root[data-theme=\"light\"] .authMessage[data-kind=\"error\"]{color:#a94755!important}';" +
            "(document.head||document.documentElement).appendChild(s);" +
            "function watchErrors(){var m=document.getElementById('authMessage');if(!m){setTimeout(watchErrors,50);return;}" +
            "var update=function(){if(m.dataset.kind==='error'){m.title=m.textContent||'';}else{m.removeAttribute('title');}};" +
            "new MutationObserver(update).observe(m,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:['data-kind']});update();}" +
            "watchErrors();}catch(e){}}apply();})();";

        private readonly Color _navy = Color.FromArgb(19, 32, 46);
        private readonly Color _panel = Color.FromArgb(29, 45, 61);
        private readonly Color _cyan = Color.FromArgb(46, 194, 215);
        private readonly WebView2 _webView;
        private readonly Label _status;
        private readonly Panel _cover;
        private readonly Label _coverTitle;
        private readonly Label _coverText;
        private readonly ConnectionPill _connectionPill;
        private readonly System.Windows.Forms.Timer _fitTimer;
        private readonly ContextMenuStrip _quickMenu;

        private string _baseUrl;
        private string _lastAttemptedUrl;
        private Uri _lastGoodUri;
        private bool _ready;
        private bool _hasSuccessfulPage;
        private bool _downloadStarted;
        private bool _connected;

        private static string SettingsPath
        {
            get
            {
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "Swiftwing NAS", "settings.txt");
            }
        }

        public MainForm()
        {
            _baseUrl = ReadBaseUrl();
            _lastAttemptedUrl = _baseUrl;

            Text = "Swiftwing NAS";
            Size = new Size(1160, 760);
            MinimumSize = new Size(820, 520);
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = _navy;
            Font = new Font("Microsoft YaHei UI", 9F);
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); }
            catch { /* The embedded EXE icon is optional during source builds. */ }

            TableLayoutPanel shell = new TableLayoutPanel();
            shell.Dock = DockStyle.Fill;
            shell.BackColor = _navy;
            shell.Margin = Padding.Empty;
            shell.Padding = Padding.Empty;
            shell.RowCount = 3;
            shell.ColumnCount = 1;
            shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 72));
            shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
            shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            Controls.Add(shell);

            GlassToolbar toolbar = new GlassToolbar();
            toolbar.Dock = DockStyle.Fill;
            toolbar.BackColor = Color.FromArgb(18, 31, 49);
            toolbar.Margin = Padding.Empty;
            shell.Controls.Add(toolbar, 0, 0);

            Panel brand = new Panel();
            brand.Margin = Padding.Empty;
            brand.BackColor = Color.Transparent;
            PictureBox brandIcon = new PictureBox();
            brandIcon.Size = new Size(34, 34);
            brandIcon.SizeMode = PictureBoxSizeMode.Zoom;
            try
            {
                using (Icon icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath))
                    if (icon != null) brandIcon.Image = icon.ToBitmap();
            }
            catch { }
            brand.Controls.Add(brandIcon);
            Label brandTitle = new Label();
            brandTitle.Text = "SWIFTWING";
            brandTitle.ForeColor = Color.FromArgb(228, 239, 255);
            brandTitle.Font = new Font("Segoe UI", 10F, FontStyle.Bold);
            brandTitle.TextAlign = ContentAlignment.MiddleLeft;
            brand.Controls.Add(brandTitle);
            brand.Resize += delegate
            {
                brandIcon.SetBounds(0, (brand.ClientSize.Height - brandIcon.Height) / 2,
                    brandIcon.Width, brandIcon.Height);
                brandTitle.SetBounds(42, 0, Math.Max(0, brand.ClientSize.Width - 42), brand.ClientSize.Height);
            };
            toolbar.Controls.Add(brand);

            _connectionPill = new ConnectionPill();
            _connectionPill.Margin = Padding.Empty;
            _connectionPill.Click += delegate { ShowConnectionMenu(); };
            new ToolTip().SetToolTip(_connectionPill, "点击打开连接选项");
            toolbar.Controls.Add(_connectionPill);
            toolbar.Resize += delegate { LayoutToolbar(toolbar, brand); };
            LayoutToolbar(toolbar, brand);

            _quickMenu = CreateQuickMenu();
            _connectionPill.HostLabel = GetHostLabel(_baseUrl);
            _connectionPill.StatusLabel = "等待连接";
            _connectionPill.IsSelected = true;

            Panel content = new Panel();
            content.Dock = DockStyle.Fill;
            content.BackColor = Color.White;
            content.Margin = Padding.Empty;
            shell.Controls.Add(content, 0, 1);

            _webView = new WebView2();
            _webView.Dock = DockStyle.Fill;
            _webView.DefaultBackgroundColor = Color.White;
            content.Controls.Add(_webView);

            _fitTimer = new System.Windows.Forms.Timer();
            _fitTimer.Interval = 220;
            _fitTimer.Tick += async delegate
            {
                _fitTimer.Stop();
                await FitLoginPageAsync();
            };
            _webView.Resize += delegate { ScheduleLoginFit(); };

            _cover = new Panel();
            _cover.Dock = DockStyle.Fill;
            _cover.BackColor = Color.FromArgb(238, 246, 248);
            content.Controls.Add(_cover);
            _cover.BringToFront();

            TableLayoutPanel coverLayout = new TableLayoutPanel();
            coverLayout.Dock = DockStyle.Fill;
            coverLayout.RowCount = 5;
            coverLayout.ColumnCount = 1;
            coverLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 36));
            coverLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 54));
            coverLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 84));
            coverLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 55));
            coverLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 64));
            coverLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            _cover.Controls.Add(coverLayout);

            _coverTitle = new Label();
            _coverTitle.Text = "正在连接 Swiftwing NAS";
            _coverTitle.Font = new Font("Microsoft YaHei UI", 19F, FontStyle.Bold);
            _coverTitle.ForeColor = _navy;
            _coverTitle.TextAlign = ContentAlignment.MiddleCenter;
            _coverTitle.Dock = DockStyle.Fill;
            coverLayout.Controls.Add(_coverTitle, 0, 1);

            _coverText = new Label();
            _coverText.Text = "请稍候…";
            _coverText.Font = new Font("Microsoft YaHei UI", 10F);
            _coverText.ForeColor = Color.FromArgb(75, 94, 108);
            _coverText.TextAlign = ContentAlignment.TopCenter;
            _coverText.Dock = DockStyle.Fill;
            _coverText.AutoEllipsis = true;
            coverLayout.Controls.Add(_coverText, 0, 2);

            FlowLayoutPanel recovery = new FlowLayoutPanel();
            recovery.Dock = DockStyle.Fill;
            recovery.FlowDirection = FlowDirection.LeftToRight;
            recovery.WrapContents = false;
            recovery.AutoSize = false;
            recovery.Anchor = AnchorStyles.None;
            recovery.Width = 310;
            recovery.Height = 45;
            Button retry = RecoveryButton("重试");
            retry.Click += delegate { Retry(); };
            Button ap = RecoveryButton("设备热点");
            ap.Click += delegate { Navigate(AccessPointUrl); };
            Button change = RecoveryButton("修改地址");
            change.Click += delegate { EditServer(); };
            recovery.Controls.Add(retry);
            recovery.Controls.Add(ap);
            recovery.Controls.Add(change);
            coverLayout.Controls.Add(recovery, 0, 3);

            _status = new Label();
            _status.Text = "准备连接";
            _status.ForeColor = Color.FromArgb(181, 201, 211);
            _status.BackColor = _panel;
            _status.Padding = new Padding(14, 0, 0, 0);
            _status.TextAlign = ContentAlignment.MiddleLeft;
            _status.Dock = DockStyle.Fill;
            _status.Margin = Padding.Empty;
            shell.Controls.Add(_status, 0, 2);

            UpdateButtons();
            Shown += async delegate { await InitializeWebViewAsync(); };
        }

        private void LayoutToolbar(GlassToolbar toolbar, Control brand)
        {
            int width = toolbar.ClientSize.Width;
            int height = toolbar.ClientSize.Height;
            if (width <= 0 || height <= 0) return;

            int sideInset = Math.Min(20, Math.Max(12, width / 40));
            int brandWidth = Math.Min(188, Math.Max(180, width / 6));
            int verticalInset = Math.Min(12, Math.Max(6, height / 7));
            int controlHeight = Math.Max(34, height - verticalInset * 2);
            int bandLeft = sideInset + brandWidth + 14;
            int bandRight = width - sideInset;
            int available = Math.Max(0, bandRight - bandLeft);
            int pillWidth = Math.Min(560, available);
            int centeredPillX = (width - pillWidth) / 2;
            int pillX = Math.Max(bandLeft, Math.Min(centeredPillX, Math.Max(bandLeft, bandRight - pillWidth)));

            brand.SetBounds(sideInset, 0, brandWidth, height);
            _connectionPill.SetBounds(pillX, (height - controlHeight) / 2, pillWidth, controlHeight);
            _connectionPill.Enabled = pillWidth >= 240;
        }

        private void ShowConnectionMenu()
        {
            if (_quickMenu != null)
                _quickMenu.Show(_connectionPill, new Point(0, _connectionPill.Height));
        }

        private ContextMenuStrip CreateQuickMenu()
        {
            ContextMenuStrip menu = new ContextMenuStrip();
            menu.BackColor = Color.FromArgb(23, 37, 56);
            menu.ForeColor = Color.FromArgb(225, 236, 255);
            menu.Font = new Font("Microsoft YaHei UI", 9F);
            menu.ShowImageMargin = false;
            menu.Renderer = new GlassMenuRenderer();
            menu.Padding = new Padding(6);
            menu.Items.Add("重新连接", null, delegate { Retry(); });
            menu.Items.Add("连接常用地址", null, delegate { Navigate(_baseUrl); });
            menu.Items.Add("连接设备热点", null, delegate { Navigate(AccessPointUrl); });
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add("更改 NAS 地址…", null, delegate { EditServer(); });
            foreach (ToolStripItem item in menu.Items)
            {
                item.Padding = new Padding(12, 7, 12, 7);
                item.ForeColor = Color.FromArgb(225, 236, 255);
            }
            return menu;
        }

        private LiquidGlassButton RecoveryButton(string text)
        {
            LiquidGlassButton b = new LiquidGlassButton();
            b.Text = text;
            b.Size = new Size(96, 38);
            b.Margin = new Padding(3, 1, 3, 1);
            b.Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular);
            b.ForeColor = Color.FromArgb(226, 237, 255);
            b.Cursor = Cursors.Hand;
            return b;
        }

        private async Task InitializeWebViewAsync()
        {
            try
            {
                string dataFolder = Path.Combine(Environment.GetFolderPath(
                    Environment.SpecialFolder.LocalApplicationData), "Swiftwing NAS", "WebView2");
                Directory.CreateDirectory(dataFolder);
                CoreWebView2Environment environment = await CoreWebView2Environment.CreateAsync(null, dataFolder);
                await _webView.EnsureCoreWebView2Async(environment);

                CoreWebView2 core = _webView.CoreWebView2;
                core.Settings.AreDefaultScriptDialogsEnabled = true;
                core.Settings.AreDevToolsEnabled = false;
                core.Settings.IsPasswordAutosaveEnabled = false;
                core.Settings.IsGeneralAutofillEnabled = false;
                await core.AddScriptToExecuteOnDocumentCreatedAsync(NativePagePolishScript);
                core.NavigationStarting += delegate(object sender, CoreWebView2NavigationStartingEventArgs e)
                {
                    _downloadStarted = false;
                    if (Math.Abs(_webView.ZoomFactor - 1.0) > 0.005) _webView.ZoomFactor = 1.0;
                    _lastAttemptedUrl = e.Uri;
                    _connected = false;
                    _connectionPill.HostLabel = GetHostLabel(e.Uri);
                    _connectionPill.StatusLabel = "正在连接";
                    _connectionPill.IsConnected = false;
                    _connectionPill.Invalidate();
                    _status.Text = "正在连接 Swiftwing NAS";
                    if (!_hasSuccessfulPage)
                    {
                        _coverTitle.Text = "正在连接 Swiftwing NAS";
                        _coverText.Text = e.Uri;
                        _cover.Visible = true;
                    }
                };
                core.NavigationCompleted += delegate(object sender, CoreWebView2NavigationCompletedEventArgs e)
                {
                    if (e.IsSuccess)
                    {
                        _hasSuccessfulPage = true;
                        _connected = true;
                        _lastGoodUri = _webView.Source;
                        _cover.Visible = false;
                        _status.Text = "已连接  ·  " + _webView.Source;
                        UpdateConnectionPill(_webView.Source == null ? _lastAttemptedUrl : _webView.Source.ToString(), "已连接", true);
                        ScheduleLoginFit();
                    }
                    else if (_downloadStarted && e.WebErrorStatus == CoreWebView2WebErrorStatus.OperationCanceled)
                    {
                        // Attachment responses may cancel navigation after DownloadStarting.
                        _cover.Visible = false;
                    }
                    else
                    {
                        _coverTitle.Text = "无法打开页面";
                        _coverText.Text = "请检查网络和服务器地址，然后重试。\r\n" +
                            _lastAttemptedUrl + "\r\n" + e.WebErrorStatus;
                        _cover.Visible = true;
                        _cover.BringToFront();
                        _status.Text = "连接失败  ·  " + e.WebErrorStatus;
                        _connected = false;
                        UpdateConnectionPill(_lastAttemptedUrl, "连接失败", false);
                    }
                    _downloadStarted = false;
                    UpdateButtons();
                };
                core.SourceChanged += delegate
                {
                    if (!_downloadStarted && _webView.Source != null)
                        _connectionPill.HostLabel = GetHostLabel(_webView.Source.ToString());
                    UpdateButtons();
                    ScheduleLoginFit();
                };
                core.HistoryChanged += delegate { UpdateButtons(); };
                core.DownloadStarting += OnDownloadStarting;
                core.NewWindowRequested += delegate(object sender, CoreWebView2NewWindowRequestedEventArgs e)
                {
                    // Keep ordinary links and NAS node downloads inside this window.
                    e.Handled = true;
                    string target = e.Uri;
                    if (!string.IsNullOrWhiteSpace(target))
                    {
                        BeginInvoke(new Action(delegate { Navigate(target); }));
                    }
                };
                core.ProcessFailed += delegate(object sender, CoreWebView2ProcessFailedEventArgs e)
                {
                    if (e.ProcessFailedKind == CoreWebView2ProcessFailedKind.BrowserProcessExited ||
                        e.ProcessFailedKind == CoreWebView2ProcessFailedKind.RenderProcessExited)
                    {
                        _coverTitle.Text = "浏览器组件需要重新启动";
                        _coverText.Text = "WebView2 进程已停止。请关闭并重新打开 Swiftwing NAS。";
                        _cover.Visible = true;
                        _cover.BringToFront();
                    }
                    _status.Text = "WebView2 进程异常  ·  " + e.ProcessFailedKind;
                    _connected = false;
                    UpdateConnectionPill(_lastAttemptedUrl, "组件异常", false);
                };

                _ready = true;
                UpdateButtons();
                string initial = _baseUrl;
                string[] args = Environment.GetCommandLineArgs();
                string overrideUrl;
                if (args.Length > 1 && TryNavigationUrl(args[1], out overrideUrl)) initial = overrideUrl;
                Navigate(initial);
            }
            catch (Exception ex)
            {
                _coverTitle.Text = "无法启动浏览器组件";
                _coverText.Text = "请安装或修复 Microsoft Edge WebView2 Runtime 后重新打开。\r\n" + ex.Message;
                _cover.Visible = true;
                _cover.BringToFront();
                _status.Text = "WebView2 初始化失败";
            }
        }

        private void OnDownloadStarting(object sender, CoreWebView2DownloadStartingEventArgs e)
        {
            _downloadStarted = true;
            // WebView2 requires a deferral when host UI is shown for this event.
            CoreWebView2Deferral deferral = e.GetDeferral();
            try
            {
                BeginInvoke(new Action(delegate
                {
                    try
                    {
                        using (SaveFileDialog save = new SaveFileDialog())
                        {
                            string proposed = Path.GetFileName(e.ResultFilePath);
                            save.FileName = string.IsNullOrWhiteSpace(proposed) ? "download" : proposed;
                            save.Filter = "所有文件 (*.*)|*.*";
                            save.OverwritePrompt = true;
                            string downloads = Path.Combine(Environment.GetFolderPath(
                                Environment.SpecialFolder.UserProfile), "Downloads");
                            if (Directory.Exists(downloads)) save.InitialDirectory = downloads;
                            if (save.ShowDialog(this) == DialogResult.OK)
                            {
                                e.ResultFilePath = Path.GetFullPath(save.FileName);
                                e.Handled = true;
                                _status.Text = "下载至  " + e.ResultFilePath;
                                if (_lastGoodUri != null)
                                    UpdateConnectionPill(_lastGoodUri.ToString(), "已连接", true);
                            }
                            else
                            {
                                e.Cancel = true;
                                e.Handled = true;
                                _status.Text = "已取消下载";
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        e.Cancel = true;
                        e.Handled = true;
                        _status.Text = "下载失败  ·  " + ex.Message;
                    }
                    finally { deferral.Complete(); }
                }));
            }
            catch
            {
                e.Cancel = true;
                deferral.Complete();
            }
        }

        private void Navigate(string url)
        {
            string valid;
            if (!TryNavigationUrl(url, out valid)) return;
            _lastAttemptedUrl = valid;
            _connected = false;
            UpdateConnectionPill(valid, "正在连接", false);
            _status.Text = "正在连接 Swiftwing NAS";
            if (!_ready)
            {
                _coverTitle.Text = "正在准备浏览器组件";
                _coverText.Text = valid;
                _cover.Visible = true;
                return;
            }
            _cover.Visible = false;
            _webView.CoreWebView2.Navigate(valid);
        }

        private void Retry()
        {
            if (!_ready) return;
            string target = string.IsNullOrWhiteSpace(_lastAttemptedUrl) ? _baseUrl : _lastAttemptedUrl;
            Navigate(target);
        }

        private void EditServer()
        {
            using (ServerDialog dialog = new ServerDialog(_baseUrl))
            {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                _baseUrl = dialog.BaseUrl;
                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath));
                    File.WriteAllText(SettingsPath, _baseUrl, new UTF8Encoding(false));
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, "地址可以在本次使用，但保存失败：\r\n" + ex.Message,
                        "无法保存设置", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
                Navigate(_baseUrl);
            }
        }

        private void UpdateButtons()
        {
            UpdateLocationSelection();
        }

        private void UpdateLocationSelection()
        {
            string current = _lastGoodUri == null ? _lastAttemptedUrl : _lastGoodUri.ToString();
            if (_webView.Source != null && !_downloadStarted) current = _webView.Source.ToString();
            _connectionPill.HostLabel = GetHostLabel(current);
            _connectionPill.IsConnected = _connected;
            _connectionPill.Invalidate();
        }

        private void UpdateConnectionPill(string url, string status, bool connected)
        {
            _connectionPill.HostLabel = GetHostLabel(url);
            _connectionPill.StatusLabel = status;
            _connectionPill.IsConnected = connected;
            _connectionPill.Invalidate();
        }

        private static string GetHostLabel(string value)
        {
            Uri uri;
            if (!Uri.TryCreate(value, UriKind.Absolute, out uri)) return "SWIFTWING NAS";
            return uri.IsDefaultPort ? uri.Host : uri.Host + ":" + uri.Port;
        }

        private void ScheduleLoginFit()
        {
            if (!_ready || _fitTimer == null || IsDisposed) return;
            _fitTimer.Stop();
            _fitTimer.Start();
        }

        private async Task FitLoginPageAsync()
        {
            if (!_ready || _webView.CoreWebView2 == null || _webView.ClientSize.Height <= 0) return;
            try
            {
                int viewportHeight = _webView.ClientSize.Height;
                string script = "(function(){var root=document.documentElement;" +
                    "var login=!!document.querySelector('.loginForm');" +
                    "var vh=" + viewportHeight.ToString(CultureInfo.InvariantCulture) + ";" +
                    "root.classList.toggle('swiftwing-native-compact',login&&vh<560);" +
                    "root.classList.toggle('swiftwing-native-tiny',login&&vh<430);" +
                    "if(!login)return 1.0;" +
                    "var h=Math.max(root.scrollHeight,document.body?document.body.scrollHeight:0);" +
                    "var w=Math.max(root.scrollWidth,document.body?document.body.scrollWidth:0);" +
                    "if(h<1||w<1)return 1.0;" +
                    "return Math.min(1.4,window.innerHeight/h,window.innerWidth/w);})()";
                string result = await _webView.CoreWebView2.ExecuteScriptAsync(script);
                double ratio;
                if (!double.TryParse(result.Trim('"'), NumberStyles.Float, CultureInfo.InvariantCulture, out ratio)) return;
                double target = Math.Max(0.45, Math.Min(1.0, ratio));
                if (Math.Abs(target - _webView.ZoomFactor) >= 0.015) _webView.ZoomFactor = target;
            }
            catch { /* Navigation may replace the document during the resize callback. */ }
        }

        private static string ReadBaseUrl()
        {
            try
            {
                if (File.Exists(SettingsPath))
                {
                    string normalized;
                    if (TryBaseUrl(File.ReadAllText(SettingsPath).Trim(), out normalized))
                        return normalized;
                }
            }
            catch { /* A missing or unreadable setting falls back to the shipped URL. */ }
            return DefaultBaseUrl;
        }

        internal static bool TryNavigationUrl(string value, out string url)
        {
            url = null;
            if (string.IsNullOrWhiteSpace(value)) return false;
            value = value.Trim();
            if (value.IndexOf("://", StringComparison.Ordinal) < 0) value = "http://" + value;
            Uri uri;
            if (!Uri.TryCreate(value, UriKind.Absolute, out uri)) return false;
            if (uri.Scheme != Uri.UriSchemeHttp && uri.Scheme != Uri.UriSchemeHttps) return false;
            if (string.IsNullOrEmpty(uri.Host) || !string.IsNullOrEmpty(uri.UserInfo)) return false;
            url = uri.AbsoluteUri;
            return true;
        }

        internal static bool TryBaseUrl(string value, out string url)
        {
            url = null;
            string navigation;
            if (!TryNavigationUrl(value, out navigation)) return false;
            Uri uri = new Uri(navigation);
            url = uri.GetLeftPart(UriPartial.Authority).TrimEnd('/') + "/";
            return true;
        }

        private sealed class GlassToolbar : Panel
        {
            public GlassToolbar()
            {
                SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            }

            protected override void OnPaintBackground(PaintEventArgs e)
            {
                Rectangle rect = ClientRectangle;
                if (rect.Width <= 0 || rect.Height <= 0) return;
                using (LinearGradientBrush background = new LinearGradientBrush(rect,
                    Color.FromArgb(32, 50, 74), Color.FromArgb(14, 25, 41), LinearGradientMode.Vertical))
                    e.Graphics.FillRectangle(background, rect);
                using (LinearGradientBrush sheen = new LinearGradientBrush(
                    new Rectangle(0, 0, Math.Max(1, rect.Width), Math.Max(1, rect.Height / 2)),
                    Color.FromArgb(24, 184, 211, 255), Color.FromArgb(0, 184, 211, 255), LinearGradientMode.Vertical))
                    e.Graphics.FillRectangle(sheen, new Rectangle(0, 0, rect.Width, rect.Height / 2));
            }

            protected override void OnPaint(PaintEventArgs e)
            {
                base.OnPaint(e);
                using (Pen edge = new Pen(Color.FromArgb(58, 171, 200, 242)))
                    e.Graphics.DrawLine(edge, 0, Height - 1, Width, Height - 1);
            }
        }

        private class LiquidGlassButton : Button
        {
            private bool _hovered;
            private Point _pointer;
            private bool _selected;

            public bool IsSelected
            {
                get { return _selected; }
                set
                {
                    if (_selected == value) return;
                    _selected = value;
                    Invalidate();
                }
            }

            public LiquidGlassButton()
            {
                SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                    ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                    ControlStyles.SupportsTransparentBackColor, true);
                FlatStyle = FlatStyle.Flat;
                FlatAppearance.BorderSize = 0;
                UseVisualStyleBackColor = false;
                BackColor = Color.Transparent;
                TabStop = true;
            }

            protected override void OnMouseEnter(EventArgs e)
            {
                _hovered = true;
                base.OnMouseEnter(e);
                Invalidate();
            }

            protected override void OnMouseLeave(EventArgs e)
            {
                _hovered = false;
                base.OnMouseLeave(e);
                Invalidate();
            }

            protected override void OnMouseMove(MouseEventArgs e)
            {
                _pointer = e.Location;
                base.OnMouseMove(e);
                if (_hovered) Invalidate();
            }

            protected override void OnGotFocus(EventArgs e)
            {
                base.OnGotFocus(e);
                Invalidate();
            }

            protected override void OnLostFocus(EventArgs e)
            {
                base.OnLostFocus(e);
                Invalidate();
            }

            protected override void OnEnabledChanged(EventArgs e)
            {
                base.OnEnabledChanged(e);
                Invalidate();
            }

            protected override void OnPaint(PaintEventArgs e)
            {
                Graphics g = e.Graphics;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                RectangleF box = ClientRectangle;
                box.Inflate(-2.0F, -2.0F);
                if (box.Width <= 2 || box.Height <= 2) return;

                using (GraphicsPath shape = RoundedRectangle(box, Math.Min(11.0F, box.Height / 2.0F)))
                {
                    Color top = _selected ? Color.FromArgb(114, 132, 181, 231) :
                        (_hovered ? Color.FromArgb(82, 128, 160, 211) : Color.FromArgb(48, 108, 140, 189));
                    Color bottom = _selected ? Color.FromArgb(78, 49, 77, 119) :
                        (_hovered ? Color.FromArgb(96, 39, 65, 98) : Color.FromArgb(72, 26, 43, 65));
                    using (LinearGradientBrush fill = new LinearGradientBrush(box, top, bottom, LinearGradientMode.Vertical))
                        g.FillPath(fill, shape);

                    if (_hovered)
                    {
                        GraphicsState clip = g.Save();
                        g.SetClip(shape);
                        float radius = Math.Max(26.0F, box.Height * 1.55F);
                        using (GraphicsPath glowShape = new GraphicsPath())
                        {
                            glowShape.AddEllipse(_pointer.X - radius, _pointer.Y - radius, radius * 2.0F, radius * 2.0F);
                            using (PathGradientBrush glow = new PathGradientBrush(glowShape))
                            {
                                glow.CenterColor = Color.FromArgb(54, 218, 237, 255);
                                glow.SurroundColors = new Color[] { Color.FromArgb(0, 165, 203, 255) };
                                g.FillPath(glow, glowShape);
                            }
                        }
                        g.Restore(clip);
                    }

                    RectangleF reflection = new RectangleF(box.X + 1.5F, box.Y + 1.5F, box.Width - 3.0F,
                        Math.Max(2.0F, box.Height * 0.43F));
                    using (GraphicsPath reflectionShape = RoundedRectangle(reflection, Math.Min(9.0F, reflection.Height / 2.0F)))
                    using (LinearGradientBrush reflectionBrush = new LinearGradientBrush(reflection,
                        Color.FromArgb(_hovered || _selected ? 34 : 21, 230, 242, 255),
                        Color.FromArgb(0, 230, 242, 255), LinearGradientMode.Vertical))
                        g.FillPath(reflectionBrush, reflectionShape);

                    Color edgeColor = !Enabled ? Color.FromArgb(26, 145, 163, 194) :
                        (_selected ? Color.FromArgb(205, 169, 203, 255) :
                            (_hovered || Focused ? Color.FromArgb(164, 163, 204, 255) : Color.FromArgb(74, 166, 193, 229)));
                    using (Pen border = new Pen(edgeColor, _selected || Focused ? 1.35F : 1.0F))
                        g.DrawPath(border, shape);

                    if (_selected)
                    {
                        float indicatorWidth = Math.Min(48.0F, box.Width * 0.46F);
                        RectangleF indicator = new RectangleF(box.X + (box.Width - indicatorWidth) / 2.0F,
                            box.Bottom - 3.0F, indicatorWidth, 1.5F);
                        using (LinearGradientBrush light = new LinearGradientBrush(indicator,
                            Color.FromArgb(35, 169, 235, 255), Color.FromArgb(230, 178, 205, 255), LinearGradientMode.Horizontal))
                        using (GraphicsPath line = RoundedRectangle(indicator, 0.8F))
                            g.FillPath(light, line);
                    }
                }

                Color text = Enabled ? ForeColor : Color.FromArgb(108, 149, 162, 184);
                TextRenderer.DrawText(g, Text, Font, ClientRectangle, text,
                    TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter |
                    TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine);
            }

            public static GraphicsPath RoundedRectangle(RectangleF rect, float radius)
            {
                GraphicsPath path = new GraphicsPath();
                float diameter = radius * 2.0F;
                if (diameter <= 1.0F)
                {
                    path.AddRectangle(rect);
                    return path;
                }
                path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0F, 90.0F);
                path.AddArc(rect.Right - diameter, rect.Y, diameter, diameter, 270.0F, 90.0F);
                path.AddArc(rect.Right - diameter, rect.Bottom - diameter, diameter, diameter, 0.0F, 90.0F);
                path.AddArc(rect.X, rect.Bottom - diameter, diameter, diameter, 90.0F, 90.0F);
                path.CloseFigure();
                return path;
            }
        }

        private sealed class ConnectionPill : LiquidGlassButton
        {
            public string HostLabel { get; set; }
            public string StatusLabel { get; set; }
            public bool IsConnected { get; set; }

            public ConnectionPill()
            {
                HostLabel = "SWIFTWING NAS";
                StatusLabel = "等待连接";
                Text = string.Empty;
                IsSelected = true;
                AccessibleName = "NAS connection status; click to open connection options";
            }

            protected override void OnPaint(PaintEventArgs e)
            {
                base.OnPaint(e);
                Graphics g = e.Graphics;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                Color dotColor = IsConnected ? Color.FromArgb(112, 236, 190) :
                    (StatusLabel == "连接失败" || StatusLabel == "组件异常" ? Color.FromArgb(255, 132, 144) : Color.FromArgb(224, 190, 112));
                Rectangle dot = new Rectangle(17, Math.Max(0, (Height - 8) / 2), 8, 8);
                using (SolidBrush brush = new SolidBrush(dotColor)) g.FillEllipse(brush, dot);
                Rectangle hostRect = new Rectangle(34, 0, Math.Max(40, Width - 176), Height);
                Rectangle stateRect = new Rectangle(Math.Max(40, Width - 132), 0, 94, Height);
                TextRenderer.DrawText(g, HostLabel ?? "SWIFTWING NAS", Font, hostRect,
                    Color.FromArgb(234, 242, 255), TextFormatFlags.Left | TextFormatFlags.VerticalCenter |
                    TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine);
                TextRenderer.DrawText(g, StatusLabel ?? "", Font, stateRect,
                    Color.FromArgb(171, 192, 222), TextFormatFlags.Right | TextFormatFlags.VerticalCenter |
                    TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine);
                using (Pen chevron = new Pen(Color.FromArgb(186, 207, 239), 1.5F))
                {
                    float x = Width - 19;
                    float y = Height / 2.0F;
                    g.DrawLines(chevron, new PointF[] {
                        new PointF(x - 3.5F, y - 1.5F), new PointF(x, y + 2.0F), new PointF(x + 3.5F, y - 1.5F)
                    });
                }
            }
        }

        private sealed class GlassMenuRenderer : ToolStripProfessionalRenderer
        {
            public GlassMenuRenderer() : base(new GlassMenuColors()) { }

            protected override void OnRenderMenuItemBackground(ToolStripItemRenderEventArgs e)
            {
                if (!e.Item.Selected && !e.Item.Pressed) return;
                Rectangle rect = new Rectangle(Point.Empty, e.Item.Size);
                rect.Inflate(-3, -1);
                e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
                using (GraphicsPath shape = LiquidGlassButton.RoundedRectangle(rect, 8F))
                using (LinearGradientBrush fill = new LinearGradientBrush(rect,
                    Color.FromArgb(105, 112, 146, 192), Color.FromArgb(70, 48, 72, 111), LinearGradientMode.Vertical))
                using (Pen edge = new Pen(Color.FromArgb(105, 176, 204, 246)))
                {
                    e.Graphics.FillPath(fill, shape);
                    e.Graphics.DrawPath(edge, shape);
                }
            }
        }

        private sealed class GlassMenuColors : ProfessionalColorTable
        {
            public override Color ToolStripDropDownBackground { get { return Color.FromArgb(23, 37, 56); } }
            public override Color MenuBorder { get { return Color.FromArgb(94, 136, 174, 221); } }
            public override Color MenuItemBorder { get { return Color.Transparent; } }
            public override Color MenuItemSelected { get { return Color.Transparent; } }
            public override Color MenuItemSelectedGradientBegin { get { return Color.Transparent; } }
            public override Color MenuItemSelectedGradientEnd { get { return Color.Transparent; } }
            public override Color ImageMarginGradientBegin { get { return Color.FromArgb(23, 37, 56); } }
            public override Color ImageMarginGradientMiddle { get { return Color.FromArgb(23, 37, 56); } }
            public override Color ImageMarginGradientEnd { get { return Color.FromArgb(23, 37, 56); } }
            public override Color SeparatorDark { get { return Color.FromArgb(63, 86, 117); } }
            public override Color SeparatorLight { get { return Color.Transparent; } }
        }

        private sealed class GlassAddressPanel : Panel
        {
            private bool _hovered;
            private Point _pointer;
            private bool _focused;

            public bool IsFocused
            {
                get { return _focused; }
                set
                {
                    if (_focused == value) return;
                    _focused = value;
                    Invalidate();
                }
            }

            public GlassAddressPanel()
            {
                SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                    ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                    ControlStyles.SupportsTransparentBackColor, true);
                BackColor = Color.Transparent;
            }

            protected override void OnMouseEnter(EventArgs e)
            {
                _hovered = true;
                base.OnMouseEnter(e);
                Invalidate();
            }

            protected override void OnMouseLeave(EventArgs e)
            {
                _hovered = false;
                base.OnMouseLeave(e);
                Invalidate();
            }

            protected override void OnMouseMove(MouseEventArgs e)
            {
                _pointer = e.Location;
                base.OnMouseMove(e);
                if (_hovered) Invalidate();
            }

            protected override void OnPaint(PaintEventArgs e)
            {
                Graphics g = e.Graphics;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                RectangleF box = ClientRectangle;
                box.Inflate(-1.0F, -1.0F);
                if (box.Width <= 2 || box.Height <= 2) return;
                using (GraphicsPath shape = LiquidGlassButton.RoundedRectangle(box, Math.Min(10.0F, box.Height / 2.0F)))
                {
                    using (LinearGradientBrush fill = new LinearGradientBrush(box,
                        Color.FromArgb(_focused ? 104 : 64, 74, 104, 151), Color.FromArgb(74, 15, 29, 47), LinearGradientMode.Vertical))
                        g.FillPath(fill, shape);
                    if (_hovered)
                    {
                        GraphicsState clip = g.Save();
                        g.SetClip(shape);
                        float radius = Math.Max(34.0F, box.Height * 1.8F);
                        using (GraphicsPath glowShape = new GraphicsPath())
                        {
                            glowShape.AddEllipse(_pointer.X - radius, _pointer.Y - radius, radius * 2.0F, radius * 2.0F);
                            using (PathGradientBrush glow = new PathGradientBrush(glowShape))
                            {
                                glow.CenterColor = Color.FromArgb(44, 205, 227, 255);
                                glow.SurroundColors = new Color[] { Color.FromArgb(0, 130, 174, 235) };
                                g.FillPath(glow, glowShape);
                            }
                        }
                        g.Restore(clip);
                    }
                    using (Pen border = new Pen(_focused ? Color.FromArgb(204, 169, 204, 255) :
                        (_hovered ? Color.FromArgb(133, 159, 192, 236) : Color.FromArgb(70, 153, 182, 218)),
                        _focused ? 1.4F : 1.0F))
                        g.DrawPath(border, shape);
                }
            }
        }
    }

    internal sealed class ServerDialog : Form
    {
        private readonly TextBox _input;
        public string BaseUrl { get; private set; }

        public ServerDialog(string current)
        {
            Text = "服务器地址";
            StartPosition = FormStartPosition.CenterParent;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            ClientSize = new Size(530, 190);
            Font = new Font("Microsoft YaHei UI", 9F);
            BackColor = Color.FromArgb(238, 246, 248);
            BaseUrl = current;

            Label heading = new Label();
            heading.Text = "Swiftwing NAS 主页地址";
            heading.Font = new Font("Microsoft YaHei UI", 11F, FontStyle.Bold);
            heading.SetBounds(22, 18, 460, 28);
            Controls.Add(heading);

            _input = new TextBox();
            _input.Text = current;
            _input.Font = new Font("Segoe UI", 11F);
            _input.SetBounds(22, 56, 484, 29);
            Controls.Add(_input);

            Label hint = new Label();
            hint.Text = "支持 http:// 或 https://。只保存协议、主机名和端口；路径不会保存。";
            hint.ForeColor = Color.FromArgb(74, 90, 102);
            hint.SetBounds(22, 94, 484, 30);
            Controls.Add(hint);

            Button cancel = new Button();
            cancel.Text = "取消";
            cancel.DialogResult = DialogResult.Cancel;
            cancel.SetBounds(320, 142, 88, 32);
            Controls.Add(cancel);

            Button save = new Button();
            save.Text = "保存并打开";
            save.SetBounds(413, 142, 93, 32);
            save.Click += delegate
            {
                string normalized;
                if (!MainForm.TryBaseUrl(_input.Text, out normalized))
                {
                    MessageBox.Show(this, "请输入有效的 http:// 或 https:// 服务器地址。",
                        "地址无效", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    _input.Focus();
                    return;
                }
                BaseUrl = normalized;
                DialogResult = DialogResult.OK;
                Close();
            };
            Controls.Add(save);
            AcceptButton = save;
            CancelButton = cancel;
        }
    }
}
