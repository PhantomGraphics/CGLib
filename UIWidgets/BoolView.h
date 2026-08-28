#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief bool 値を表示・編集するウィジェット．
 *
 * ImGui::Checkbox を使ってチェックボックスを描画する．
 */
class BoolView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 false）．
	 * @param name ラベル文字列．
	 */
	explicit BoolView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値．
	 */
	BoolView(const std::string& name, const bool value);

	/**
	 * @brief チェックボックスを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在の bool 値を返す．
	 * @return 現在の値．
	 */
	bool getValue() const { return value; }

	/**
	 * @brief bool 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const bool value) { this->value = value; }

private:
	bool value;
};

	}
}
