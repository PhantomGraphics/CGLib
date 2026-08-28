#pragma once

#include "IView.h"
#include "Button.h"
#include "StringView.h"

namespace Phantom {
	namespace UI {

/**
 * @brief ディレクトリパスを選択・表示するウィジェット．
 *
 * 「選択」ボタンと選択済みパスの表示フィールドを持つ．
 * ボタン押下時に tinyfiledialogs のディレクトリ選択ダイアログを呼び出す．
 */
class DirectoryView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期パスなし）．
	 * @param name ラベル文字列．
	 */
	explicit DirectoryView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期パス指定）．
	 * @param name ラベル文字列．
	 * @param path 初期表示するディレクトリパス．
	 */
	DirectoryView(const std::string& name, const std::string& path);

	/**
	 * @brief 現在選択されているディレクトリパスを返す．
	 * @return パス文字列．未選択の場合は空文字列．
	 */
	std::string getPath() const { return pathView.getValue(); }

	/**
	 * @brief ディレクトリパスをプログラムから設定する．
	 * @param path 設定するパス文字列．
	 */
	void setPath(const std::string& path) { this->pathView.setValue(path); }

private:
	/**
	 * @brief ディレクトリ選択ダイアログを開いて pathView に反映する．
	 */
	void onSelect();

private:
	Button selectButton;    ///< 「選択」ボタン．
	StringView pathView;    ///< 選択済みパスの表示フィールド．
};

	}
}
