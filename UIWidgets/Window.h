#pragma once

#include "IView.h"

namespace Phantom {
	namespace UI {

/**
 * @brief トップレベルウィンドウ（宣言的構成用）．
 *
 * ImGui::Begin / End を内部で管理し，開いている間だけ子ウィジェットを順に
 * show() する．タイトル・初期位置・初期サイズ・開閉フラグ（閉じるボタンの
 * 連携先）を構築時に登録する．
 *
 * `Immediate::beginWindow()`/`endWindow()` と同じ「Begin は必ず End と対」の
 * 契約に従う（Begin が false を返しても End は呼ぶ）．
 */
class Window : public IView
{
public:
	explicit Window(const std::string& title) :
		IView(title)
	{}

	/**
	 * @brief タイトルバーの閉じるボタンの連携先を登録する．
	 *        非 nullptr かつ *open == false のときはウィンドウを描画しない．
	 */
	void setOpenFlag(bool* open) { open_ = open; }

	/** @brief 初回のみ適用される初期位置． */
	void setInitialPosition(float x, float y) { hasPos_ = true; posX_ = x; posY_ = y; }

	/** @brief 初回のみ適用される初期サイズ． */
	void setInitialSize(float width, float height) { hasSize_ = true; width_ = width; height_ = height; }

	void onShow() override;

private:
	bool*  open_    = nullptr;
	bool   hasPos_  = false;
	bool   hasSize_ = false;
	float  posX_ = 0.f, posY_ = 0.f;
	float  width_ = 0.f, height_ = 0.f;
};

	}
}
