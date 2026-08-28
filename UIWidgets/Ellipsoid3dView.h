#pragma once

#include "IView.h"
#include "Vector3dView.h"

#include "../Math/Ellipsoid3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Ellipsoid3df（3D楕円体）を表示・編集するウィジェット．
 *
 * IView を継承し，中心点（center），U軸（uvec），V軸（vvec），W軸（wvec）の
 * 4つの Vector3dView を持つ．各軸ベクトルの長さが各半径に対応する．
 */
class Ellipsoid3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Ellipsoid3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Ellipsoid3df）．
	 */
	Ellipsoid3dView(const std::string& name, const Math::Ellipsoid3df& value);

	/**
	 * @brief 現在の Ellipsoid3df 値を返す．
	 * @return center / uvec / vvec / wvec の各サブビューから再構成した値．
	 */
	Math::Ellipsoid3df getValue() const;

	/**
	 * @brief Ellipsoid3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Ellipsoid3df& value);

private:
	Vector3dView centerView; ///< 楕円体の中心点．
	Vector3dView uvecView;   ///< 楕円体の U 軸ベクトル．
	Vector3dView vvecView;   ///< 楕円体の V 軸ベクトル．
	Vector3dView wvecView;   ///< 楕円体の W 軸ベクトル．
};

	}
}
