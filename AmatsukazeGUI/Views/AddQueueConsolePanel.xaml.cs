using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace Amatsukaze.Views
{
    /// <summary>
    /// AddQueueConsole.xaml の相互作用ロジック
    /// </summary>
    public partial class AddQueueConsolePanel : UserControl
    {
        public AddQueueConsolePanel()
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
