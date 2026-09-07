#pragma once

#include "IWindow.h"

#include <functional>

namespace Phantom {
	namespace UI {

/**
 * @brief bool 値を表示・編集するウィジェット．
 *
 * ImGui::Checkbox を使ってチェックボックスを描画する．
 *
 * bind() で getter/setter を登録すると毎フレーム getter で最新値を取り込み、
 * ユーザーがトグルしたときだけ setter を呼ぶ（宣言的構成用）．未 bind なら
 * 従来どおり内部値を保持するだけ．
 */
class BoolView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ（初期値 false）．
	 * @param name ラベル文字列．
	 */
	explicit BoolView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値．
	 */
	BoolView(const std::string& name, const bool value);

	/**
	 * @brief チェックボックスを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 現在の bool 値を返す．
	 * @return 現在の値．
	 */
	bool getValue() const { return value; }

	/**
	 * @brief bool 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const bool value) { this->value = value; }

	/**
	 * @brief モデル値との双方向 binding を登録する．
	 * @param getter 描画時に呼び、表示する最新値を返す（状態は変更しない）．
	 * @param setter ユーザーがトグルしたときだけ呼ばれる．
	 */
	void bind(std::function<bool()> getter, std::function<void(bool)> setter) {
		getter_ = std::move(getter);
		setter_ = std::move(setter);
	}

private:
	bool value;
	std::function<bool()> getter_;
	std::function<void(bool)> setter_;
};

	}
}
