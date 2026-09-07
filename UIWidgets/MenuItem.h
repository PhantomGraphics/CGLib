#pragma once

#include "IMenuItem.h"
#include <functional>
#include <string>
#include <utility>

namespace Phantom {
	namespace UI {

/**
 * @brief std::function コールバックを持つメニュー項目．
 *
 * IMenuItem を継承し，選択時に登録されたコールバックを呼ぶ．
 * 選択状態・有効状態・ツールチップは任意の provider で登録でき，描画時に
 * 評価される（宣言的構成用）．いずれも未登録なら従来どおり
 * 「非選択・有効・ツールチップなし」．IMenu の子として追加して使用する．
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
		func(std::move(func))
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

	/** @brief チェックマーク表示の provider を登録する． */
	void setSelected(std::function<bool()> fn) { selected_ = std::move(fn); }

	/** @brief 有効/無効の provider を登録する． */
	void setEnabled(std::function<bool()> fn) { enabled_ = std::move(fn); }

	/** @brief 固定ツールチップを登録する（空文字で解除）． */
	void setTooltip(std::string text) {
		tooltip_ = std::move(text);
		tooltipProvider_ = nullptr;
	}

	/** @brief ツールチップ文字列を毎フレーム供給する provider を登録する． */
	void setTooltip(std::function<std::string()> fn) { tooltipProvider_ = std::move(fn); }

protected:
	/**
	 * @brief 項目が選択されたときに登録済みコールバックを呼ぶ．
	 */
	void onPushed() override {
		if (this->func) this->func();
	}

	bool isSelected() const override { return selected_ ? selected_() : false; }
	bool isEnabled()  const override { return enabled_  ? enabled_()  : true; }
	std::string tooltipText() const override {
		return tooltipProvider_ ? tooltipProvider_() : tooltip_;
	}

private:
	std::function<void(void)>        func;            ///< 選択時コールバック．
	std::function<bool()>            selected_;       ///< 任意のチェック状態 provider．
	std::function<bool()>            enabled_;        ///< 任意の有効状態 provider．
	std::string                      tooltip_;        ///< 固定ツールチップ文字列．
	std::function<std::string()>     tooltipProvider_;///< 任意のツールチップ provider．
};

	}

}
