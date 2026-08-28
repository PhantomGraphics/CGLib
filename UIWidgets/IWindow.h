#pragma once

#include <string>
#include <list>
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
	 * @brief このウィジェットを描画する．onShow() を呼び出す．
	 */
	void show() { onShow(); }

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
};

	}
}
