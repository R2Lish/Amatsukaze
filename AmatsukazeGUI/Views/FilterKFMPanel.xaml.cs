using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace Amatsukaze.Views
{
    /// <summary>
    /// FilterKFMPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class FilterKFMPanel : UserControl
    {
        public FilterKFMPanel()
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
    }
}
