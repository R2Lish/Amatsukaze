using System.Windows;
using System.Windows.Controls;

namespace Amatsukaze.Views
{
    /// <summary>
    /// MakeScriptPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class MakeScriptPanel : UserControl
    {
        public MakeScriptPanel()
        {
            InitializeComponent();
        }

        private void TextBox_PreviewDragOver(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                e.Effects = System.Windows.DragDropEffects.Copy;
                e.Handled = true;
            }
        }

        private void TextBox_Drop(object sender, DragEventArgs e)
        {
            var files = e.Data.GetData(DataFormats.FileDrop) as string[];
            var target = sender as TextBox;
            if (files != null && target != null)
            {
                target.Text = files[0];
            }
        }
    }
}
