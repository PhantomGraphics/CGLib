# CGLib/File/FileTest テスト改善計画

作成日: 2026-04-17

---

## 1. 現状サマリー

### テストファイル一覧

| ファイル | テスト数 | 対象クラス |
|---|---|---|
| OBJFileReaderTest.cpp | 7 | OBJFileReader |
| OBJFileWriterTest.cpp | 1 | OBJFileWriter |
| MTLFileReaderTest.cpp | 14 | MTLFileReader |
| MTLFileWriterTest.cpp | 1 | MTLFileWriter |
| STLASCIIFileReaderTest.cpp | 2 | STLFileReader (ASCII) |
| STLASCIIFileWriterTest.cpp | 1 | STLFileWriter (ASCII) |
| PLYFileReaderTest.cpp | 2 | PLYFileReader |
| PLYFileWriterTest.cpp | 4 | PLYFileWriter |
| GLTFFileReaderWriterTest.cpp | 1 | GLTFFileReader + GLTFFileWriter |
| **合計** | **33** | |

### 形式別カバレッジ概況

| 形式 | Reader テスト数 | Writer テスト数 | ラウンドトリップ | Binary |
|---|---|---|---|---|
| OBJ | 7 | 1 | なし | - |
| MTL | 14 | 1 | なし | - |
| STL (ASCII) | 2 | 1 | なし | **未テスト** |
| STL (Binary) | **0** | **0** | なし | **未実装** |
| PLY | 2 | 4 | なし | 読み書き有 |
| GLTF | 1 (ラウンドトリップ) | 1 (ラウンドトリップ) | 1 | - |

---

## 2. 問題点の分類

### 2-A. 高優先度（バグを見逃すリスクが高い）

#### 2-A-1. STL バイナリ形式のテストがゼロ
- `STLFileReader::readBinary()` / `STLFileWriter::writeBinary()` の実装はあるが、テストが一切ない
- バイナリ STL は業界標準の主流フォーマット
- **追加すべきテスト:**
  - バイナリ読み込み（2〜3面）
  - バイナリ書き込み（2〜3面）
  - ASCII → Binary → ASCII ラウンドトリップ
  - 80バイトヘッダー内容の検証
  - 50バイトレコード（normal + 3 vertices + 2 padding bytes）の整合性確認

#### 2-A-2. ラウンドトリップ検証がほぼない
- GLTF 以外の全形式でラウンドトリップテストがない
- 書いて読んだ値が一致するかを検証しないと、Writer/Reader の対称性が保証できない
- **追加すべきテスト（各形式）:**
  - `OBJFileWriter` → `OBJFileReader` の整合性検証
  - `MTLFileWriter` → `MTLFileReader` の整合性検証
  - `STLFileWriter::writeAscii` → `STLFileReader::readAscii` の整合性検証
  - `STLFileWriter::writeBinary` → `STLFileReader::readBinary` の整合性検証
  - `PLYFileWriter::writeASCII` → `PLYFileReader::read` の整合性検証
  - `PLYFileWriter::writeBinary` → `PLYFileReader::read` の整合性検証

#### 2-A-3. エラーハンドリングテストが皆無
- 不正入力に対して何が返ってくるか検証されていない
- `STLASCIIFileReaderTest::TestReadNan` のみ唯一の否定テスト
- **追加すべきテスト:**
  - 存在しないパスを指定した場合の挙動
  - 空ファイル
  - 途中で切れた（truncated）バイナリ
  - 不正な頂点数（OBJの `f` 行が2頂点など）
  - 不正な型値（PLY の数値フィールドに文字列）

### 2-B. 中優先度（機能完全性の担保）

#### 2-B-1. MTL Writer の出力が不完全
- `MTLFileWriter` は Ka/Kd/Ks/Ns/Tr しか出力しない
- Reader が対応している `map_Ka`, `map_Kd`, `map_bump`, `Ni`, `illum` が書き出されない
- **追加すべきテスト:**
  - テクスチャ付きマテリアルのラウンドトリップ
  - illum 各モードの書き出し検証
  - Ni (屈折率) の書き出し検証

#### 2-B-2. OBJ Writer の検証が薄い
- 現在のテストは `TestWrite` 1件のみ
- **追加すべきテスト:**
  - 複数グループを持つモデルの書き出し
  - `mtllib` / `usemtl` 参照の書き出し
  - テクスチャ座標なし/法線なしの組み合わせ

#### 2-B-3. PLY プロパティ型バリエーション
- テストは float (FLOAT) の頂点座標のみ
- `PLYType` は CHAR/UCHAR/SHORT/USHORT/INT/UINT/FLOAT/DOUBLE の8種類
- **追加すべきテスト:**
  - DOUBLE / INT / UCHAR プロパティの読み書き
  - カラー属性付き点群（典型的な UCHAR r,g,b）

