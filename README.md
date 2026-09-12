# MSDOS4 for PC98 (Kernel)
MS-DOS 4.0 のソースコードをPC98で動かしてみた。

FreeDOS(98)のカーネルを使ってMS-DOS 4.0を実行します。

途中で放っていたものですがAIが続きを書いてくれました。

FreeDOS(98)の派生物みたいなものなのでコネクタ部分のライセンスもGPL2にしておきます。

* 日本語 MSDOS.SYS, COMMAND.COM<br>
https://github.com/sanguisorba/DOS400_JAP

* FDFORMAT for MSDOS 4.0<br>
https://github.com/sanguisorba/fdformat/

# 概要
FreeDOS(98), MS-DOS のソースコードにはなるべく手をつけず、間をつなげるアダプタモジュールを作って起動します。

ビルドするとKERNEL.SYSが出来上がるのでこれをFreeDOS(98)のIPLで読み込んで起動させます。

KERNEL.SYSには本来IO.SYSとMSDOS.SYSに相当する機能があります。本ビルドではMSDOS.SYSの機能を欠落させ、代わりに外部のMSDOS.SYSを読むためのコネクタを起動します。

# 中身
* licenses - 引用してきたファイルたちのライセンス情報。emu2のソースコードはGPL2に則り添付してあります。
* scripts - ビルドスクリプト。かつてはMakefileでやっていましたがAIとしてはPythonのほうがやりやすいみたい。
* source - アダプタの中身です。
* src - KERNEL.SYSを生成するのに必要なFreeDOS(98)とMSDOSのソースコードです。
* tools - Ubuntu用ビルドツール一式。

## FreeDOS(98)の改変内容
ソースコードには一切の手を加えていません。依存関係が知りたい皆さんのために、不要なコードは全部消してあります。
## MSDOSの改変内容
主にIBMVER無効化、DBCS有効化、STACKSW無効化を行っています。こちらも依存関係を知りたい皆さんのために、不要なコードは全部消してあります。

スイッチを操作した際に発現するバグがあるため、最小限のパッチを当てています。該当箇所は sanguisorba で検索

# 手順
python3でビルドスクリプトを走らせてください。Ubuntu以外でビルドする場合は各自頑張ってください。

あとはFreeDOSでフォーマットしたフロッピーにKERNEL.SYS, MSDOS.SYS, COMMAND.COMの3点を書きこんだらOK.

MSDOS.SYS, COMMAND.COMは別のレポジトリにあります

# 残件
* HDDを繋げた時のCHSの表示がうまくいかない
* 2000年問題。時刻取得がうまくいってないかも
* 各種テスト。現状MS-DOS版N88-BASICを起動した時にKEYテーブルが変になるのは確認しています。
