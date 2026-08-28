#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief 複数の子ウィジェットを縦に並べる汎用ビュー基底クラス．
 *
 * IWindow を継承し，onShow() で children を順番に show() する．
 * 複合ウィジェット（Matrix4dView, Box3dView など）の基底として使用される．
 */
class IView : public IWindow
{
public:
	/**
	 * @brief コンストラクタ．
	 * @param name ウィジェットのラベル文字列．
	 */
	explicit IView(const std::string& name) :
		IWindow(name)
	{}

	virtual ~IView() {};

	/**
	 * @brief 子ウィジェットを順番に描画する．
	 */
	void onShow() override {
		for (auto c : children) {
			c->show();
		}
	}
};

	}
}
