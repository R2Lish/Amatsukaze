﻿using Amatsukaze.Components;
using System.Windows;
using System.Windows.Controls;

namespace Amatsukaze.Views
{
    /* 
	 * ViewModelからの変更通知などの各種イベントを受け取る場合は、PropertyChangedWeakEventListenerや
     * CollectionChangedWeakEventListenerを使うと便利です。独自イベントの場合はLivetWeakEventListenerが使用できます。
     * クローズ時などに、LivetCompositeDisposableに格納した各種イベントリスナをDisposeする事でイベントハンドラの開放が容易に行えます。
     *
     * WeakEventListenerなので明示的に開放せずともメモリリークは起こしませんが、できる限り明示的に開放するようにしましょう。
     */

    /// <summary>
    /// SelectOutPath.xaml の相互作用ロジック
    /// </summary>
    public partial class SelectOutPath : Window
    {
        public SelectOutPath()
        {
            InitializeComponent();
            Utils.SetWindowProperties(this);
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            Utils.SetWindowCenter(this);
        }

        private void textBox_PreviewDragOver(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                e.Effects = System.Windows.DragDropEffects.Copy;
                e.Handled = true;
            }
        }

        private void textBox_Drop(object sender, DragEventArgs e)
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