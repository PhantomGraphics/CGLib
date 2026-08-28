#pragma once

#include "IView.h"
#include "imgui.h"
#include "FloatView.h"
#include "IntView.h"
#include "../Graphics/ColorMap.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Graphics::ColorMap（カラーマップ）を設定するウィジェット．
 *
 * 解像度（resolution），最小値（minValue），最大値（maxValue）の
 * 3つのサブビューを持ち，カラーマップのパラメータを対話的に設定できる．
 */
class ColorMapView : public IView
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name ラベル文字列．
	 */
	explicit ColorMapView(const std::string& name);

	/**
	 * @brief カラーマップ設定ウィジェットを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在設定されているカラーマップを返す．
	 * @return 現在の Graphics::ColorMap 値．
	 */
	Graphics::ColorMap getValue() const { return value; }

private:
	Graphics::ColorMap value; ///< 管理するカラーマップ．
	IntView resolution;       ///< カラーマップ解像度の入力フィールド．
	FloatView minValue;       ///< カラーマップ最小値の入力フィールド．
	FloatView maxValue;       ///< カラーマップ最大値の入力フィールド．
};

	}
}
