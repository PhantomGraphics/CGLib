
#pragma once

#include "IView.h"
#include "Vector3dView.h"
#include "../Math/Ray3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Ray3df（3Dレイ：始点と方向）を表示・編集するウィジェット．
 *
 * IView を継承し，始点（origin）と方向（direction）の2つの Vector3dView を持つ．
 */
class Ray3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Ray3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Ray3df）．
	 */
	Ray3dView(const std::string& name, const Math::Ray3df& value);

	/**
	 * @brief 現在の Ray3df 値を返す．
	 * @return origin / direction の各サブビューから再構成した値．
	 */
	Math::Ray3df getValue() const;

	/**
	 * @brief Ray3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Ray3df& value);

private:
	Vector3dView originView;    ///< レイの始点．
	Vector3dView directionView; ///< レイの方向ベクトル．
};

	}
}
