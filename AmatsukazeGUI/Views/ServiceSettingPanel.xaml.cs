using System.Windows.Controls;
using System.Windows.Input;

namespace Amatsukaze.Views
{
    /// <summary>
    /// ServiceSettingPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class ServiceSettingPanel : UserControl
    {
        public ServiceSettingPanel()
        {
            InitializeComponent();
        }

        private void Calendar_SelectedDatesChanged(object sender, SelectionChangedEventArgs e)
        {
            Mouse.Capture(null);
        }
    }
}
