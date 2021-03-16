using Amatsukaze.ViewModels;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Shapes;

namespace Amatsukaze.Views
{
    /// <summary>
    /// DiskFreeSpacePanel.xaml の相互作用ロジック
    /// </summary>
    public partial class DiskFreeSpacePanel : UserControl
    {
        public DiskFreeSpacePanel()
        {
            InitializeComponent();
        }

        private void Rectangle_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            var rect = sender as Rectangle;
            var dc = rect.DataContext as DiskItemViewModel;
            dc.TotalWidth = rect.ActualWidth;
        }
    }
}
