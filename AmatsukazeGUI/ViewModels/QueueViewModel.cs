﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

using Livet;
using Livet.Commands;
using Livet.Messaging;
using Livet.Messaging.IO;
using Livet.EventListeners;
using Livet.Messaging.Windows;

using Amatsukaze.Models;
using Amatsukaze.Server;
using System.Threading.Tasks;
using System.Windows;
using System.IO;
using Amatsukaze.Components;
using System.Windows.Data;
using System.Collections;

namespace Amatsukaze.ViewModels
{
    public class QueueMenuProfileViewModel : ViewModel
    {
        public QueueViewModel QueueVM { get; set; }
        public object Item { get; set; }

        #region SelectedCommand
        private ListenerCommand<IEnumerable> _SelectedCommand;

        public ListenerCommand<IEnumerable> SelectedCommand
        {
            get
            {
                if (_SelectedCommand == null)
                {
                    _SelectedCommand = new ListenerCommand<IEnumerable>(Selected);
                }
                return _SelectedCommand;
            }
        }

        public void Selected(IEnumerable selectedItems)
        {
            QueueVM.ChangeProfile(selectedItems, Item);
        }
        #endregion
    }

    public class PriorityItemViewModel : ViewModel
    {
        public QueueViewModel QueueVM { get; set; }
        public int Priority { get; set; }

        #region SelectedCommand
        private ListenerCommand<IEnumerable> _SelectedCommand;

        public ListenerCommand<IEnumerable> SelectedCommand
        {
            get
            {
                if (_SelectedCommand == null)
                {
                    _SelectedCommand = new ListenerCommand<IEnumerable>(Selected);
                }
                return _SelectedCommand;
            }
        }

        public void Selected(IEnumerable selectedItems)
        {
            QueueVM.ChangePriority(selectedItems, Priority);
        }
        #endregion
    }

    public class QueueViewModel : NamedViewModel
    {
        /* コマンド、プロパティの定義にはそれぞれ 
         * 
         *  lvcom   : ViewModelCommand
         *  lvcomn  : ViewModelCommand(CanExecute無)
         *  llcom   : ListenerCommand(パラメータ有のコマンド)
         *  llcomn  : ListenerCommand(パラメータ有のコマンド・CanExecute無)
         *  lprop   : 変更通知プロパティ(.NET4.5ではlpropn)
         *  
         * を使用してください。
         * 
         * Modelが十分にリッチであるならコマンドにこだわる必要はありません。
         * View側のコードビハインドを使用しないMVVMパターンの実装を行う場合でも、ViewModelにメソッドを定義し、
         * LivetCallMethodActionなどから直接メソッドを呼び出してください。
         * 
         * ViewModelのコマンドを呼び出せるLivetのすべてのビヘイビア・トリガー・アクションは
         * 同様に直接ViewModelのメソッドを呼び出し可能です。
         */

        /* ViewModelからViewを操作したい場合は、View側のコードビハインド無で処理を行いたい場合は
         * Messengerプロパティからメッセージ(各種InteractionMessage)を発信する事を検討してください。
         */

        /* Modelからの変更通知などの各種イベントを受け取る場合は、PropertyChangedEventListenerや
         * CollectionChangedEventListenerを使うと便利です。各種ListenerはViewModelに定義されている
         * CompositeDisposableプロパティ(LivetCompositeDisposable型)に格納しておく事でイベント解放を容易に行えます。
         * 
         * ReactiveExtensionsなどを併用する場合は、ReactiveExtensionsのCompositeDisposableを
         * ViewModelのCompositeDisposableプロパティに格納しておくのを推奨します。
         * 
         * LivetのWindowテンプレートではViewのウィンドウが閉じる際にDataContextDisposeActionが動作するようになっており、
         * ViewModelのDisposeが呼ばれCompositeDisposableプロパティに格納されたすべてのIDisposable型のインスタンスが解放されます。
         * 
         * ViewModelを使いまわしたい時などは、ViewからDataContextDisposeActionを取り除くか、発動のタイミングをずらす事で対応可能です。
         */

