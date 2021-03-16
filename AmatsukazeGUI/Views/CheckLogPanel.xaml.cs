using Amatsukaze.ViewModels;
using System.Windows.Controls;
using System.Windows.Input;

namespace Amatsukaze.Views
{
    /// <summary>
    /// CheckLogPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class CheckLogPanel : UserControl
    {
        public CheckLogPanel()
        {
            InitializeComponent();
        }

        private void ListView_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            var vm = DataContext as CheckLogViewModel;
            if (vm != null)
            {
                vm.GetLogFileOfCurrentSelectedItem();
            }
        }
    }
}
