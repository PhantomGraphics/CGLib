#pragma once

#include "IView.h"
#include "Button.h"
#include "StringView.h"

namespace Phantom {
	namespace UI {

/**
 * @brief ImGui ウィジェットとして埋め込めるファイルを保存する選択ビュー．
 *
 * 「選択」ボタンと選択済みファイル名の表示フィールドを持つ．
 * ボタン押下時に FileSaveDialog（OSネイティブダイアログ）を呼び出す．
 * FileSaveDialog と異なりモーダルではなく，ImGui レイアウト内に配置できる．
 */
class FileSaveView : public IView
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name ラベル文字列．
	 */
	explicit FileSaveView(const std::string& name);

	/**
	 * @brief ファイルフィルタを追加する．
	 * @param filter フィルタ文字列（例: "*.png"）．複数回呼べば複数フィルタを登録できる．
	 */
	void addFilter(const char* filter);

	/**
	 * @brief 選択（入力）されたファイル名を返す．
	 * @return ファイル名文字列．未選択の場合は空文字列．
	 */
	std::string getFileName() const { return fileNameView.getValue(); }

protected:
	/**
	 * @brief ダイアログを開いてファイル名を選択し，fileNameView に反映する．
	 */
	void onSelect();

private:
	Button selectButton;                ///< 「選択」ボタン．
	StringView fileNameView;            ///< 選択済みファイル名の表示フィールド．
	std::vector< char const* > filters; ///< 登録済みフィルタのリスト．
};

	}
}