        /* UIDispatcherを操作する場合は、DispatcherHelperのメソッドを操作してください。
         * UIDispatcher自体はApp.xaml.csでインスタンスを確保してあります。
         * 
         * LivetのViewModelではプロパティ変更通知(RaisePropertyChanged)やDispatcherCollectionを使ったコレクション変更通知は
         * 自動的にUIDispatcher上での通知に変換されます。変更通知に際してUIDispatcherを操作する必要はありません。
         */

        public ClientModel Model { get; set; }
        public MainWindowViewModel MainPanel { get; set; }

        private CollectionItemListener<DisplayQueueItem> queueListener;
        private CollectionChangedEventListener queueListener2;
        private ICollectionView itemsView;
        private PropertyChangedEventListener higeOneSegListener;

        public void Initialize()
        {
            ProfileList = new ObservableViewModelCollection<QueueMenuProfileViewModel, DisplayProfile>(
                Model.ProfileList, s => new QueueMenuProfileViewModel()
                {
                    QueueVM = this,
                    Item = s
                });

            AutoSelectList = new ObservableViewModelCollection<QueueMenuProfileViewModel, DisplayAutoSelect>(
                Model.AutoSelectList, s => new QueueMenuProfileViewModel()
                {
                    QueueVM = this,
                    Item = s
                });

            PriorityList = new List<PriorityItemViewModel>(
                Model.PriorityList.Select(p => new PriorityItemViewModel() { QueueVM = this, Priority = p }));

            queueListener = new CollectionItemListener<DisplayQueueItem>(Model.QueueItems,
                item => item.PropertyChanged += QueueItemPropertyChanged,
                item => item.PropertyChanged -= QueueItemPropertyChanged);

            queueListener2 = new CollectionChangedEventListener(Model.QueueItems,
                (sender, e) => ItemStateUpdated());

            itemsView = CollectionViewSource.GetDefaultView(Model.QueueItems);
            itemsView.Filter = ItemsFilter;

            higeOneSegListener = new PropertyChangedEventListener(Model, (sender, e) =>
            {
                if (e.PropertyName == "Setting" || e.PropertyName == "Setting.HideOneSeg")
                {
                    itemsView.Refresh();
                }
            });
        }

        private bool ItemsFilter(object obj)
        {
            var item = obj as DisplayQueueItem;
            if (Model.Setting.HideOneSeg && item.IsTooSmall) return false;
            if (string.IsNullOrEmpty(_SearchWord)) return true;
            return item.Model.ServiceName.IndexOf(_SearchWord) != -1 ||
                item.Model.FileName.IndexOf(_SearchWord) != -1 ||
                item.GenreString.IndexOf(_SearchWord) != -1 ||
                item.StateString.IndexOf(_SearchWord) != -1 ||
                item.ModeString.IndexOf(_SearchWord) != -1;
        }

        private void QueueItemPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if(string.IsNullOrEmpty(e.PropertyName))
            {
                ItemStateUpdated();
            }
        }

        public async void FileDropped(IEnumerable<string> list, bool isText)
        {
            var targets = list.SelectMany(path =>
            {
                if (Directory.Exists(path))
                {
                    return Directory.EnumerateFiles(path).Where(s => s.EndsWith("ts"));
                }
                else
                {
                    return new string[] { path };
                }
            }).Select(s => new AddQueueItem() { Path = s }).ToList();

            if(targets.Count == 0)
            {
                return;
            }

            var req = new AddQueueRequest()
            {
                DirPath = Path.GetDirectoryName(targets[0].Path),
                Targets = targets,
                Outputs = new List<OutputInfo>() {
                    new OutputInfo()
                    {
                        DstPath = Model.UIState.Model?.LastOutputPath,
                        Profile = Model.UIState.Model?.LastUsedProfile,
                        // デフォルト優先順は3
                        Priority = 3
                    }
                }
            };

            // 出力先フォルダ選択
            var selectPathVM = new SelectOutPathViewModel()
            {
                Model = Model,
                Item = req
            };
            await Messenger.RaiseAsync(new TransitionMessage(
                typeof(Views.SelectOutPath), selectPathVM, TransitionMode.Modal, "FromMain"));

            if (selectPathVM.Succeeded)
            {
                if(selectPathVM.PauseStart)
                {
                    await Model.Server?.PauseEncode(true);
                }
                Model.Server?.AddQueue(req).AttachHandler();
            }
        }

