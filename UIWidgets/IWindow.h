#pragma once

#include <string>
#include <list>
#include <functional>
#include "../Util/UnCopyable.h"

namespace Phantom {
	namespace UI {

/**
 * @brief すべてのUIウィジェットの基底クラス．
 *
 * Compositeパターンを採用しており，子ウィジェットのリストを保持する．
 * コピー不可（UnCopyable 継承）．
 * ImGuiフレームのレンダリングループ内で show() を呼ぶことで描画される．
 */
class IWindow : private UnCopyable
{
protected:
	/**
	 * @brief コンストラクタ．
	 * @param name ウィジェットのラベル文字列（ImGuiウィンドウID兼表示名）．
	 */
	explicit IWindow(const std::string& name) :
		name(name)
	{}

	virtual ~IWindow() {};

public:
	/**
	 * @brief 子ウィジェットを末尾に追加する．
	 * @param child 追加する子ウィジェットのポインタ（所有権は移さない）．
	 */
	void add(IWindow* child) { children.push_back(child); }

	/**
	 * @brief 子ウィジェットリストをすべて削除する．
	 */
	void clear() { children.clear(); }

	/**
	 * @brief このウィジェットを描画する．
	 *
	 * 表示条件（setVisibleWhen()）が登録されていて偽を返す場合は onShow() を
	 * 呼ばずに何も描画しない．条件が未登録なら常に描画する（既定動作）．
	 * 条件は構築時に一度だけ登録し，毎フレームこの場で評価される（状態は変更しない）．
	 */
	void show() {
		if (visibleWhen_ && !visibleWhen_()) return;
		onShow();
	}

	/**
	 * @brief 表示条件を登録する（宣言的構成用）．
	 * @param fn 真を返すと描画，偽を返すと当該フレームは描画しない述語．
	 *           nullptr を渡すと条件を解除する（常に描画）．
	 */
	void setVisibleWhen(std::function<bool()> fn) { visibleWhen_ = std::move(fn); }

	/**
	 * @brief 派生クラスで実装する描画処理．
	 *
	 * ImGuiの描画コールをここに記述する．
	 * IView は子を順に show() する実装を提供する．
	 */
	virtual void onShow() = 0;

protected:
	std::string name;           ///< ウィジェットのラベル文字列．
	std::list<IWindow*> children; ///< 子ウィジェットリスト（所有権なし）．
	std::function<bool()> visibleWhen_; ///< 任意の表示条件．未設定なら常に描画．
};

	}
}
