#pragma once

#include <string>
#include <filesystem>
#include <vector>

namespace Phantom {
	namespace UI {

/**
 * @brief ネイティブのファイルを開くダイアログ．
 *
 * tinyfiledialogs を使ってOSネイティブのファイル選択ダイアログを表示する．
 * IWindow を継承しないスタンドアロンクラス．
 * show() を呼ぶとモーダルダイアログが開き，選択後に getFileName() / getFilePath() で結果を取得する．
 */
class FileOpenDialog
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name ダイアログのタイトル文字列．
	 */
	explicit FileOpenDialog(const std::string& name);

	/**
	 * @brief ファイルフィルタを追加する．
	 * @param filter フィルタ文字列（例: "*.png"）．複数回呼べば複数フィルタを登録できる．
	 */
	void addFilter(char const* filter);

	/**
	 * @brief ファイル選択ダイアログをモーダルで表示する．
	 *
	 * ユーザがファイルを選択するかキャンセルするまでブロックする．
	 */
	void show();

	/**
	 * @brief 選択されたファイル名（パスなし）を返す．
	 * @return ファイル名文字列．キャンセル時は空文字列．
	 */
	std::string getFileName() const;

	/**
	 * @brief 選択されたファイルのフルパスを返す．
	 * @return ファイルパス．キャンセル時は空のパス．
	 */
	std::filesystem::path getFilePath() const;

private:
	std::vector< char const* > filters; ///< 登録済みフィルタのリスト．
	char const* filename; ///< 選択されたファイルパス（tinyfiledialogs が返す文字列）．
	char const* name;     ///< ダイアログタイトル．
};

	}
}