#### 2-B-4. GLTF の複雑ケース
- 現在のテストは 1mesh/1material/1node/1scene の最小構成のみ
- **追加すべきテスト:**
  - 複数メッシュ・複数プリミティブ
  - すべての `GLTFPrimitiveMode` (Points, Lines, ...)
  - タンジェント(tangents)属性付きプリミティブ
  - インデックスなし（非インデックス）プリミティブ
  - doubleSided マテリアル

### 2-C. 低優先度（品質向上・将来対応）

#### 2-C-1. 大規模ファイルのテスト
- 数万〜数十万面モデルでの読み込み・書き込みパフォーマンス確認
- メモリ使用量の妥当性確認

#### 2-C-2. 外部ツール出力ファイルとの相互運用テスト
- Blender, MeshLab, Assimp が出力した実ファイルを使用するテスト
- `test_data/` ディレクトリにサンプルファイルを配置して参照

#### 2-C-3. `Helper.h` のユニットテスト
- `read<T>()`, `readVector<T>()`, `readNextString()`, `split()` の単体テストが存在しない
- 境界値（空文字列、空白のみ、改行コード差異 \r\n vs \n）のテスト

---

## 3. 改善タスク一覧（優先度順）

| # | タスク | 新規ファイル | 優先度 | 工数目安 |
|---|---|---|---|---|
| T1 | STL バイナリ Reader テスト追加 | STLBinaryFileReaderTest.cpp | 高 | S |
| T2 | STL バイナリ Writer テスト追加 | STLBinaryFileWriterTest.cpp | 高 | S |
| T3 | STL ASCII ラウンドトリップテスト | STLASCIIFileReaderTest.cpp 追記 | 高 | S |
| T4 | STL バイナリ ラウンドトリップテスト | STLBinaryFileReaderTest.cpp 追記 | 高 | S |
| T5 | OBJ ラウンドトリップテスト | OBJFileReaderTest.cpp 追記 | 高 | S |
| T6 | PLY ラウンドトリップテスト（ASCII/Binary） | PLYFileReaderTest.cpp 追記 | 高 | S |
| T7 | MTL ラウンドトリップテスト | MTLFileReaderTest.cpp 追記 | 高 | S |
| T8 | エラーハンドリングテスト（全形式） | *ErrorHandlingTest.cpp | 高 | M |
| T9 | MTL Writer テクスチャ・illum 対応 | MTLFileWriterTest.cpp 追記 | 中 | M |
| T10 | OBJ Writer 複数グループ・mtllib | OBJFileWriterTest.cpp 追記 | 中 | S |
| T11 | PLY プロパティ型バリエーション | PLYFileReaderTest.cpp 追記 | 中 | M |
| T12 | GLTF 複数メッシュ・複数プリミティブ | GLTFFileReaderWriterTest.cpp 追記 | 中 | M |
| T13 | GLTF 全 PrimitiveMode | GLTFFileReaderWriterTest.cpp 追記 | 中 | S |
| T14 | Helper.h ユニットテスト | HelperTest.cpp | 低 | S |
| T15 | 外部ツールサンプルファイルテスト | test_data/ + 各形式テスト追記 | 低 | L |

工数目安: S=0.5日以内, M=1日, L=2日以上

---

## 4. 推奨実装順序

```
フェーズ1（高優先度・即着手）
  T1 → T2 → T3 → T4  （STL バイナリ完全対応）
  T5 → T6 → T7        （ラウンドトリップ整備）
  T8                   （エラーハンドリング）

フェーズ2（中優先度）
  T9 → T10             （Writer 品質向上）
  T11                  （PLY 型バリエーション）
  T12 → T13            （GLTF 拡充）

フェーズ3（低優先度）
  T14                  （Helper 単体テスト）
  T15                  （相互運用テスト）
```

---

## 5. テスト品質向上のための共通指針

1. **テスト名は `Test<動詞><対象><条件>` 形式に統一する**
   - 例: `TestReadBinaryWithTwoFaces`, `TestWriteAsciiRoundTrip`

2. **ラウンドトリップテストはインメモリ（`std::stringstream`）で行う**
   - ファイルI/O不要で高速・クリーン

3. **エラーテストは `EXPECT_THROW` または戻り値チェックで記述**
   - 現在の実装がどのようにエラーを返すか確認してから実装

4. **外部ファイルを使うテストは `test_data/` サブディレクトリに配置**
   - CMake/vcxproj のコピー設定も忘れずに更新

5. **テストごとに独立した入力データを持つ（フィクスチャ共有は最小限に）**

---

## 6. 参考：現在の実装で検出済みの潜在バグ

- `STLASCIIFileReaderTest::TestReadNan` — NaN を含む STL を読んだとき何が起きるか「期待失敗」として記録されているが、実際の戻り値・例外の仕様が明文化されていない
- `MTLFileWriter` は `d` (opacity) を書かず `Tr` (transparency) のみを書く。Reader は両方読む。ラウンドトリップ時に `d` 値が失われる可能性がある
- `PLYFileReader` は ASCII/Binary を自動判定していない可能性がある（`read()` 内の処理を要確認）
