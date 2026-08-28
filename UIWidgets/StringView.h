#pragma once

#include "IWindow.h"
#include <vector>

namespace Phantom {
	namespace UI {

/**
 * @brief std::string 値を表示・編集するウィジェット．
 *
 * ImGui::InputText を使ってテキスト入力フィールドを描画する．
 */
class StringView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 空文字列）．
	 * @param name ラベル文字列．
	 */
	explicit StringView(const std::string& name) :
		IWindow(name),
		value("")
	{}

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値．
	 */
	StringView(const std::string& name, const std::string& value) :
		IWindow(name),
		value(value.c_str())
	{}

	/**
	 * @brief テキスト入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在の文字列を返す．
	 * @return 現在の値．
	 */
	std::string getValue() const { return value; }

	/**
	 * @brief 文字列をプログラムから設定する．
	 * @param value 設定する文字列．
	 */
	void setValue(const std::string& value) { this->value = value; }

private:
	std::string value;
};

	}
}
