#pragma once

#include "IView.h"
#include "Vector3dView.h"
#include "FloatView.h"

#include "../Math/Sphere3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Sphere3df（3D球）を表示・編集するウィジェット．
 *
 * IView を継承し，中心点（center）と半径（radius）のサブビューを持つ．
 */
class Sphere3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Sphere3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Sphere3df）．
	 */
	Sphere3dView(const std::string& name, const Math::Sphere3df& value);

	/**
	 * @brief 現在の Sphere3df 値を返す．
	 * @return center / radius の各サブビューから再構成した値．
	 */
	Math::Sphere3df getValue() const;

	/**
	 * @brief Sphere3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Sphere3df& value);

private:
	Vector3dView center; ///< 球の中心点．
	FloatView radius;    ///< 球の半径．
};
	}
}
