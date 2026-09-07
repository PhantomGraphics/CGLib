#pragma once

#include "IView.h"

#include "Vector3dView.h"

#include "../Math/Box3d.h"

#include <functional>

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Box3df（3D軸平行バウンディングボックス）を表示・編集するウィジェット．
 *
 * IView を継承し，最小点（min）と最大点（max）の2つの Vector3dView を子として持つ．
 *
 * bind() で getter/setter を登録すると，描画前に getter でモデルの最新値を取り込み，
 * ユーザーが min/max を編集したフレームだけ setter を呼ぶ（宣言的構成用）．
 * 未 bind なら従来どおり setValue/getValue で手動同期する．
 */
class Box3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Box3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Box3df）．
	 */
	Box3dView(const std::string& name, const Math::Box3df& value);

	/**
	 * @brief 現在の Box3df 値を返す．
	 * @return min / max の各 Vector3dView から再構成した値．
	 */
	Math::Box3df getValue() const;

	/**
	 * @brief Box3df 値をプログラムから設定する．
	 * @param value 設定する値．min / max が各 Vector3dView に分配される．
	 */
	void setValue(const Math::Box3df& value);

	/**
	 * @brief モデル値との双方向 binding を登録する．
	 */
	void bind(std::function<Math::Box3df()> getter,
	          std::function<void(const Math::Box3df&)> setter) {
		getter_ = std::move(getter);
		setter_ = std::move(setter);
	}

	void onShow() override;

private:
	Vector3dView minView; ///< バウンディングボックスの最小点．
	Vector3dView maxView; ///< バウンディングボックスの最大点．
	std::function<Math::Box3df()> getter_;
	std::function<void(const Math::Box3df&)> setter_;
};

	}
}
