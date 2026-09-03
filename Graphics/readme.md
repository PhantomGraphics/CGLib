# Graphics モジュール — ファイル構成

`Phantom::Graphics` 名前空間。カメラ・色空間・画像 I/O を提供する。`Math` に依存。
API 詳細は [`../docs/module-reference.md`](../docs/module-reference.md) を参照。

```mermaid
flowchart TB
    A["GraphicsCore\n(静的ライブラリ)"]

    subgraph HC[カメラ]
        HC1[Camera.h]
    end

    subgraph HColor[色]
        HColor1[ColorRGB.h / ColorRGBA.h / ColorHSV.h]
        HColor2[ColorConverter.h]
        HColor3[ColorTable.h / ColorMap.h]
    end

    subgraph HI[画像 I/O]
        HI1[Image.h]
        HI2[ImageFileReader.h]
        HI3[ImageFileWriter.h]
    end

    A --> HC
    A --> HColor
    A --> HI
```

- `Camera` は透視投影・正射影を切り替え、`getViewMatrix()` / `getProjectionMatrix()` /
  `getModelMatrix()` で GLM ベースの行列を返す。
- `ImageFileReader` / `ImageFileWriter` は STB 経由（8bit RGBA / HDR float）。
- `ColorMap` は値→色の線形マッピング（min/max 正規化 + カラーテーブル補間）で、科学可視化向け。
