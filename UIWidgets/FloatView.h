#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief float 値を表示・編集するウィジェット．
 *
 * ImGui::InputFloat を使って入力フィールドを描画する．
 */
class FloatView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 0.0f）．
	 * @param name ラベル文字列．
	 */
	explicit FloatView(const std::string& name) :
		FloatView(name, 0.0f)
	{}

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値．
	 */
	FloatView(const std::string& name, const float value) :
		IWindow(name),
		value(value)
	{}

	/**
	 * @brief 入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在の float 値を返す．
	 * @return 現在の値．
	 */
	float getValue() const { return value; }

	/**
	 * @brief float 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const float value) { this->value = value; }

private:
	float value;
};

	}
}
