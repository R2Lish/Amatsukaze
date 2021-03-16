﻿using Amatsukaze.ViewModels;
using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;

namespace Amatsukaze.Views
{
    /// <summary>
    /// QueuePanel.xaml の相互作用ロジック
    /// </summary>
    public partial class QueuePanel : UserControl
    {
        public QueuePanel()
        {
            InitializeComponent();

            // コンテキストメニューの外の名前空間を見えるようにする
            NameScope.SetNameScope(queueMenu, NameScope.GetNameScope(this));
            NameScope.SetNameScope(buttonProfileMenu, NameScope.GetNameScope(this));
        }

        private void ListBox_PreviewDragOver(object sender, DragEventArgs e)
        {
            e.Handled = e.Data.GetDataPresent(DataFormats.FileDrop) ||
                        e.Data.GetDataPresent(DataFormats.Text);
        }

        private void ListBox_Drop(object sender, DragEventArgs e)
        {
            var vm = DataContext as QueueViewModel;
            if (vm != null)
            {
                if (e.Data.GetDataPresent(DataFormats.FileDrop))
                {
                    vm.FileDropped(e.Data.GetData(DataFormats.FileDrop) as string[], false);
                }
            }
        }

        private DispatcherTimer dispatcherTimer;

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            var window = Window.GetWindow(this);
            window.GotFocus += UpdateTick;
            window.KeyDown += HandleKeyEvent;
            window.KeyUp += HandleKeyEvent;
            queueMenu.KeyDown += HandleKeyEvent;
            queueMenu.KeyUp += HandleKeyEvent;

            dispatcherTimer = new DispatcherTimer(DispatcherPriority.ContextIdle);
            dispatcherTimer.Interval = new TimeSpan(0, 0, 1);
            dispatcherTimer.Tick += new EventHandler(UpdateTick);
            dispatcherTimer.Start();
        }

        private void UpdateTick(object sender, EventArgs e)
        {
            var vm = DataContext as QueueViewModel;
            if (vm != null)
            {
                if (Window.GetWindow(this)?.IsActive ?? false)
                    vm.ShiftDown = (Keyboard.IsKeyDown(Key.LeftShift) || Keyboard.IsKeyDown(Key.RightShift));
            }
        }

        private void HandleKeyEvent(object sender, KeyEventArgs e)
        {
            var vm = DataContext as QueueViewModel;
            if(vm != null)
            {
                if (e.Key == Key.LeftShift || e.Key == Key.RightShift)
                {
                    // Shiftキーだった
                    vm.ShiftDown = e.IsDown;
                }
            }
        }
    }
}
