#pragma once

#include "IWindow.h"
#include <functional>

namespace Phantom {
	namespace UI {

/**
 * @brief クリック可能なボタンウィジェット．
 *
 * ImGui::Button を使って描画し，押下時に登録されたコールバックを呼ぶ．
 */
class Button : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（コールバックなし）．
	 * @param name ボタンのラベル文字列．
	 */
	explicit Button(const std::string& name) :
		IWindow(name)
	{}

	/**
	 * @brief コンストラクタ（コールバック付き）．
	 * @param name ボタンのラベル文字列．
	 * @param func ボタン押下時に呼ばれるコールバック．
	 */
	Button(const std::string& name, std::function<void()> func) :
		IWindow(name),
		func(func)
	{}

	~Button()
	{}

	/**
	 * @brief ボタンを描画し，押下時にコールバックを呼ぶ．
	 */
	void onShow() override;

	/**
	 * @brief 押下時に呼ぶコールバックを設定する．
	 * @param func 設定するコールバック（既存のコールバックは上書きされる）．
	 */
	void setFunction(std::function<void(void)> func) {
		this->func = std::move(func);
	}

private:
	std::function<void(void)> func; ///< 押下時コールバック．
};

	}
}
