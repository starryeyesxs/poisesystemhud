#pragma once

namespace HudMenuVisibility
{
	inline bool IsPassiveOverlay(bool alwaysOpen, bool pausesGame, bool usesMenuContext,
		bool usesCursor, bool applicationMenu)
	{
		return alwaysOpen && !pausesGame && !usesMenuContext && !usesCursor && !applicationMenu;
	}
}
