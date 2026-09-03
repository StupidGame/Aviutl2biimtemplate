# biimテンプレート for AviUtl2

AviUtl2のフィルタプラグインSDKで作った、biim形式の画面枠を出力するメディアオブジェクトです。
ゲーム欄は初期状態で透明なので、ゲーム映像の前面へ一つ置くだけで右欄と下欄を重ねられます。

## インストール

`dist/BiimTemplate.au2pkg.zip` をAviUtl2のプレビュー画面へドラッグ＆ドロップします。

手動で入れる場合は、`dist/BiimTemplate.auf2` をAviUtl2のアプリケーションデータフォルダにある
`Plugin/BiimTemplate/` へコピーします。

## 使い方

1. AviUtl2のオブジェクト追加から「biimテンプレート」を追加します。
2. ゲーム映像より前面に描画されるレイヤーへ置き、必要な長さまで伸ばします。
3. オブジェクト設定で右欄と下欄の文章、背景色、幅、高さなどを編集します。

主な設定は次の通りです。

- レイアウト: 右欄の幅、下欄の高さ、余白、区切り線
- 背景: 右欄・下欄・ゲーム欄の色、背景の不透明度
- テキスト: 見出し、右欄本文、話者名、下欄本文、フォント、文字色、縁取り
- 「720p基準で拡縮」を有効にすると、余白や文字サイズを出力解像度に合わせて拡縮します
- 「ゲーム欄を塗る」を有効にすると、透明部分にも指定色を入れられます

## ビルド

Visual Studioの「C++によるデスクトップ開発」を入れたWindows環境で、PowerShellから実行します。

```powershell
.\build.ps1
```

生成物は `dist/` に出力されます。
DLLを読み込み、1280x720の既定レイアウトまで検査する場合は `.\build.ps1 -RunTests` を使います。

GitHub Actionsの「Build AviUtl2 plugin」は、`main`へのpush、Pull Request、`v`で始まるタグ、
手動実行でWindows x64版をビルドします。完了後のArtifactsから
`BiimTemplate-AviUtl2-x64`を取得できます。

ローカルに `aviutl2_sdk/filter2.h` があれば、そのSDKを参照します。SDKがない場合、または
`-DownloadSdk`を付けた場合は、[AviUtl公式サイト](https://spring-fragrance.mints.ne.jp/aviutl/index.php)から
2026年8月31日版のSDKを取得し、SHA-256を検証してからビルドします。SDK本体はリポジトリに含めません。
SDKのライセンス情報は `THIRD_PARTY_NOTICES.md` を確認してください。
