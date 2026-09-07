#pragma once

#include "IView.h"

#include <functional>
#include <string>

namespace Phantom {
	namespace UI {

/**
 * @brief 子ウィジェットを ImGui の ID スタックスコープで囲むコンテナ．
 *
 * PushID(id) → children を順に show() → PopID() する．ID は固定文字列でも
 * provider（毎フレーム評価）でも登録できる．ページ切替のように表示内容が
 * 入れ替わる箇所で、項目ラベルの衝突を避けつつ ID を安定させるのに使う．
 */
class IdScope : public IView
{
public:
	explicit IdScope(std::string id) :
		IView(std::move(id))
	{}

	explicit IdScope(std::function<std::string()> idProvider) :
		IView("IdScope"),
		idProvider_(std::move(idProvider))
	{}

	void setId(std::string id) { name = std::move(id); idProvider_ = nullptr; }

	void onShow() override;

private:
	std::function<std::string()> idProvider_;
};

	}
}
