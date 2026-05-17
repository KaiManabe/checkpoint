# Checkpoint

## 概要
- ディレクトリをスナップショットとして保存し，あとから任意の状態に復元できるようにするためのCLIツール

## インストール
### Windows (exe)
1. [Releases](https://github.com/KaiManabe/checkpoint/releases)から最新のexeファイルをダウンロード
2. ダウンロードしたexeを任意の場所(例: `C:\checkpoint\checkpoint.exe`)に配置
3. 先の手順で配置したディレクトリにPATHを通す
4. 新しいターミナルを開き，`checkpoint --help` ではなく `checkpoint` を実行してコマンドが見つかることを確認

### Windows (ビルド)
1. Visual Studio 2022 もしくは Build Tools for Visual Studio 2022 をインストール
2. CMake が使える状態でリポジトリを取得
    ```bat
    git clone https://github.com/KaiManabe/checkpoint.git
    cd checkpoint
    ```
3. リポジトリ直下で `build_windows.bat` を実行
    ```bat
    build_windows.bat
    ```
4. ビルド後に `build\dist\checkpoint.exe` が生成される
2. `build\dist\checkpoint.exe`を任意の場所(例: `C:\checkpoint\checkpoint.exe`)に配置
5. 先の手順で配置したディレクトリにPATHを通す



### Linux
- デフォルト設定でインストール
    ```bash
    git clone https://github.com/KaiManabe/checkpoint.git
    cd checkpoint
    make
    sudo make install
    ```
- ユーザーローカルにインストール
    ```bash
    git clone https://github.com/KaiManabe/checkpoint.git
    cd checkpoint
    make
    make install PREFIX="$HOME/.local"
    ```
- 任意のディレクトリにインストール
    ```bash
    git clone https://github.com/KaiManabe/checkpoint.git
    cd checkpoint
    make
    make install DESTDIR=/your_directory
    ```

## 使用方法
### 初期化
```bash
checkpoint init
checkpoint init /your/working/directory
```
- 引数なしならカレントディレクトリを初期化します
- 初回スナップショット `initial` が自動作成されます


### スナップショット作成
```bash
checkpoint snap
checkpoint snap my_snapshot
```
- `init`したフォルダの現在の状態を保管します
- 成功すると，スナップショットidが表示されます
- 引数を与えることで，スナップショットにidをつけることもできます


### 復元
```bash
checkpoint restore my_snapshot
```
- 現在の状態でスナップショットを作成してから，指定したスナップショットの状態に復元します


## 除外対象
- あらゆる階層にある`.checkpoint`ディレクトリ
- あらゆる階層にある`.git`ディレクトリ
- あらゆる階層にある`.gitignore`に記載されているファイルやディレクトリ


## 免責事項
- このツールはファイルの保存と復元を行うため，バグによりファイルが消失する可能性がゼロではありません
  - そのため，gitとの併用をおすすめします
- このツールを使用したことによるいかなる損害に対する責任を負いかねます
