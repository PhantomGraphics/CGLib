#pragma once

#include "IWindow.h"

#include <functional>

namespace Phantom {
	namespace UI {

/**
 * @brief float 値を表示・編集するウィジェット．
 *
 * ImGui::InputFloat を使って入力フィールドを描画する．
 *
 * bind() で getter/setter を登録すると，毎フレーム getter でモデルの最新値を
 * 取り込み，ユーザーが実際に値を編集したときだけ setter を呼ぶ（宣言的構成用）．
 * bind() を呼ばなければ従来どおり内部値を保持するだけで挙動は変わらない．
 */
class FloatView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 0.0f）．
	 * @param name ラベル文字列．
	 */
	explicit FloatView(const std::string& name) :
		FloatView(name, 0.0f)
	{}

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値．
	 */
	FloatView(const std::string& name, const float value) :
		IWindow(name),
		value(value)
	{}

	/**
	 * @brief 入力フィールドを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在の float 値を返す．
	 * @return 現在の値．
	 */
	float getValue() const { return value; }

	/**
	 * @brief float 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const float value) { this->value = value; }

	/**
	 * @brief モデル値との双方向 binding を登録する．
	 * @param getter 描画時に呼び，表示する最新値を返す（状態は変更しない）．
	 * @param setter ユーザーが値を編集したときだけ呼ばれる．
	 */
	void bind(std::function<float()> getter, std::function<void(float)> setter) {
		getter_ = std::move(getter);
		setter_ = std::move(setter);
	}

private:
	float value;
	std::function<float()> getter_;
	std::function<void(float)> setter_;
};

	}
}
