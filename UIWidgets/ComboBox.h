#pragma once

#include "IWindow.h"
#include <vector>
#include <string>

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
		return s_currentItem;
	}

	/**
	 * @brief インデックスを指定して選択状態を設定する．
	 * @param index 選択する項目のインデックス（0 始まり）．
	 */
	void setSelected(const int index) { s_currentItem = items[index].c_str(); }

private:
	std::vector<std::string> items; ///< 選択肢のリスト．
	const char* s_currentItem = nullptr; ///< 現在選択中の項目（items の要素を参照）．
};

	}
}
