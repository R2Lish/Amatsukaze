using Amatsukaze.ViewModels;
using System.Windows.Controls;
using System.Windows.Input;

namespace Amatsukaze.Views
{
    /// <summary>
    /// EncodeLogPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class EncodeLogPanel : UserControl
    {
        public EncodeLogPanel()
        {
            InitializeComponent();
        }

        private void ListView_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            var vm = DataContext as EncodeLogViewModel;
            if (vm != null)
            {
                vm.GetLogFileOfCurrentSelectedItem();
            }
        }
    }
}
