#pragma once

#include "IMenu.h"

namespace Phantom {
	namespace UI {

/**
 * @brief メニューバー上の 1 メニュー（"File" / "Edit" 等）の具象クラス．
 *
 * IMenu の BeginMenu/EndMenu 実装をそのまま使い，子に MenuItem / Separator /
 * 入れ子の Menu を add() する．MainMenuBar の子として登録する．
 */
class Menu : public IMenu
{
public:
	explicit Menu(const std::string& name) :
		IMenu(name)
	{}
};

	}
}
