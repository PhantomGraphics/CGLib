#pragma once

#include "IView.h"
#include "Vector3dView.h"
#include "FloatView.h"

#include "../Math/Circle3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Circle3df（3D円）を表示・編集するウィジェット．
 *
 * IView を継承し，中心点（center），法線（normal），半径（radius）のサブビューを持つ．
 */
class Circle3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Circle3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Circle3df）．
	 */
	Circle3dView(const std::string& name, const Math::Circle3df& value);

	/**
	 * @brief 現在の Circle3df 値を返す．
	 * @return center / normal / radius の各サブビューから再構成した値．
	 */
	Math::Circle3df getValue() const;

	/**
	 * @brief Circle3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Circle3df& value);

private:
	Vector3dView center; ///< 円の中心点．
	Vector3dView normal; ///< 円の法線ベクトル．
	FloatView radius;    ///< 円の半径．
};

	}
}
