#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief メニュー内の1項目を表す基底クラス．
 *
 * ImGui の MenuItem に対応する．
 * 項目が選択されたとき onPushed() が呼ばれる．
 * 具体的な処理は派生クラス（MenuItem）で実装する．
 */
class IMenuItem : public IWindow
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name メニュー項目のラベル文字列．
	 */
	explicit IMenuItem(const std::string& name) :
		IWindow(name)
	{}

	virtual ~IMenuItem()
	{}

	/**
	 * @brief MenuItem を描画し，選択時に onPushed() を呼ぶ．
	 */
	void onShow() override;

protected:
	/**
	 * @brief メニュー項目が選択されたときに呼ばれる純粋仮想関数．
	 */
	virtual void onPushed() = 0;

private:
};

	}
}
