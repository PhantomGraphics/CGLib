#pragma once

#include "IWindow.h"

#include "../Math/Matrix3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Matrix3df（3x3 float 行列）を表示・編集するウィジェット．
 *
 * 各行を ImGui::InputFloat3 で描画する．
 */
class Matrix3dView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 単位行列）．
	 * @param name ラベル文字列．
	 */
	explicit Matrix3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Matrix3df）．
	 */
	Matrix3dView(const std::string& name, const Math::Matrix3df& value);

	/**
	 * @brief 3x3 行列の入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief Matrix3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Matrix3df& value);

	/**
	 * @brief 現在の Matrix3df 値を返す．
	 * @return 現在の値．
	 */
	Math::Matrix3df getValue() const;

private:
	Math::Matrix3df value;
};

	}
}
