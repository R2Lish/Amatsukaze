using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;

namespace Amatsukaze.Views
{
    /// <summary>
    /// SettingPanel.xaml の相互作用ロジック
    /// </summary>
    public partial class SettingPanel : UserControl
    {
        public SettingPanel()
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