        private void LaunchLogoAnalyze(IEnumerable selectedItems, bool slimts)
        {
            var item = selectedItems.OfType<DisplayQueueItem>().FirstOrDefault();
            if (item == null)
            {
                return;
            }
            var file = item.Model;
            var workpath = Model.Setting.WorkPath;
            if (Directory.Exists(workpath) == false)
            {
                MessageBox.Show("一時ファイルフォルダがアクセスできる場所に設定されていないため起動できません");
                return;
            }
            string filepath = file.SrcPath;
            if (File.Exists(filepath) == false)
            {
                // failedに入っているかもしれないのでそっちも見る
                filepath = Path.GetDirectoryName(file.SrcPath) + "\\failed\\" + Path.GetFileName(file.SrcPath);
                if (File.Exists(filepath) == false)
                {
                    MessageBox.Show("ファイルが見つかりません");
                    return;
                }
            }
            var apppath = System.Reflection.Assembly.GetExecutingAssembly().Location;
            var args = "-l logo --file \"" + filepath + "\" --work \"" + workpath + "\" --serviceid " + file.ServiceId;
            if (slimts)
            {
                args += " --slimts";
            }
            System.Diagnostics.Process.Start(apppath, args);
        }

        private async Task<bool> ConfirmRetryCompleted(IEnumerable<DisplayQueueItem> items, string retry)
        {
            var completed = items.Where(s => s.Model.State == QueueState.Complete).ToArray();
            if (completed.Length > 0)
            {
                var top = completed.Take(10);
                var numOthers = completed.Count() - top.Count();
                var message = new ConfirmationMessage(
                    "以下のアイテムは既に完了しています。本当に" + retry + "しますか？\r\n\r\n" +
                    string.Join("\r\n", top.Select(s => s.Model.FileName)) +
                    ((numOthers > 0) ? "\r\n\r\n他" + numOthers + "個" : ""),
                    "Amatsukaze " + retry,
                    MessageBoxImage.Question,
                    MessageBoxButton.OKCancel,
                    "Confirm");

                await Messenger.RaiseAsync(message);

                return message.Response == true;
            }
            return true;
        }

        public ObservableViewModelCollection<QueueMenuProfileViewModel, DisplayProfile> ProfileList { get; private set; }
        public ObservableViewModelCollection<QueueMenuProfileViewModel, DisplayAutoSelect> AutoSelectList { get; private set; }
        public List<PriorityItemViewModel> PriorityList { get; private set; }

        #region SearchWord変更通知プロパティ
        private string _SearchWord;

        public string SearchWord
        {
            get { return _SearchWord; }
            set
            {
                if (_SearchWord == value)
                    return;
                _SearchWord = value;
                RaisePropertyChanged();
                itemsView.Refresh();
            }
        }
        #endregion

        #region ClearSearchWordCommand
        private ViewModelCommand _ClearSearchWordCommand;

        public ViewModelCommand ClearSearchWordCommand
        {
            get
            {
                if (_ClearSearchWordCommand == null)
                {
                    _ClearSearchWordCommand = new ViewModelCommand(ClearSearchWord);
                }
                return _ClearSearchWordCommand;
            }
        }

        public void ClearSearchWord()
        {
            SearchWord = null;
        }
        #endregion

        #region DeleteQueueItemCommand
        private ListenerCommand<IEnumerable> _DeleteQueueItemCommand;

