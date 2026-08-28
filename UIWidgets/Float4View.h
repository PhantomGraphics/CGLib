#pragma once

#include "IWindow.h"

#include "../Math/Vector4d.h"
#include <array>

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Vector4df（4要素 float ベクトル）を表示・編集するウィジェット．
 *
 * ImGui::InputFloat4 を使って4成分の入力フィールドを描画する．
 * 内部では double 精度（Vector4dd）で保持し，getValue() 時に float へ変換する．
 */
class Float4View : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロベクトル）．
	 * @param name ラベル文字列．
	 */
	explicit Float4View(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Vector4df）．
	 */
	Float4View(const std::string& name, const Math::Vector4df& value);

	/**
	 * @brief 4成分入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在の Vector4df 値を返す．
	 * @return 現在の値（float 精度）．
	 */
	Math::Vector4df getValue() const { return value; }

	/**
	 * @brief Vector4df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Vector4df& value) { this->value = value; }

private:
	Math::Vector4dd value; ///< 内部は double 精度で保持．
};

	}
}
