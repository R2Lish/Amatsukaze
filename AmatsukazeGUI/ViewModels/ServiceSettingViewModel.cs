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

namespace Amatsukaze.ViewModels
{
    public class ServiceSettingViewModel : NamedViewModel
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

        public void Initialize()
        {
        }

        #region SelectedServiceIndex変更通知プロパティ
        private int _SelectedServiceIndex = -1;

        public int SelectedServiceIndex {
            get { return _SelectedServiceIndex; }
            set { 
                if (_SelectedServiceIndex == value)
                    return;
                _SelectedServiceIndex = value;
                RaisePropertyChanged(nameof(SelectedServiceItem));
                RaisePropertyChanged(nameof(SelectedLogoItem));
                RaisePropertyChanged();
            }
        }

        public DisplayService SelectedServiceItem
        {
            get
            {
                if (_SelectedServiceIndex >= 0 && _SelectedServiceIndex < Model.ServiceSettings.Count)
                {
                    return Model.ServiceSettings[_SelectedServiceIndex];
                }
                return null;
            }
        }
        #endregion

        #region SelectedLogoIndex変更通知プロパティ
        private int _SelectedLogoIndex = -1;

        public int SelectedLogoIndex
        {
            get { return _SelectedLogoIndex; }
            set
            {
                if (_SelectedLogoIndex == value)
                    return;
                _SelectedLogoIndex = value;
                RaisePropertyChanged(nameof(SelectedLogoItem));
                RaisePropertyChanged();
            }
        }

        public DisplayLogo SelectedLogoItem
        {
            get
            {
                var service = SelectedServiceItem;
                if(service != null && service.LogoList != null &&
                    _SelectedLogoIndex >= 0 &&
                    _SelectedLogoIndex < service.LogoList.Length)
                {
                    return service.LogoList[_SelectedLogoIndex];
                }
                return null;
            }
        }
        #endregion

        #region ApplyDateCommand
        private ViewModelCommand _ApplyDateCommand;

        public ViewModelCommand ApplyDateCommand {
            get {
                if (_ApplyDateCommand == null)
                {
                    _ApplyDateCommand = new ViewModelCommand(ApplyDate);
                }
                return _ApplyDateCommand;
            }
        }

        public void ApplyDate()
        {
            var logo = SelectedLogoItem;
            if(logo != null)
            {
                logo.ApplyDate();
            }
        }
        #endregion

        #region AddNoLogoCommand
        private ViewModelCommand _AddNoLogoCommand;

        public ViewModelCommand AddNoLogoCommand {
            get {
                if (_AddNoLogoCommand == null)
                {
                    _AddNoLogoCommand = new ViewModelCommand(AddNoLogo);
                }
                return _AddNoLogoCommand;
            }
        }

        public void AddNoLogo()
        {
            var service = SelectedServiceItem;
            if(service != null)
            {
                Model.Server?.SetServiceSetting(new ServiceSettingUpdate() {
                    Type = ServiceSettingUpdateType.AddNoLogo,
                    ServiceId = service.Data.ServiceId
                });
            }
        }
        #endregion

        #region RemoveNoLogoCommand
        private ViewModelCommand _RemoveNoLogoCommand;

        public ViewModelCommand RemoveNoLogoCommand {
            get {
                if (_RemoveNoLogoCommand == null)
                {
                    _RemoveNoLogoCommand = new ViewModelCommand(RemoveNoLogo);
                }
                return _RemoveNoLogoCommand;
            }
        }

        public void RemoveNoLogo()
        {
            var logo = SelectedLogoItem;
            if (logo != null)
            {
                if(logo.Setting.FileName == LogoSetting.NO_LOGO)
                {
                    Model.RemoveLogo(logo);
                }
            }
        }
        #endregion

        #region RemoveServiceSettingCommand
        private ViewModelCommand _RemoveServiceSettingCommand;

        public ViewModelCommand RemoveServiceSettingCommand {
            get {
                if (_RemoveServiceSettingCommand == null)
                {
                    _RemoveServiceSettingCommand = new ViewModelCommand(RemoveServiceSetting);
                }
                return _RemoveServiceSettingCommand;
            }
        }

        public void RemoveServiceSetting()
        {
            var service = SelectedServiceItem;
            if (service != null)
            {
                Model.Server?.SetServiceSetting(new ServiceSettingUpdate() {
                    Type = ServiceSettingUpdateType.Remove,
                    ServiceId = service.Data.ServiceId
                });
            }
        }
        #endregion

    }
}
