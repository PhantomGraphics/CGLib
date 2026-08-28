#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief ImGui の子ウィンドウ（BeginChild/EndChild）でラップする単一子コンテナ．
 *
 * 子ウィジェットを1つだけ保持し，スクロール可能な子ウィンドウ内に描画する．
 * setChild() で子を差し替えると以前の子は削除される．
 */
class Panel : public IWindow
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name パネルのラベル文字列（ImGui のウィンドウIDとして使用）．
	 */
	explicit Panel(const std::string& name) :
		IWindow(name)
	{}

	//virtual ~Panel() = default;

	/**
	 * @brief 子ウィジェットを設定する．既存の子はすべて削除される．
	 * @param window 新たに設定する子ウィジェットのポインタ（所有権は移さない）．
	 */
	void setChild(IWindow* window) {
		this->children.clear();
		this->children.push_back(window);
	}

	/**
	 * @brief BeginChild/EndChild で囲んだ領域に子ウィジェットを描画する．
	 */
	void onShow() override;

private:
};

	}
}
