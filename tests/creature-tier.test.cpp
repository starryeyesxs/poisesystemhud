#include "../src/creature_tier.h"

#include <cassert>

int main()
{
	using namespace BuiltinPoise;
	const RaceTraits creature;
	RaceTraits npc;
	npc.npc = true;
	RaceTraits dragon;
	dragon.dragon = true;
	RaceTraits giant;
	giant.giant = true;

	assert(ClassifyRace("WolfRace", creature) == CreatureTier::Light);
	assert(ClassifyRace("IceWolfRace", creature) == CreatureTier::Light);
	assert(ClassifyRace("SkeeverRace", creature) == CreatureTier::Light);
	assert(ClassifyRace("MudcrabRace", creature) == CreatureTier::Light);
	assert(ClassifyRace("FoxRace", creature) == CreatureTier::Light);
	assert(ClassifyRace("HorkerRace", creature) == CreatureTier::Light);

	assert(ClassifyRace("DraugrRace", creature) == CreatureTier::Medium);
	assert(ClassifyRace("DraugrRace", npc) == CreatureTier::Medium);
	assert(ClassifyRace("FalmerRace", npc) == CreatureTier::Medium);
	assert(ClassifyRace("DLC1SkeletonRace", creature) == CreatureTier::Medium);
	assert(ClassifyRace("ChaurusRace", creature) == CreatureTier::Medium);
	assert(ClassifyRace("SabreCatRace", creature) == CreatureTier::Medium);
	assert(ClassifyRace("FrostbiteSpiderRace", creature) == CreatureTier::Medium);
	assert(ClassifyRace("FrostbiteSpiderRaceGiant", creature) == CreatureTier::Medium);
	assert(ClassifyRace("IceWraithRace", creature) == CreatureTier::Medium);
	assert(ClassifyRace("SprigganRace", creature) == CreatureTier::Medium);

	assert(ClassifyRace("BearRace", creature) == CreatureTier::Heavy);
	assert(ClassifyRace("TrollRace", creature) == CreatureTier::Heavy);
	assert(ClassifyRace("HagravenRace", creature) == CreatureTier::Heavy);
	assert(ClassifyRace("GargoyleRace", creature) == CreatureTier::Heavy);
	assert(ClassifyRace("DwarvenSphereRace", creature) == CreatureTier::Heavy);
	assert(ClassifyRace("ChaurusHunterRace", creature) == CreatureTier::Heavy);
	assert(ClassifyRace("WerewolfBeastRace", creature) == CreatureTier::Heavy);

	assert(ClassifyRace("GiantRace", creature) == CreatureTier::Boss);
	assert(ClassifyRace("MammothRace", creature) == CreatureTier::Boss);
	assert(ClassifyRace("DwarvenCenturionRace", creature) == CreatureTier::Boss);
	assert(ClassifyRace("DLC2LurkerRace", creature) == CreatureTier::Boss);
	assert(ClassifyRace("DragonRace", creature) == CreatureTier::Boss);
	assert(ClassifyRace("DragonPriestRace", npc) == CreatureTier::Boss);
	assert(ClassifyRace("SomeModdedWyrm", dragon) == CreatureTier::Boss);
	assert(ClassifyRace("SomeModdedTitan", giant) == CreatureTier::Boss);

	assert(ClassifyRace("RabbitRace", creature) == CreatureTier::Excluded);
	assert(ClassifyRace("ChickenRace", creature) == CreatureTier::Excluded);
	assert(ClassifyRace("SlaughterfishRace", creature) == CreatureTier::Excluded);
	assert(ClassifyRace("ButterflyRace", creature) == CreatureTier::Excluded);
	assert(ClassifyRace("DragonflyRace", creature) == CreatureTier::Excluded);
	assert(ClassifyRace("MothRace", creature) == CreatureTier::Excluded);
	assert(ClassifyRace("TorchBugRace", creature) == CreatureTier::Excluded);

	// Giant mudcrabs stay light and a mammoth is not mistaken for a moth.
	assert(ClassifyRace("GiantMudcrabRace", creature) == CreatureTier::Light);
	assert(ClassifyRace("MammothRace", creature) != CreatureTier::Excluded);

	assert(ClassifyRace("NordRace", npc) == CreatureTier::Standard);
	assert(ClassifyRace("ImperialRace", npc) == CreatureTier::Standard);
	assert(ClassifyRace("HorseRace", creature) == CreatureTier::Unknown);
	assert(ClassifyRace("", creature) == CreatureTier::Unknown);
	assert(ClassifyRace("CustomModBeastRace", creature) == CreatureTier::Unknown);

	assert(IsDragonPriestName("Dragon Priest"));
	assert(IsDragonPriestName("Hevnoraak"));
	assert(IsDragonPriestName("Otar the Mad"));
	assert(!IsDragonPriestName("Whiterun Guard"));
	assert(!IsDragonPriestName(""));

	// Race tier wins over the NPC keyword while the creature toggle is on.
	assert(ResolveActorTier(CreatureTier::Medium, true, "", true) == CreatureTier::Medium);
	assert(ResolveActorTier(CreatureTier::Standard, true, "Dragon Priest", true) == CreatureTier::Boss);
	assert(ResolveActorTier(CreatureTier::Standard, true, "Whiterun Guard", true) == CreatureTier::Standard);
	assert(ResolveActorTier(CreatureTier::Heavy, false, "", true) == CreatureTier::Heavy);
	assert(ResolveActorTier(CreatureTier::Unknown, false, "", true) == CreatureTier::Unknown);
	assert(ResolveActorTier(CreatureTier::Standard, false, "", true) == CreatureTier::Unknown);
	assert(ResolveActorTier(CreatureTier::Unknown, true, "", true) == CreatureTier::Standard);
	assert(ResolveActorTier(CreatureTier::Excluded, false, "", true) == CreatureTier::Excluded);

	// Ice Wolves share WolfRace, so only the display name promotes them from Light to Medium.
	assert(IsIceWolfName("Ice Wolf"));
	assert(IsIceWolfName("ice wolf"));
	assert(!IsIceWolfName("Wolf"));
	assert(!IsIceWolfName(""));
	assert(ResolveActorTier(CreatureTier::Light, false, "Ice Wolf", true) == CreatureTier::Medium);
	assert(ResolveActorTier(CreatureTier::Light, false, "Wolf", true) == CreatureTier::Light);
	assert(ResolveActorTier(CreatureTier::Light, false, "Skeever", true) == CreatureTier::Light);
	assert(ResolveActorTier(CreatureTier::Heavy, false, "Ice Wolf", true) == CreatureTier::Heavy);
	assert(ResolveActorTier(CreatureTier::Light, false, "Ice Wolf", false) == CreatureTier::Excluded);

	// With creature poise off, NPC-keyword races revert to plain NPCs and real creatures are untracked.
	assert(ResolveActorTier(CreatureTier::Medium, true, "", false) == CreatureTier::Standard);
	assert(ResolveActorTier(CreatureTier::Standard, true, "Dragon Priest", false) == CreatureTier::Standard);
	assert(ResolveActorTier(CreatureTier::Boss, true, "", false) == CreatureTier::Standard);
	assert(ResolveActorTier(CreatureTier::Boss, false, "", false) == CreatureTier::Excluded);
	assert(ResolveActorTier(CreatureTier::Unknown, false, "", false) == CreatureTier::Excluded);
}
