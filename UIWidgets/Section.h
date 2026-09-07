#pragma once

#include "IView.h"

namespace Phantom {
	namespace UI {

/**
 * @brief 見出し付きの折り畳み可能なグループ（宣言的構成用）．
 *
 * ImGui::CollapsingHeader でヘッダーを描画し，開いている間だけ children を
 * 順に show() する．見出し文字列（name）と初期開閉状態を構築時に登録する．
 * 開閉状態は Dear ImGui が imgui.ini に永続化する．
 */
class Section : public IView
{
public:
	/**
	 * @param title       ヘッダーに表示する見出し（ImGui ID も兼ねる）．
	 * @param defaultOpen 初期状態で開いておくか（既定 true）．
	 */
	explicit Section(const std::string& title, bool defaultOpen = true) :
		IView(title),
		defaultOpen_(defaultOpen)
	{}

	void onShow() override;

private:
	bool defaultOpen_;
};

	}
}
