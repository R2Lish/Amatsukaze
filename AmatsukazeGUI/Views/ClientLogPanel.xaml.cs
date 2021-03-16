using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace Amatsukaze.Views
{
    /// <summary>
    /// ClientLogPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class ClientLogPanel : UserControl
    {
        public ClientLogPanel()
        {
            InitializeComponent();
        }

        private void MenuItem_Click(object sender, RoutedEventArgs e)
        {
           App.SetClipboardText(string.Join("\r\n",
                lst.SelectedItems.Cast<object>().Select(item => item.ToString())));
        }
    }
}
