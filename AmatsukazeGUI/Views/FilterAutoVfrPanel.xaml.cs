using System;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Navigation;

namespace Amatsukaze.Views
{
    /// <summary>
    /// FilterAutoVfrPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class FilterAutoVfrPanel : UserControl
    {
        public FilterAutoVfrPanel()
        {
            InitializeComponent();
        }

        private void Hyperlink_RequestNavigate(object sender, RequestNavigateEventArgs e)
        {
            var url = e.Uri.OriginalString.Replace("&", "^&");
            System.Diagnostics.Process.Start(
                new System.Diagnostics.ProcessStartInfo("cmd", $"/c start {url}") { CreateNoWindow = true });
        }

        private void Hyperlink_PluginFolder(object sender, RequestNavigateEventArgs e)
        {
            var path = AppContext.BaseDirectory + "plugins64";
            if (!System.IO.Directory.Exists(path))
            {
                System.IO.Directory.CreateDirectory(path);
            }
            System.Diagnostics.Process.Start("EXPLORER.EXE", path);
        }

        private void Hyperlink_Click(object sender, RoutedEventArgs e)
        {
            var link = (Hyperlink)sender;
            var text = string.Join("\r\n", link.Inlines.Where(r => r is Run).Cast<Run>().Select(r => r.Text));
            App.SetClipboardText(text);
        }
    }
}
