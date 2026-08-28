#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief メニューバーのメニュー項目グループを表す基底クラス．
 *
 * ImGui の BeginMenu / EndMenu に対応する．
 * 子に IMenuItem を追加して使用する．
 */
class IMenu : public IWindow
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name メニューのラベル文字列（メニューバー上に表示される名前）．
	 */
	IMenu(const std::string& name) :
		IWindow(name)
	{}

	virtual ~IMenu() {};

protected:
	/**
	 * @brief メニュー本体を描画する．
	 *
	 * BeginMenu / EndMenu を使って子ウィジェットを描画する．
	 */
	void onShow() override;

private:
};

	}
}
