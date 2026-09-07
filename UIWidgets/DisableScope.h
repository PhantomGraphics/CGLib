#pragma once

#include "IView.h"

#include <functional>
#include <string>

namespace Phantom {
	namespace UI {

/**
 * @brief 子ウィジェットを条件付きで無効化（淡色・非操作）するコンテナ．
 *
 * setDisabledWhen() の述語が真なら ImGui::BeginDisabled/EndDisabled で囲んで
 * 子を描画する．setTooltip() を登録すると、囲んだ最後の項目をホバーしたとき
 * （無効時も含む）にツールチップを出す．いずれも構築時に一度だけ登録する．
 */
class DisableScope : public IView
{
public:
	explicit DisableScope(const std::string& name = "DisableScope") :
		IView(name)
	{}

	void setDisabledWhen(std::function<bool()> fn) { disabledWhen_ = std::move(fn); }
	void setTooltip(std::string text) { tooltip_ = std::move(text); tooltipProvider_ = nullptr; }
	void setTooltip(std::function<std::string()> fn) { tooltipProvider_ = std::move(fn); }

	void onShow() override;

private:
	std::function<bool()>        disabledWhen_;
	std::string                  tooltip_;
	std::function<std::string()> tooltipProvider_;
};

	}
}
