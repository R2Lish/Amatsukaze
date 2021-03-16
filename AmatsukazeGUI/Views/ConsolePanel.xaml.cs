using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace Amatsukaze.Views
{
    /// <summary>
    /// ConsolePanel.xaml の相互作用ロジック
    /// </summary>
    public partial class ConsolePanel : UserControl
    {
        public ConsolePanel()
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
