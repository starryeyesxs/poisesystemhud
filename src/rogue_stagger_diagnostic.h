#pragma once

#include <string>

namespace RE
{
	class Actor;
	class HitData;
	class MagicTarget;
}

namespace RogueStaggerCapture
{
	void Install();
	void NotePhysicalHit(RE::Actor* target, RE::Actor* aggressor, const RE::HitData* data);
	void NotePhysicalHitComplete(RE::Actor* target, const RE::HitData* data);
	void NoteMagicEffect(RE::MagicTarget* target, RE::MagicTarget::AddTargetData* data);
	[[nodiscard]] std::string LatestAlert();
}
