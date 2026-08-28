#pragma once

#include "IWindow.h"
#include "Button.h"

namespace Phantom {
	namespace UI {

/**
 * @brief OK / Cancel ボタンを持つダイアログビューの基底クラス．
 *
 * 子ウィジェット → OK ボタン → Cancel ボタンの順に描画する．
 * OK 押下で onOk()，Cancel 押下で onCancel() が呼ばれる．
 * 具体的なダイアログは本クラスを継承して onOk() を実装する．
 */
class IOkCancelView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ．OK / Cancel ボタンにコールバックを設定する．
	 * @param name ダイアログのラベル文字列．
	 */
	IOkCancelView(const std::string& name) :
		IWindow(name),
		ok("Ok"),
		cancel("Cancel")
	{
		ok.setFunction([=]() { onOk(); });
		cancel.setFunction([=]() { onCancel(); });
	}

	virtual ~IOkCancelView() {}

protected:
	/**
	 * @brief 子ウィジェット，OK ボタン，Cancel ボタンを順に描画する．
	 */
	virtual void onShow() {
		for (auto c : children) {
			c->show();
		}
		ok.show();
		cancel.show();
	}

	/**
	 * @brief OK ボタンが押されたときに呼ばれる純粋仮想関数．
	 */
	virtual void onOk() = 0;

	/**
	 * @brief Cancel ボタンが押されたときに呼ばれる仮想関数．デフォルトは何もしない．
	 */
	virtual void onCancel() {};

private:
	Button ok;     ///< OK ボタン．
	Button cancel; ///< Cancel ボタン．
};

	}
}
