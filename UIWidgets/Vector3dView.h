#pragma once

#include "IWindow.h"

#include "../Math/Vector3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Vector3df（3要素 float ベクトル）を表示・編集するウィジェット．
 *
 * ImGui::InputFloat3 を使って XYZ 成分の入力フィールドを描画する．
 */
class Vector3dView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロベクトル）．
	 * @param name ラベル文字列．
	 */
	explicit Vector3dView(const std::string& name) :
		IWindow(name),
		value(0, 0, 0)
	{}

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Vector3df）．
	 */
	Vector3dView(const std::string& name, const Math::Vector3df& value) :
		IWindow(name),
		value(value)
	{}

	/**
	 * @brief XYZ 成分の入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief Vector3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Vector3df& value) { this->value = value; }

	/**
	 * @brief 現在の Vector3df 値を返す．
	 * @return 現在の値．
	 */
	Math::Vector3df getValue() const { return value; }

private:
	Math::Vector3df value;
};

	}
}
