#pragma once

#include "IView.h"

#include "../Math/Matrix4d.h"
#include "Float4View.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Matrix4df（4x4 float 行列）を表示・編集するウィジェット．
 *
 * IView を継承し，4つの Float4View（各行）を子ウィジェットとして持つ．
 * 各行が縦に並んで表示される．
 */
class Matrix4dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値 単位行列）．
	 * @param name ラベル文字列．
	 */
	explicit Matrix4dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Matrix4df）．
	 */
	Matrix4dView(const std::string& name, const Math::Matrix4df& value);

	/**
	 * @brief Matrix4df 値をプログラムから設定する．
	 * @param value 設定する値．各行の Float4View に分配される．
	 */
	void setValue(const Math::Matrix4df& value);

	/**
	 * @brief 現在の Matrix4df 値を返す．
	 * @return 各行の Float4View から再構成した値．
	 */
	Math::Matrix4df getValue() const;

private:
	Float4View row1View; ///< 第1行．
	Float4View row2View; ///< 第2行．
	Float4View row3View; ///< 第3行．
	Float4View row4View; ///< 第4行．
};

	}
}
