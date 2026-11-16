#include "../src/hud_menu_visibility.h"

#include <cassert>

int main()
{
	using HudMenuVisibility::IsPassiveOverlay;
	assert(IsPassiveOverlay(true, false, false, false, false));
	assert(!IsPassiveOverlay(false, false, false, false, false));
	assert(!IsPassiveOverlay(true, true, false, false, false));
	assert(!IsPassiveOverlay(true, false, true, false, false));
	assert(!IsPassiveOverlay(true, false, false, true, false));
	assert(!IsPassiveOverlay(true, false, false, false, true));
}
