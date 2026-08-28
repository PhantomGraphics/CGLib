#pragma once

#include "IMenuItem.h"
#include <functional>

namespace Phantom {
	namespace UI {

/**
 * @brief std::function コールバックを持つメニュー項目．
 *
 * IMenuItem を継承し，選択時に登録されたコールバックを呼ぶ．
 * IMenu の子として追加して使用する．
 */
class MenuItem : public IMenuItem
{
public:
	/**
	 * @brief コンストラクタ（コールバックなし）．
	 * @param name メニュー項目のラベル文字列．
	 */
	explicit MenuItem(const std::string& name) :
		IMenuItem(name)
	{}

	/**
	 * @brief コンストラクタ（コールバック付き）．
	 * @param name ラベル文字列．
	 * @param func 項目選択時に呼ばれるコールバック．
	 */
	MenuItem(const std::string& name, std::function<void()> func) :
		IMenuItem(name),
		func(func)
	{}

	~MenuItem()
	{}

	/**
	 * @brief 選択時に呼ぶコールバックを設定する．
	 * @param func 設定するコールバック（既存のコールバックは上書きされる）．
	 */
	void setFunction(std::function<void(void)> func) {
		this->func = std::move(func);
	}

protected:
	/**
	 * @brief 項目が選択されたときに登録済みコールバックを呼ぶ．
	 */
	void onPushed() override {
		this->func();
	}

private:
	std::function<void(void)> func; ///< 選択時コールバック．
};

	}

}
