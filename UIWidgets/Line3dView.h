#pragma once

#include "IView.h"

#include "Vector3dView.h"

#include "../Math/Line3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Line3df（3D線分）を表示・編集するウィジェット．
 *
 * IView を継承し，始点（start）と終点（end）の2つの Vector3dView を持つ．
 */
class Line3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Line3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Line3df）．
	 */
	Line3dView(const std::string& name, const Math::Line3df& value);

	/**
	 * @brief 現在の Line3df 値を返す．
	 * @return start / end の各サブビューから再構成した値．
	 */
	Math::Line3df getValue() const;

	/**
	 * @brief Line3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Line3df& value);

private:
	Vector3dView startView; ///< 線分の始点．
	Vector3dView endView;   ///< 線分の終点．
};

	}
}
