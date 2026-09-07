#pragma once

#include "IWindow.h"

#include <functional>
#include <string>

namespace Phantom {
	namespace UI {

/**
 * @brief float 値をスライダーで編集するウィジェット（宣言的構成用）．
 *
 * ImGui::SliderFloat．範囲・書式は構築時に固定．bind() で getter/setter を
 * 登録すると毎フレーム getter で最新値を取り込み、ユーザー編集時のみ setter を
 * 呼ぶ（未 bind なら内部値のみ保持）．
 */
class FloatSlider : public IWindow
{
public:
	FloatSlider(const std::string& name, float minValue, float maxValue,
	            std::string format = "%.3f") :
		IWindow(name),
		min_(minValue),
		max_(maxValue),
		format_(std::move(format))
	{}

	float getValue() const { return value_; }
	void  setValue(float v) { value_ = v; }

	void bind(std::function<float()> getter, std::function<void(float)> setter) {
		getter_ = std::move(getter);
		setter_ = std::move(setter);
	}

	void onShow() override;

private:
	float value_ = 0.f;
	float min_;
	float max_;
	std::string format_;
	std::function<float()> getter_;
	std::function<void(float)> setter_;
};

	}
}
