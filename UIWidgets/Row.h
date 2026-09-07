#pragma once

#include "IView.h"

namespace Phantom {
	namespace UI {

/**
 * @brief 子ウィジェットを横一列に並べるコンテナ（宣言的構成用）．
 *
 * IView を継承し，onShow() で children を ImGui::SameLine() で繋ぎながら
 * 順に show() する．構成は add() で一度だけ登録する．
 *
 * 注意: 表示条件（setVisibleWhen()）で一部の子を隠した場合，隠れた子の
 * 直前に置いた SameLine が次の可視要素に効く．Phase 1 時点の利用箇所
 * （固定構成の操作ボタン列）ではこの副作用は起きない．
 */
class Row : public IView
{
public:
	explicit Row(const std::string& name = "Row") :
		IView(name)
	{}

	void onShow() override;
};

	}
}
