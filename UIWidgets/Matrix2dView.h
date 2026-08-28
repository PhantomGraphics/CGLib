#pragma once

#include "IWindow.h"

#include "../Math/Matrix2d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Matrix2df（2x2 float 行列）を表示・編集するウィジェット．
 *
 * 各行を ImGui::InputFloat2 で描画する．初期値は単位行列．
 */
class Matrix2dView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 単位行列）．
	 * @param name ラベル文字列．
	 */
	explicit Matrix2dView(const std::string& name) :
		IWindow(name),
		value(Math::Matrix2df(1, 0, 0, 1))
	{}

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Matrix2df）．
	 */
	Matrix2dView(const std::string& name, const Math::Matrix2df& value) :
		IWindow(name),
		value(value)
	{}

	/**
	 * @brief 2x2 行列の入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief Matrix2df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Matrix2df& value) { this->value = value; }

	/**
	 * @brief 現在の Matrix2df 値を返す．
	 * @return 現在の値．
	 */
	Math::Matrix2df getValue() const { return value; }

private:
	Math::Matrix2df value;
};

	}
}