        public ListenerCommand<IEnumerable> DeleteQueueItemCommand {
            get {
                if (_DeleteQueueItemCommand == null)
                {
                    _DeleteQueueItemCommand = new ListenerCommand<IEnumerable>(DeleteQueueItem);
                }
                return _DeleteQueueItemCommand;
            }
        }

        public async void DeleteQueueItem(IEnumerable selectedItems)
        {
            foreach (var item in selectedItems.OfType<DisplayQueueItem>().ToArray())
            {
                var file = item.Model;
                await Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.RemoveItem
                });
            }
        }
        #endregion

        #region RemoveCompletedAllCommand
        private ListenerCommand<IEnumerable> _RemoveCompletedAllCommand;

        public ListenerCommand<IEnumerable> RemoveCompletedAllCommand {
            get {
                if (_RemoveCompletedAllCommand == null)
                {
                    _RemoveCompletedAllCommand = new ListenerCommand<IEnumerable>(RemoveCompletedAll);
                }
                return _RemoveCompletedAllCommand;
            }
        }

        public void RemoveCompletedAll(IEnumerable selectedItems)
        {
            Model.Server?.ChangeItem(new ChangeItemData()
            {
                ChangeType = ChangeItemType.RemoveCompleted
            });
        }
        #endregion

        #region UpperRowLength変更通知プロパティ
        private GridLength _UpperRowLength = new GridLength(1, GridUnitType.Star);

        public GridLength UpperRowLength
        {
            get
            { return _UpperRowLength; }
            set
            {
                if (_UpperRowLength == value)
                    return;
                _UpperRowLength = value;
                RaisePropertyChanged();
            }
        }
        #endregion

        #region LowerRowLength変更通知プロパティ
        private GridLength _LowerRowLength = new GridLength(1, GridUnitType.Star);

        public GridLength LowerRowLength
        {
            get
            { return _LowerRowLength; }
            set
            {
                if (_LowerRowLength == value)
                    return;
                _LowerRowLength = value;
                RaisePropertyChanged();
            }
        }
        #endregion

        #region IsPanelOpen変更通知プロパティ
        private bool _IsPanelOpen;

        public bool IsPanelOpen
        {
            get
            { return _IsPanelOpen; }
            set
            {
                if (_IsPanelOpen == value)
                    return;
                _IsPanelOpen = value;
                RaisePropertyChanged();
            }
        }
        #endregion

        #region OpenLogoAnalyzeCommand
        private ListenerCommand<IEnumerable> _OpenLogoAnalyzeCommand;

        public ListenerCommand<IEnumerable> OpenLogoAnalyzeCommand
        {
            get
            {
                if (_OpenLogoAnalyzeCommand == null)
                {
                    _OpenLogoAnalyzeCommand = new ListenerCommand<IEnumerable>(OpenLogoAnalyze);
                }
                return _OpenLogoAnalyzeCommand;
            }
        }

        public void OpenLogoAnalyze(IEnumerable selectedItems)
        {
            LaunchLogoAnalyze(selectedItems, false);
        }
        #endregion

        #region OpenLogoAnalyzeSlimTsCommand
        private ListenerCommand<IEnumerable> _OpenLogoAnalyzeSlimTsCommand;

        public ListenerCommand<IEnumerable> OpenLogoAnalyzeSlimTsCommand
        {
            get
            {
                if (_OpenLogoAnalyzeSlimTsCommand == null)
                {
                    _OpenLogoAnalyzeSlimTsCommand = new ListenerCommand<IEnumerable>(OpenLogoAnalyzeSlimTs);
                }
                return _OpenLogoAnalyzeSlimTsCommand;
            }
        }

        public void OpenLogoAnalyzeSlimTs(IEnumerable selectedItems)
        {
            LaunchLogoAnalyze(selectedItems, true);
        }
        #endregion

        #region RetryCommand
        private ListenerCommand<IEnumerable> _RetryCommand;

        public ListenerCommand<IEnumerable> RetryCommand
        {
            get
            {
                if (_RetryCommand == null)
                {
                    _RetryCommand = new ListenerCommand<IEnumerable>(Retry);
                }
                return _RetryCommand;
            }
        }

        public async void Retry(IEnumerable selectedItems)
        {
            var items = selectedItems.OfType<DisplayQueueItem>().ToArray();
            if (await ConfirmRetryCompleted(items, "リトライ"))
            {
                foreach (var item in items)
                {
                    var file = item.Model;
                    await Model.Server?.ChangeItem(new ChangeItemData()
                    {
                        ItemId = file.Id,
                        ChangeType = ChangeItemType.ResetState
                    });
                }
            }
        }
        #endregion

        #region RetryUpdateCommand
        private ListenerCommand<IEnumerable> _RetryUpdateCommand;

        public ListenerCommand<IEnumerable> RetryUpdateCommand {
            get {
                if (_RetryUpdateCommand == null)
                {
                    _RetryUpdateCommand = new ListenerCommand<IEnumerable>(RetryUpdate);
                }
                return _RetryUpdateCommand;
            }
        }

        public async void RetryUpdate(IEnumerable selectedItems)
        {
            var items = selectedItems.OfType<DisplayQueueItem>().ToArray();
            foreach (var item in items)
            {
                var file = item.Model;
                await Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.UpdateProfile
                });
            }
        }
        #endregion

        #region ReAddCommand
        private ListenerCommand<IEnumerable> _ReAddCommand;

        public ListenerCommand<IEnumerable> ReAddCommand
        {
            get
            {
                if (_ReAddCommand == null)
                {
                    _ReAddCommand = new ListenerCommand<IEnumerable>(ReAdd);
                }
                return _ReAddCommand;
            }
        }

        public async void ReAdd(IEnumerable selectedItems)
        {
            var items = selectedItems.OfType<DisplayQueueItem>().ToArray();
            foreach (var item in items)
            {
                var file = item.Model;
                await Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.Duplicate
                });
            }
        }
        #endregion

        #region CancelCommand
        private ListenerCommand<IEnumerable> _CancelCommand;

        public ListenerCommand<IEnumerable> CancelCommand
        {
            get
            {
                if (_CancelCommand == null)
                {
                    _CancelCommand = new ListenerCommand<IEnumerable>(Cancel);
                }
                return _CancelCommand;
            }
        }

        public void Cancel(IEnumerable selectedItems)
        {
            foreach (var item in selectedItems.OfType<DisplayQueueItem>().ToArray())
            {
                var file = item.Model;
                Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.Cancel
                });
            }
        }
        #endregion
        
        #region RemoveCommand
        private ListenerCommand<IEnumerable> _RemoveCommand;

        public ListenerCommand<IEnumerable> RemoveCommand {
            get {
                if (_RemoveCommand == null)
                {
                    _RemoveCommand = new ListenerCommand<IEnumerable>(Remove);
                }
                return _RemoveCommand;
            }
        }

        public void Remove(IEnumerable selectedItems)
        {
            foreach (var item in selectedItems.OfType<DisplayQueueItem>().ToArray())
            {
                var file = item.Model;
                Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.RemoveItem
                });
            }
        }
        #endregion

        #region ForceStartCommand
        private ListenerCommand<IEnumerable> _ForceStartCommand;

        public ListenerCommand<IEnumerable> ForceStartCommand {
            get {
                if (_ForceStartCommand == null)
                {
                    _ForceStartCommand = new ListenerCommand<IEnumerable>(ForceStart);
                }
                return _ForceStartCommand;
            }
        }

        public void ForceStart(IEnumerable selectedItems)
        {
            foreach (var item in selectedItems.OfType<DisplayQueueItem>().ToArray())
            {
                var file = item.Model;
                Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.ForceStart
                });
            }
        }
        #endregion

        #region OpenFileInExplorerCommand
        private ListenerCommand<IEnumerable> _OpenFileInExplorerCommand;

        public ListenerCommand<IEnumerable> OpenFileInExplorerCommand
        {
            get
            {
                if (_OpenFileInExplorerCommand == null)
                {
                    _OpenFileInExplorerCommand = new ListenerCommand<IEnumerable>(OpenFileInExplorer);
                }
                return _OpenFileInExplorerCommand;
            }
        }

        public void OpenFileInExplorer(IEnumerable selectedItems)
        {
            var item = selectedItems.OfType<DisplayQueueItem>().FirstOrDefault();
            if (item == null)
            {
                return;
            }
            var file = item.Model;
            if (File.Exists(file.SrcPath) == false)
            {
                MessageBox.Show("ファイルが見つかりません");
                return;
            }
            System.Diagnostics.Process.Start("EXPLORER.EXE", "/select, \"" + file.SrcPath + "\"");
        }
        #endregion

        #region TogglePauseCommand
        private ViewModelCommand _TogglePauseCommand;

        public ViewModelCommand TogglePauseCommand {
            get {
                if (_TogglePauseCommand == null)
                {
                    _TogglePauseCommand = new ViewModelCommand(TogglePause);
                }
                return _TogglePauseCommand;
            }
        }

        public void TogglePause()
        {
            Model.Server?.PauseEncode(!Model.IsPaused);
        }
        #endregion

        #region ShowItemDetailCommand
        private ListenerCommand<DisplayQueueItem> _ShowItemDetailCommand;

        public ListenerCommand<DisplayQueueItem> ShowItemDetailCommand {
            get {
                if (_ShowItemDetailCommand == null)
                {
                    _ShowItemDetailCommand = new ListenerCommand<DisplayQueueItem>(ShowItemDetail);
                }
                return _ShowItemDetailCommand;
            }
        }

        public void ShowItemDetail(DisplayQueueItem item)
        {
            if(item.Model.State == QueueState.Encoding)
            {
                if(item.Model.ConsoleId < Model.ConsoleList.Count)
                {
                    MainPanel.ConsolePanelSelectedIndex = item.Model.ConsoleId;
                }
            }
        }
        #endregion

        #region ListStyle変更通知プロパティ
        private int _ListStyle;

        public int ListStyle
        {
            get { return _ListStyle; }
            set
            {
                if (_ListStyle == value)
                    return;
                _ListStyle = value;
                RaisePropertyChanged();
                itemsView.Refresh();
            }
        }
        #endregion

        public int Active { get { return Model.QueueItems.Count(s => s.Model.IsActive); } }
        public int Encoding { get { return Model.QueueItems.Count(s => s.Model.State == QueueState.Encoding); } }
        public int Complete { get { return Model.QueueItems.Count(s => s.Model.State == QueueState.Complete); } }
        public int Pending { get { return Model.QueueItems.Count(s => s.Model.State == QueueState.LogoPending); } }
        public int Fail { get { return Model.QueueItems.Count(s => s.Model.State == QueueState.Failed); } }
        public int Canceled { get { return Model.QueueItems.Count(s => s.Model.State == QueueState.Canceled); } }

        private void ItemStateUpdated()
        {
            RaisePropertyChanged("Active");
            RaisePropertyChanged("Encoding");
            RaisePropertyChanged("Complete");
            RaisePropertyChanged("Pending");
            RaisePropertyChanged("Fail");
            RaisePropertyChanged("Canceled");
        }

        public void ChangeProfile(IEnumerable selectedItems, object profile)
        {
            var profileName = DisplayProfile.GetProfileName(profile);
            foreach (var item in selectedItems.OfType<DisplayQueueItem>().ToArray())
            {
                var file = item.Model;
                Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.Profile,
                    Profile = profileName
                });
            }
        }

        public void ChangePriority(IEnumerable selectedItems, int priority)
        {
            foreach (var item in selectedItems.OfType<DisplayQueueItem>().ToArray())
            {
                var file = item.Model;
                Model.Server?.ChangeItem(new ChangeItemData()
                {
                    ItemId = file.Id,
                    ChangeType = ChangeItemType.Priority,
                    Priority = priority
                });
            }
        }
    }
}
