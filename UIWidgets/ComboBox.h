#pragma once

#include "IWindow.h"
#include <vector>
#include <string>
#include <functional>

namespace Phantom {
	namespace UI {

/**
 * @brief 選択肢を持つドロップダウンリスト（コンボボックス）ウィジェット．
 *
 * ImGui::BeginCombo / EndCombo を使って描画する．
 * addItem() で選択肢を追加し，getSelectedItem() で選択中の項目を取得する．
 */
class ComboBox : public IWindow
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name ラベル文字列．
	 */
	explicit ComboBox(const std::string& name) :
		IWindow(name)
	{}

	~ComboBox()
	{}

	/**
	 * @brief コンボボックスを描画する．
	 */
	void onShow() override;

	/**
	 * @brief 選択肢を末尾に追加する．
	 * @param item 追加する選択肢の文字列．
	 */
	void addItem(const std::string& item) {
		items.push_back(item);
	}

	/**
	 * @brief 現在選択されている項目の文字列を返す．
	 * @return 選択中の項目文字列．未選択の場合は空文字列．
	 */
	std::string getSelectedItem() const {
		return selectedIndex >= 0 && selectedIndex < static_cast<int>(items.size())
			? items[static_cast<std::size_t>(selectedIndex)] : std::string{};
	}

	/**
	 * @brief インデックスを指定して選択状態を設定する．
	 * @param index 選択する項目のインデックス（0 始まり）．
	 */
	void setSelected(const int index) {
		selectedIndex = index >= 0 && index < static_cast<int>(items.size()) ? index : -1;
	}

	/**
	 * @brief 現在の選択インデックスを返す（未選択なら -1）．
	 */
	int getSelectedIndex() const { return selectedIndex; }

	/**
	 * @brief モデルとの binding を登録する（宣言的構成用）．
	 * @param getter 描画時に呼び，選択すべきインデックスを返す（状態は変更しない）．
	 * @param setter ユーザーが別の項目を選んだときだけ呼ばれる．
	 */
	void bind(std::function<int()> getter, std::function<void(int)> setter) {
		indexGetter_ = std::move(getter);
		indexSetter_ = std::move(setter);
	}

	/**
	 * @brief 選択が変化したときに呼ばれるコールバックを登録する．
	 *        bind() の setter と併用可（両方呼ばれる）．
	 */
	void setOnChange(std::function<void(int)> fn) { onChange_ = std::move(fn); }

private:
	std::vector<std::string> items; ///< 選択肢のリスト．
	int selectedIndex = -1; ///< 現在選択中の項目。文字列ポインターの無効化を避けるため添字で保持する。
	std::function<int()> indexGetter_;      ///< 任意のモデル getter．
	std::function<void(int)> indexSetter_;  ///< 任意のモデル setter（変更時のみ）．
	std::function<void(int)> onChange_;     ///< 任意の変更通知．
};

	}
}
