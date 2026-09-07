#pragma once

#include "IWindow.h"

#include <functional>
#include <string>

namespace Phantom {
	namespace UI {

/**
 * @brief 1 行（複数行可）のテキスト表示ウィジェット（宣言的構成用）．
 *
 * 固定文字列，またはフレームごとに評価する provider のどちらかで構築する．
 * provider は描画時に最新の文字列を返すだけで，モデルの状態は変更しない．
 * スタイルは通常・淡色（無効表示）・折り返しから選ぶ．
 */
class Label : public IWindow
{
public:
	enum class Style { Normal, Disabled, Wrapped };

	explicit Label(std::string text, Style style = Style::Normal) :
		IWindow("Label"),
		text_(std::move(text)),
		style_(style)
	{}

	explicit Label(std::function<std::string()> provider, Style style = Style::Normal) :
		IWindow("Label"),
		provider_(std::move(provider)),
		style_(style)
	{}

	/** @brief 固定文字列を差し替える（provider は解除される）． */
	void setText(std::string text) { text_ = std::move(text); provider_ = nullptr; }

	void onShow() override;

private:
	std::string text_;
	std::function<std::string()> provider_;
	Style style_;
};

	}
}
