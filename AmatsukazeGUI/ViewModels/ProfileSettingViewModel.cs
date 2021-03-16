﻿using Amatsukaze.Models;
using Amatsukaze.Server;
using Livet.Commands;
using Livet.Messaging;
using System;
using System.Linq;

namespace Amatsukaze.ViewModels
{
    public class ProfileSettingViewModel : NamedViewModel
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

        #region ApplyProfileCommand
        private ViewModelCommand _ApplyProfileCommand;

        public ViewModelCommand ApplyProfileCommand {
            get {
                if (_ApplyProfileCommand == null)
                {
                    _ApplyProfileCommand = new ViewModelCommand(ApplyProfile);
                }
                return _ApplyProfileCommand;
            }
        }

        public void ApplyProfile()
        {
            if(Model.SelectedProfile != null)
            {
                Model.UpdateProfile(Model.SelectedProfile.Data);
            }
        }
        #endregion

        #region NewProfileCommand
        private ViewModelCommand _NewProfileCommand;

        public ViewModelCommand NewProfileCommand {
            get {
                if (_NewProfileCommand == null)
                {
                    _NewProfileCommand = new ViewModelCommand(NewProfile);
                }
                return _NewProfileCommand;
            }
        }

        public async void NewProfile()
        {
            if (Model.SelectedProfile != null)
            {
                var profile = Model.SelectedProfile;
                var newp = new NewProfileViewModel() {
                    Model = Model,
                    Title = "プロファイル",
                    Operation = "追加",
                    IsDuplicate = Name => Model.ProfileList.Any(
                        s => s.Data.Name.Equals(Name, StringComparison.OrdinalIgnoreCase)),
                Name = profile.Data.Name + "のコピー"
                };

                await Messenger.RaiseAsync(new TransitionMessage(
                    typeof(Views.NewProfileWindow), newp, TransitionMode.Modal, "FromProfile"));

                if(newp.Success)
                {
                    var newprofile = ServerSupport.DeepCopy(profile.Data);
                    var invalidChars = System.IO.Path.GetInvalidFileNameChars();
                    newprofile.Name = new string(newp.Name.ToCharArray()
                        .Where(c => Array.IndexOf(invalidChars, c) == -1).ToArray());
                    await Model.AddProfile(newprofile);
                }
            }
        }
        #endregion

        #region RenameProfileCommand
        private ViewModelCommand _RenameProfileCommand;

        public ViewModelCommand RenameProfileCommand {
            get {
                if (_RenameProfileCommand == null)
                {
                    _RenameProfileCommand = new ViewModelCommand(RenameProfile);
                }
                return _RenameProfileCommand;
            }
        }

        public async void RenameProfile()
        {
            if (Model.SelectedProfile != null)
            {
                var profile = Model.SelectedProfile;
                var newp = new NewProfileViewModel()
                {
                    Model = Model,
                    Title = "プロファイル",
                    Operation = "リネーム",
                    IsDuplicate = Name => Name != profile.Data.Name && Model.ProfileList.Any(
                        s => s.Data.Name.Equals(Name, StringComparison.OrdinalIgnoreCase)),
                    Name = profile.Data.Name
                };

                await Messenger.RaiseAsync(new TransitionMessage(
                    typeof(Views.NewProfileWindow), newp, TransitionMode.Modal, "FromProfile"));

                if (newp.Success)
                {
                    await Model.Server.SetProfile(new ProfileUpdate()
                    {
                        Type = UpdateType.Update,
                        Profile = profile.Data,
                        NewName = newp.Name
                    });
                }
            }
        }
        #endregion

        #region DeleteProfileCommand
        private ViewModelCommand _DeleteProfileCommand;

        public ViewModelCommand DeleteProfileCommand {
            get {
                if (_DeleteProfileCommand == null)
                {
                    _DeleteProfileCommand = new ViewModelCommand(DeleteProfile);
                }
                return _DeleteProfileCommand;
            }
        }

        public async void DeleteProfile()
        {
            if (Model.SelectedProfile != null)
            {
                var profile = Model.SelectedProfile.Data;

                var message = new ConfirmationMessage(
                    "プロファイル「" + profile.Name + "」を削除しますか？",
                    "Amatsukaze",
                    System.Windows.MessageBoxImage.Information,
                    System.Windows.MessageBoxButton.OKCancel,
                    "Confirm");

                await Messenger.RaiseAsync(message);

                if (message.Response == true)
                {
                    await Model.RemoveProfile(profile);
                }
            }
        }
        #endregion

        #region ResourceDescriptionVisible変更通知プロパティ
        private bool _ResourceDescriptionVisible;

        public bool ResourceDescriptionVisible {
            get { return _ResourceDescriptionVisible; }
            set { 
                if (_ResourceDescriptionVisible == value)
                    return;
                _ResourceDescriptionVisible = value;
                RaisePropertyChanged();
            }
        }
        #endregion

        #region ToggleResourceDescriptionCommand
        private ViewModelCommand _ToggleResourceDescriptionCommand;

        public ViewModelCommand ToggleResourceDescriptionCommand {
            get {
                if (_ToggleResourceDescriptionCommand == null)
                {
                    _ToggleResourceDescriptionCommand = new ViewModelCommand(ToggleResourceDescription);
                }
                return _ToggleResourceDescriptionCommand;
            }
        }

        public void ToggleResourceDescription()
        {
            ResourceDescriptionVisible = !ResourceDescriptionVisible;
        }
        #endregion

    }
}
