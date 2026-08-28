#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief int 値を表示・編集するウィジェット．
 *
 * ImGui::InputInt を使って入力フィールドを描画する．
 */
class IntView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 0）．
	 * @param name ラベル文字列．
	 */
	explicit IntView(const std::string& name) :
		IWindow(name),
		value(0)
	{}

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値．
	 */
	IntView(const std::string& name, const int value) :
		IWindow(name),
		value(value)
	{}

	/**
	 * @brief 入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在の int 値を返す．
	 * @return 現在の値．
	 */
	int getValue() const { return value; }

	/**
	 * @brief int 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const int value) { this->value = value; }

private:
	int value;
};

	}
}
