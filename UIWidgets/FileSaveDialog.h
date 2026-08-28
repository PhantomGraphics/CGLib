#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace Phantom {
	namespace UI {

/**
 * @brief ネイティブのファイルを保存するダイアログ．
 *
 * tinyfiledialogs を使ってOSネイティブのファイル保存ダイアログを表示する．
 * IWindow を継承しないスタンドアロンクラス．
 * show() を呼ぶとモーダルダイアログが開き，確定後に getFileName() / getFilePath() で結果を取得する．
 */
class FileSaveDialog
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name ダイアログのタイトル文字列．
	 */
	explicit FileSaveDialog(const std::string& name);

	/**
	 * @brief ファイルフィルタを追加する．
	 * @param filter フィルタ文字列（例: "*.png"）．複数回呼べば複数フィルタを登録できる．
	 */
	void addFilter(char const* filter);

	/**
	 * @brief ファイル保存ダイアログをモーダルで表示する．
	 *
	 * ユーザがファイル名を入力・確定するかキャンセルするまでブロックする．
	 */
	void show();

	/**
	 * @brief 入力されたファイル名（パスなし）を返す．
	 * @return ファイル名文字列．キャンセル時は空文字列．
	 */
	std::string getFileName() const;

	/**
	 * @brief 入力されたファイルのフルパスを返す．
	 * @return ファイルパス．キャンセル時は空のパス．
	 */
	std::filesystem::path getFilePath() const;

private:
	std::vector< char const* > filters; ///< 登録済みフィルタのリスト．
	char const* filename; ///< 入力されたファイルパス（tinyfiledialogs が返す文字列）．
	char const* name;     ///< ダイアログタイトル．
};

	}
}
