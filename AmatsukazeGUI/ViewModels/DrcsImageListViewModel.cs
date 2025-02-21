﻿using System.ComponentModel;

using Livet.EventListeners;

using Amatsukaze.Models;
using Amatsukaze.Server;

namespace Amatsukaze.ViewModels
{
    public class DrcsImageListViewModel : NamedViewModel
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

        #region ImageList変更通知プロパティ
        private Components.ObservableViewModelCollection<DrcsImageViewModel, DrcsImage> _ImageList;

        public Components.ObservableViewModelCollection<DrcsImageViewModel, DrcsImage> ImageList {
            get { return _ImageList; }
            set { 
                if (_ImageList == value)
                    return;
                _ImageList = value;
                RaisePropertyChanged();
            }
        }
        #endregion

        #region DrcsImageSelectedItem変更通知プロパティ
        private DrcsImageViewModel _DrcsImageSelectedItem;

        public DrcsImageViewModel DrcsImageSelectedItem {
            get { return _DrcsImageSelectedItem; }
            set { 
                if (_DrcsImageSelectedItem == value)
                    return;
                _DrcsImageSelectedItem = value;
                RaisePropertyChanged();
            }
        }
        #endregion

        public string[] Panels { get; } = new string[]
        {
            "マッピングのない文字", "すべての文字"
        };

        #region PanelSelectedIndex変更通知プロパティ
        private int _PanelSelectedIndex = 0;

        public int PanelSelectedIndex {
            get { return _PanelSelectedIndex; }
            set { 
                // 未選択にはしない
                if (_PanelSelectedIndex == value || value == -1)
                    return;
                _PanelSelectedIndex = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(IsNoMapOnly));
                imagesView.Refresh();
            }
        }
        public bool IsNoMapOnly {
            get { return _PanelSelectedIndex == 0; }
        }
        #endregion

        private ICollectionView imagesView;
        private CollectionChangedEventListener imagesListener;

        public void Initialize()
        {
            ImageList = new Components.ObservableViewModelCollection<DrcsImageViewModel, DrcsImage>(
                Model.DrcsImageList, item => new DrcsImageViewModel(Model, item));

            imagesView = System.Windows.Data.CollectionViewSource.GetDefaultView(_ImageList);
            imagesView.Filter = x => IsNoMapOnly == false || string.IsNullOrEmpty(((DrcsImageViewModel)x).Image.MapStr);

            imagesListener = new CollectionChangedEventListener(_ImageList, (o, e) => imagesView.Refresh());
        }
    }
}
