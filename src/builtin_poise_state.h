#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace BuiltinPoise
{
	inline constexpr float kPostBreakRegenMultiplier = 1.65F;

	struct Settings
	{
		bool enabled = true;
		bool npcEnabled = true;
		float basePoise = 40.0F;
		float armorWeightMultiplier = 1.0F;
		float shieldWeightPoiseMultiplier = 0.0F;
		bool applyShieldWeightToNPCs = true;
		float baseHitDamage = 6.0F;
		float creatureBaseHitPoiseDamage = 5.5F;
		float weaponWeightDamageMultiplier = 0.35F;
		float twoHandedMultiplier = 1.15F;
		float shieldBashMultiplier = 1.0F;
		float weaponBashMultiplier = 0.35F;
		float projectileMultiplier = 0.65F;
		float powerAttackMultiplier = 1.5F;
		float shieldBlockedPhysicalMultiplier = 0.25F;
		float weaponBlockedPhysicalMultiplier = 0.45F;
		float regenDelaySeconds = 3.0F;
		float regenRatePerSecond = 8.0F;
		float staggerImmunitySeconds = 1.0F;
		float fleshSpellPoiseMultiplier = 0.45F;
		float dragonhidePoiseBonus = 60.0F;
		float magicBurstPoiseMultiplier = 1.2F;
		float magicStaggerPoiseMultiplier = 10.0F;
		float concentrationPoiseMultiplier = 1.0F;
		float concentrationIntervalSeconds = 1.0F;
		float concentrationMaxPoisePerInterval = 15.0F;
		float clothingMagicPoiseMultiplier = 0.70F;
		float lightArmorMagicPoiseMultiplier = 1.00F;
		float heavyArmorMagicPoiseMultiplier = 1.15F;
		// Channeled spells (Flames, Frostbite, Sparks) keep their debuffs but deal no direct poise damage by default.
		bool concentrationSpellsDealPoise = false;
		float wardMagicDamageMultiplier = 0.35F;
		float shieldBlockedMagicMultiplier = 0.4F;
		float weaponBlockedMagicMultiplier = 0.7F;
		bool suppressVanillaStaggers = true;
		bool enableHitPushBack = false;
		bool allowTimedBashStaggers = false;
		bool enableCreaturePoise = true;
		float lightCreaturePoiseMultiplier = 0.75F;
		float mediumCreaturePoiseMultiplier = 1.7F;
		float heavyCreaturePoiseMultiplier = 3.75F;
		float bossCreaturePoiseMultiplier = 6.0F;
		float bossCreatureRegenMultiplier = 1.5F;
		float lightCreatureDamageMultiplier = 0.75F;
		float mediumCreatureDamageMultiplier = 1.25F;
		float heavyCreatureDamageMultiplier = 1.65F;
		float bossCreatureDamageMultiplier = 2.40F;
		float creatureStaggerImmunitySeconds = 2.0F;
		float summonPoiseMultiplier = 1.5F;
		float npcMagicPoiseMultiplier = 0.6F;
		float physicalDamagePoiseMultiplier = 0.05F;
		float physicalDamagePoiseCap = 6.0F;
		float lightArmorRatingPoiseMultiplier = 0.15F;
		float lightArmorRatingPoiseCap = 15.0F;
		float lightArmorFleshMultiplier = 0.75F;
		float heavyArmorFleshMultiplier = 0.45F;
		bool enablePoiseDebuffs = true;
		bool enableSappedDebuff = true;
		bool enableEnfeebledDebuff = true;
		bool enableNumbedDebuff = true;
		bool enableHemorrhagingDebuff = true;
		bool enableScorchedDebuff = true;
		float enfeebledRegenCeilingMin = 0.70F;
		float enfeebledRegenCeilingMax = 0.85F;
		// Hostile poison magnitude per second that reaches the minimum ceiling.
		float enfeebledMagnitudeForMin = 10.0F;
		float numbedRegenDelayBonus = 1.0F;
		float hemorrhagingRegenRateMultiplier = 0.65F;
		float scorchedMeleeDamageMultiplier = 1.20F;
		float minGlobalRegenCeiling = 0.60F;
		// Comma-separated, case-insensitive; an effect whose editor ID or name contains any entry counts as a bleed.
		std::string hemorrhagingKeywords = "bleed,hemorrhag";
	};

	enum class DebuffId : std::size_t { sapped = 0, enfeebled, numbed, hemorrhaging, scorched };
	constexpr std::size_t kDebuffCount = 5;
	// How long an absorb hit keeps Sapped on when no lasting effect remains to be scanned.
	constexpr float kSappedPulseSeconds = 2.0F;

	[[nodiscard]] constexpr const char* DebuffName(DebuffId id) noexcept
	{
		switch (id) {
		case DebuffId::sapped: return "sapped";
		case DebuffId::enfeebled: return "enfeebled";
		case DebuffId::numbed: return "numbed";
		case DebuffId::hemorrhaging: return "hemorrhaging";
		case DebuffId::scorched: return "scorched";
		}
		return "";
	}

	[[nodiscard]] constexpr const char* DebuffLabel(DebuffId id) noexcept
	{
		switch (id) {
		case DebuffId::sapped: return "SAPPED";
		case DebuffId::enfeebled: return "ENFEEBLED";
		case DebuffId::numbed: return "NUMBED";
		case DebuffId::hemorrhaging: return "HEMORRHAGING";
		case DebuffId::scorched: return "SCORCHED";
		}
		return "";
	}

	// Case-insensitive match of any comma-separated keyword against the text; empty entries are ignored.
	[[nodiscard]] inline bool MatchesKeywordList(std::string_view list, std::string_view text)
	{
		const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
		std::string haystack(text);
		std::transform(haystack.begin(), haystack.end(), haystack.begin(), lower);
		std::size_t start = 0;
		while (start <= list.size()) {
			auto end = list.find(',', start);
			if (end == std::string_view::npos) {
				end = list.size();
			}
			auto entry = list.substr(start, end - start);
			while (!entry.empty() && std::isspace(static_cast<unsigned char>(entry.front()))) {
				entry.remove_prefix(1);
			}
			while (!entry.empty() && std::isspace(static_cast<unsigned char>(entry.back()))) {
				entry.remove_suffix(1);
			}
			if (!entry.empty()) {
				std::string needle(entry);
				std::transform(needle.begin(), needle.end(), needle.begin(), lower);
				if (haystack.find(needle) != std::string::npos) {
					return true;
				}
			}
			start = end + 1;
		}
		return false;
	}

	enum class EffectKind { other, absorb };
	enum class EffectResist { none, poison, fire, frost, other };

	// Vanilla Skyrim.esm bleed: the War Axe and Battleaxe perks (Hack and Slash, Limbsplitter) cast the PerkBleeding*
	// spells, which all use this one effect. It has no hostile flag (only detrimental), so it is matched by form.
	// Skyrim.esm records 0x00073F33-35 are potions, not effects.
	constexpr std::uint32_t kVanillaBleedEffectFormID = 0x000C367A;

	[[nodiscard]] inline bool IsVanillaBleedEffect(std::string_view plugin, std::uint32_t localFormID) noexcept
	{
		constexpr std::string_view skyrim = "Skyrim.esm";
		if (plugin.size() != skyrim.size()) {
			return false;
		}
		for (std::size_t i = 0; i < skyrim.size(); ++i) {
			const char a = plugin[i] >= 'A' && plugin[i] <= 'Z' ? static_cast<char>(plugin[i] - 'A' + 'a') : plugin[i];
			const char b = skyrim[i] >= 'A' && skyrim[i] <= 'Z' ? static_cast<char>(skyrim[i] - 'A' + 'a') : skyrim[i];
			if (a != b) {
				return false;
			}
		}
		return localFormID == kVanillaBleedEffectFormID;
	}

	// An effect is hostile when its record or parent spell says so. Bleed effects, which are only flagged detrimental,
	// are accepted on that flag too.
	[[nodiscard]] constexpr bool EffectIsHostile(bool effectHostile, bool spellHostile, bool detrimental,
		bool bleedCandidate) noexcept
	{
		return effectHostile || spellHostile || (bleedCandidate && detrimental);
	}

	// What the scanner reads from one active effect, free of game types so classification is testable.
	struct EffectFacts
	{
		bool hostile = false;
		bool selfCast = false;
		EffectKind kind = EffectKind::other;
		EffectResist resist = EffectResist::none;
		bool healthDamage = false;
		bool harmfulPoisonKeyword = false;
		bool bleedMatch = false;
		// The effect record is flagged detrimental (harmful to its holder), independent of the hostile flag.
		bool detrimental = false;
		// The effect record's defining plugin and load-order-independent form ID.
		std::string_view plugin;
		std::uint32_t localFormID = 0;
		float duration = 0.0F;
		float magnitude = 0.0F;
	};

	// Raw scan result for one actor. Resolution against the settings happens at use time, so F10 changes apply
	// without rescanning.
	struct PoiseDebuffs
	{
		std::array<bool, kDebuffCount> active{};
		std::array<std::string, kDebuffCount> source{};
		// Strongest hostile poison magnitude per second seen while enfeebled.
		float poisonMagnitude = 0.0F;

		// Seconds left on a brief Sapped applied by an absorb hit that leaves no lasting effect (weapon enchantments).
		float sappedPulse = 0.0F;

		[[nodiscard]] bool Has(DebuffId id) const noexcept
		{
			return active[static_cast<std::size_t>(id)] || (id == DebuffId::sapped && sappedPulse > 0.0F);
		}
		[[nodiscard]] bool Any() const noexcept
		{
			return sappedPulse > 0.0F || std::any_of(active.begin(), active.end(), [](bool on) { return on; });
		}
		void NoteSapped(float seconds, std::string_view label)
		{
			auto& name = source[static_cast<std::size_t>(DebuffId::sapped)];
			if (!active[static_cast<std::size_t>(DebuffId::sapped)] && sappedPulse <= 0.0F) {
				name = label;
			}
			sappedPulse = (std::max)(sappedPulse, seconds);
		}
	};

	// Folds one effect in. Same-type effects never add: flags are single booleans and the strongest poison wins.
	inline void AccumulateDebuff(PoiseDebuffs& debuffs, const EffectFacts& facts, std::string_view name)
	{
		if (!facts.hostile) {
			return;
		}
		const auto mark = [&](DebuffId id) {
			auto& flag = debuffs.active[static_cast<std::size_t>(id)];
			auto& source = debuffs.source[static_cast<std::size_t>(id)];
			if (!flag) {
				source = name;
			}
			flag = true;
		};
		const bool bleed = facts.bleedMatch || IsVanillaBleedEffect(facts.plugin, facts.localFormID);
		if (facts.selfCast) {
			// Combat-hit perks such as Hack and Slash apply their bleed with the victim as its own caster, so a detrimental
			// bleed counts even then. It is never a self-buff; every other self-cast effect stays ignored.
			if (bleed && facts.detrimental) {
				mark(DebuffId::hemorrhaging);
			}
			return;
		}
		if (facts.kind == EffectKind::absorb) {
			mark(DebuffId::sapped);
		}
		if (facts.resist == EffectResist::poison || facts.harmfulPoisonKeyword) {
			const float magnitude = std::isfinite(facts.magnitude) ? (std::max)(0.0F, facts.magnitude) : 0.0F;
			if (!debuffs.Has(DebuffId::enfeebled) || magnitude > debuffs.poisonMagnitude) {
				debuffs.source[static_cast<std::size_t>(DebuffId::enfeebled)] = name;
				debuffs.poisonMagnitude = magnitude;
			}
			debuffs.active[static_cast<std::size_t>(DebuffId::enfeebled)] = true;
		}
		if (facts.resist == EffectResist::frost && facts.healthDamage && facts.duration > 0.0F) {
			mark(DebuffId::numbed);
		}
		if (facts.resist == EffectResist::fire && facts.healthDamage && facts.duration > 0.0F) {
			mark(DebuffId::scorched);
		}
		if (bleed) {
			mark(DebuffId::hemorrhaging);
		}
	}

	// What the active debuffs do to one actor's poise right now; neutral when the system or a debuff is off.
	struct DebuffEffects
	{
		float ceilingRatio = 1.0F;
		float regenRateMult = 1.0F;
		bool sapped = false;
		float delayBonus = 0.0F;
		float scorchedMeleeMult = 1.0F;
		std::array<bool, kDebuffCount> active{};
	};

	[[nodiscard]] inline DebuffEffects ResolveDebuffEffects(const Settings& settings, const PoiseDebuffs& debuffs)
	{
		DebuffEffects fx;
		if (!settings.enablePoiseDebuffs) {
			return fx;
		}
		const auto on = [&](DebuffId id, bool enabled) { return enabled && debuffs.Has(id); };
		fx.active[static_cast<std::size_t>(DebuffId::sapped)] = on(DebuffId::sapped, settings.enableSappedDebuff);
		fx.active[static_cast<std::size_t>(DebuffId::enfeebled)] = on(DebuffId::enfeebled, settings.enableEnfeebledDebuff);
		fx.active[static_cast<std::size_t>(DebuffId::numbed)] = on(DebuffId::numbed, settings.enableNumbedDebuff);
		fx.active[static_cast<std::size_t>(DebuffId::hemorrhaging)] = on(DebuffId::hemorrhaging, settings.enableHemorrhagingDebuff);
		fx.active[static_cast<std::size_t>(DebuffId::scorched)] = on(DebuffId::scorched, settings.enableScorchedDebuff);
		fx.sapped = fx.active[static_cast<std::size_t>(DebuffId::sapped)];
		if (fx.active[static_cast<std::size_t>(DebuffId::enfeebled)]) {
			const float high = std::clamp((std::max)(settings.enfeebledRegenCeilingMin, settings.enfeebledRegenCeilingMax), 0.0F, 1.0F);
			const float low = std::clamp((std::min)(settings.enfeebledRegenCeilingMin, settings.enfeebledRegenCeilingMax), 0.0F, 1.0F);
			const float scale = settings.enfeebledMagnitudeForMin > 0.0F ?
				std::clamp(debuffs.poisonMagnitude / settings.enfeebledMagnitudeForMin, 0.0F, 1.0F) : 1.0F;
			const float poisonRatio = high - (high - low) * scale;
			fx.ceilingRatio = (std::max)(std::clamp(settings.minGlobalRegenCeiling, 0.0F, 1.0F), poisonRatio);
		}
		if (fx.active[static_cast<std::size_t>(DebuffId::hemorrhaging)]) {
			fx.regenRateMult = settings.hemorrhagingRegenRateMultiplier;
		}
		if (fx.active[static_cast<std::size_t>(DebuffId::numbed)]) {
			fx.delayBonus = (std::max)(0.0F, settings.numbedRegenDelayBonus);
		}
		if (fx.active[static_cast<std::size_t>(DebuffId::scorched)]) {
			fx.scorchedMeleeMult = settings.scorchedMeleeDamageMultiplier;
		}
		return fx;
	}

	// Shields never set an armor class flag or add light armor rating.
	struct ArmorProfile
	{
		float weight = 0.0F;
		float shieldWeight = 0.0F;
		float lightRating = 0.0F;
		bool hasHeavy = false;
		bool hasLight = false;
	};

	struct ArmorMagicPoiseScale
	{
		const char* label = "Clothing Armor";
		float multiplier = 1.0F;
	};

	[[nodiscard]] inline ArmorMagicPoiseScale MagicArmorPoiseScale(
		const Settings& settings, const ArmorProfile& armor) noexcept
	{
		if (armor.hasHeavy) {
			return { "Heavy Armor", settings.heavyArmorMagicPoiseMultiplier };
		}
		if (armor.hasLight) {
			return { "Light Armor", settings.lightArmorMagicPoiseMultiplier };
		}
		return { "Clothing Armor", settings.clothingMagicPoiseMultiplier };
	}

	struct FleshRating
	{
		float armor = 0.0F;
		bool dragonhide = false;
	};

	struct MaximumBreakdown
	{
		float maximum = 0.0F;
		float lightArmorBonus = 0.0F;
		float fleshBonus = 0.0F;
		float fleshEfficiency = 1.0F;
	};

	struct PhysicalBonus
	{
		float bonus = 0.0F;
		bool capped = false;
	};

	// Poise damage added from the hit's own physical damage (sneak and perk multipliers included as reported).
	[[nodiscard]] inline PhysicalBonus PhysicalDamageBonus(const Settings& settings, float physicalDamage) noexcept
	{
		const float damage = std::isfinite(physicalDamage) ? (std::max)(0.0F, physicalDamage) : 0.0F;
		const float cap = (std::max)(0.0F, settings.physicalDamagePoiseCap);
		const float scaled = damage * settings.physicalDamagePoiseMultiplier;
		return { (std::min)(cap, scaled), scaled > 0.0F && scaled >= cap };
	}

	[[nodiscard]] inline float LightArmorTierBonus(const Settings& settings, float lightRating) noexcept
	{
		const float rating = std::isfinite(lightRating) ? std::clamp(lightRating, 0.0F, 2000.0F) : 0.0F;
		return (std::min)((std::max)(0.0F, settings.lightArmorRatingPoiseCap),
			rating * settings.lightArmorRatingPoiseMultiplier);
	}

	// Heavy takes priority over Light; unarmored and cloth wearers keep full flesh efficiency.
	[[nodiscard]] inline float FleshEfficiency(const Settings& settings, const ArmorProfile& armor) noexcept
	{
		if (armor.hasHeavy) {
			return settings.heavyArmorFleshMultiplier;
		}
		return armor.hasLight ? settings.lightArmorFleshMultiplier : 1.0F;
	}

	// Standard is a regular NPC; Excluded actors (ambient critters) are never tracked.
	// Unknown is a creature outside the named buckets and uses the unmodified base poise.
	enum class CreatureTier { Standard, Excluded, Unknown, Light, Medium, Heavy, Boss };

	constexpr float kBossMinimumBreakMagnitude = 1.0F;
	constexpr float kDefaultMinimumBreakMagnitude = 0.25F;

	[[nodiscard]] constexpr bool IsCreatureTier(CreatureTier tier) noexcept
	{
		return tier != CreatureTier::Standard && tier != CreatureTier::Excluded;
	}

	[[nodiscard]] constexpr float TierPoiseMultiplier(const Settings& settings, CreatureTier tier) noexcept
	{
		switch (tier) {
		case CreatureTier::Light: return settings.lightCreaturePoiseMultiplier;
		case CreatureTier::Medium: return settings.mediumCreaturePoiseMultiplier;
		case CreatureTier::Heavy: return settings.heavyCreaturePoiseMultiplier;
		case CreatureTier::Boss: return settings.bossCreaturePoiseMultiplier;
		default: return 1.0F;
		}
	}

	[[nodiscard]] constexpr float TierRegenMultiplier(const Settings& settings, CreatureTier tier) noexcept
	{
		return tier == CreatureTier::Boss ? settings.bossCreatureRegenMultiplier : 1.0F;
	}

	[[nodiscard]] constexpr float TierMinimumBreakMagnitude(CreatureTier tier) noexcept
	{
		return tier == CreatureTier::Boss ? kBossMinimumBreakMagnitude : kDefaultMinimumBreakMagnitude;
	}

	// Only the base pool is scaled by tier; armor and spell bonuses stay additive.
	[[nodiscard]] constexpr float TierBasePoise(const Settings& settings, CreatureTier tier) noexcept
	{
		return settings.basePoise * TierPoiseMultiplier(settings, tier);
	}

	[[nodiscard]] inline MaximumBreakdown ComputeMaximumPoise(const Settings& settings, CreatureTier tier,
		const ArmorProfile& armor, const FleshRating& flesh, float summonScale, bool isPlayer) noexcept
	{
		const float weight = std::isfinite(armor.weight) ? std::clamp(armor.weight, 0.0F, 10000.0F) : 0.0F;
		const float shieldWeight = std::isfinite(armor.shieldWeight) ? std::clamp(armor.shieldWeight, 0.0F, 10000.0F) : 0.0F;
		const float shieldMultiplier = !isPlayer && settings.applyShieldWeightToNPCs ?
			settings.armorWeightMultiplier : settings.shieldWeightPoiseMultiplier;
		MaximumBreakdown result;
		result.fleshEfficiency = FleshEfficiency(settings, armor);
		const float lightBonus = LightArmorTierBonus(settings, armor.lightRating);
		const float fleshPoise = (flesh.armor * settings.fleshSpellPoiseMultiplier +
			(flesh.dragonhide ? settings.dragonhidePoiseBonus : 0.0F)) * result.fleshEfficiency;
		result.maximum = (std::max)(10.0F, (TierBasePoise(settings, tier) +
			weight * settings.armorWeightMultiplier + shieldWeight * shieldMultiplier + lightBonus +
			fleshPoise) * summonScale);
		result.lightArmorBonus = lightBonus * summonScale;
		result.fleshBonus = fleshPoise * summonScale;
		return result;
	}

	[[nodiscard]] constexpr bool TierEnabled(const Settings& settings, CreatureTier tier) noexcept
	{
		if (tier == CreatureTier::Excluded) {
			return false;
		}
		return tier == CreatureTier::Standard ? settings.npcEnabled : settings.enableCreaturePoise;
	}

	// Scales an attacking creature's physical poise damage; non-creature attackers are unscaled.
	[[nodiscard]] constexpr float CreatureDamageTierMultiplier(const Settings& settings, CreatureTier tier) noexcept
	{
		switch (tier) {
		case CreatureTier::Light: return settings.lightCreatureDamageMultiplier;
		case CreatureTier::Medium: return settings.mediumCreatureDamageMultiplier;
		case CreatureTier::Heavy: return settings.heavyCreatureDamageMultiplier;
		case CreatureTier::Boss: return settings.bossCreatureDamageMultiplier;
		default: return 1.0F;
		}
	}

	enum class HitType { melee, bash, creature, projectile };

	struct Hit
	{
		HitType type = HitType::melee;
		float weaponWeight = 0.0F;
		bool twoHanded = false;
		bool powerAttack = false;
		bool blocked = false;
		bool shieldBlocked = false;
		bool shieldBash = false;
		CreatureTier attackerTier = CreatureTier::Standard;
		float physicalDamage = 0.0F;
	};

	struct HitResult
	{
		bool handled = false;
		bool broke = false;
		float magnitude = 0.0F;
		float rawBonus = 0.0F;
		bool rawCapped = false;
		// True when stagger immunity swallowed the hit, so no poise was removed.
		bool absorbed = false;
	};

	constexpr std::size_t kMaxHitTags = 6;

	struct HitTag
	{
		const char* label = "";
		float factor = 1.0F;
	};

	// Filled by State::Apply only when the caller asks for it (live telemetry); labels are string literals.
	struct HitBreakdown
	{
		const char* typeLabel = "";
		float baseDamage = 0.0F;
		float rawBonus = 0.0F;
		bool rawCapped = false;
		std::array<HitTag, kMaxHitTags> tags{};
		std::size_t tagCount = 0;
		float finalDamage = 0.0F;

		void Add(const char* label, float factor) noexcept
		{
			if (tagCount < tags.size()) {
				tags[tagCount++] = { label, factor };
			}
		}
	};

	[[nodiscard]] constexpr const char* AttackerTierLabel(CreatureTier tier) noexcept
	{
		switch (tier) {
		case CreatureTier::Light: return "Light";
		case CreatureTier::Medium: return "Medium";
		case CreatureTier::Heavy: return "Heavy";
		case CreatureTier::Boss: return "Boss";
		default: return "";
		}
	}

	struct HitRecord
	{
		bool outgoing = false;
		std::string attacker;
		std::string target;
		std::string type;
		float baseDamage = 0.0F;
		float rawBonus = 0.0F;
		bool rawCapped = false;
		std::vector<std::pair<std::string, float>> tags;
		float finalDamage = 0.0F;
		bool broke = false;
		bool immune = false;
	};

	// The last few poise hits involving the player, newest first when read.
	class HitLog
	{
	public:
		static constexpr std::size_t kCapacity = 5;

		void Push(HitRecord record)
		{
			entries[next] = std::move(record);
			next = (next + 1) % kCapacity;
			count = (std::min)(count + 1, kCapacity);
		}

		void Clear()
		{
			entries = {};
			next = 0;
			count = 0;
		}

		[[nodiscard]] std::size_t Size() const noexcept { return count; }

		[[nodiscard]] const HitRecord& Newest(std::size_t index) const
		{
			return entries[(next + kCapacity - 1 - index) % kCapacity];
		}

	private:
		std::array<HitRecord, kCapacity> entries{};
		std::size_t next = 0;
		std::size_t count = 0;
	};

	// Poise damage the equipped right-hand weapon deals to an unblocked target, including the capped estimate of the
	// physical-damage bonus taken from the weapon's base damage (the real bonus uses the actual hit damage).
	struct WeaponBenchmark
	{
		bool armed = false;
		bool twoHanded = false;
		bool ranged = false;
		float lightHit = 0.0F;
		float powerHit = 0.0F;
		float estimatedRawBonus = 0.0F;
		bool rawCapped = false;
	};

	[[nodiscard]] inline WeaponBenchmark ComputeWeaponBenchmark(const Settings& settings, bool armed, float weaponWeight,
		float baseDamage, bool twoHanded, bool ranged) noexcept
	{
		WeaponBenchmark result;
		result.armed = armed;
		result.twoHanded = twoHanded;
		result.ranged = ranged;
		const float weight = std::isfinite(weaponWeight) ? std::clamp(weaponWeight, 0.0F, 1000.0F) : 0.0F;
		const auto raw = PhysicalDamageBonus(settings, armed ? baseDamage : 0.0F);
		result.estimatedRawBonus = raw.bonus;
		result.rawCapped = raw.capped;
		float damage = settings.baseHitDamage + weight * settings.weaponWeightDamageMultiplier + raw.bonus;
		if (twoHanded) {
			damage *= settings.twoHandedMultiplier;
		} else if (ranged) {
			damage *= settings.projectileMultiplier;
		}
		result.lightHit = damage;
		result.powerHit = result.lightHit * settings.powerAttackMultiplier;
		return result;
	}

	class State
	{
	public:
		void Reset()
		{
			current = 0.0F;
			maximum = 0.0F;
			regenDelay = 0.0F;
			immunity = 0.0F;
			recoveringFromBreak = false;
			lightArmorBonusPoise = 0.0F;
			fleshBonusPoise = 0.0F;
			debuffs = {};
		}

		void SetDebuffs(PoiseDebuffs value)
		{
			// A rescan only sees lasting effects, so a running absorb pulse is carried over it.
			const float pulse = debuffs.sappedPulse;
			const auto pulseSource = debuffs.source[static_cast<std::size_t>(DebuffId::sapped)];
			debuffs = std::move(value);
			if (pulse > 0.0F) {
				debuffs.sappedPulse = pulse;
				if (!debuffs.active[static_cast<std::size_t>(DebuffId::sapped)]) {
					debuffs.source[static_cast<std::size_t>(DebuffId::sapped)] = pulseSource;
				}
			}
		}

		// An absorb hit landed: Sapped holds for a short window even if the effect itself is already gone.
		void NoteSapped(std::string_view label) { debuffs.NoteSapped(kSappedPulseSeconds, label); }

		void SetPlayerTarget(bool value) noexcept
		{
			isPlayer = value;
		}

		void SetMaximum(float newMaximum)
		{
			if (!std::isfinite(newMaximum) || newMaximum <= 0.0F) {
				Reset();
				return;
			}
			current = maximum > 0.0F ? std::clamp(current / maximum, 0.0F, 1.0F) * newMaximum : newMaximum;
			maximum = newMaximum;
		}

		void SetMaximum(const MaximumBreakdown& breakdown)
		{
			SetMaximum(breakdown.maximum);
			if (maximum > 0.0F) {
				lightArmorBonusPoise = breakdown.lightArmorBonus;
				fleshBonusPoise = breakdown.fleshBonus;
			}
		}

		HitResult Apply(const Settings& settings, const Hit& hit, HitBreakdown* breakdown = nullptr)
		{
			if (maximum <= 0.0F || !settings.enabled) {
				return {};
			}
			const float weight = std::isfinite(hit.weaponWeight) ? std::clamp(hit.weaponWeight, 0.0F, 1000.0F) : 0.0F;
			// A shield-blocked hit never carries the attacker's raw weapon damage; weapon blocks and open hits do.
			const bool shieldBlocked = hit.blocked && hit.shieldBlocked;
			const auto raw = shieldBlocked ? PhysicalBonus{} : PhysicalDamageBonus(settings, hit.physicalDamage);
			lastRawBonus = raw.bonus;
			lastRawCapped = raw.capped;
			const bool creatureHit = hit.type == HitType::creature;
			float damage = creatureHit ? settings.creatureBaseHitPoiseDamage + raw.bonus :
				settings.baseHitDamage + weight * settings.weaponWeightDamageMultiplier + raw.bonus;
			if (breakdown) {
				*breakdown = {};
				breakdown->baseDamage = creatureHit ? settings.creatureBaseHitPoiseDamage :
					settings.baseHitDamage + weight * settings.weaponWeightDamageMultiplier;
				breakdown->rawBonus = raw.bonus;
				breakdown->rawCapped = raw.capped;
			}
			switch (hit.type) {
			case HitType::melee:
				if (breakdown) {
					breakdown->typeLabel = hit.twoHanded ? "2H Melee" : "1H Melee";
				}
				if (hit.twoHanded) {
					damage *= settings.twoHandedMultiplier;
					if (breakdown) {
						breakdown->Add("2H", settings.twoHandedMultiplier);
					}
				}
				break;
			case HitType::bash: {
				const float multiplier = hit.shieldBash ? settings.shieldBashMultiplier : settings.weaponBashMultiplier;
				damage *= multiplier;
				if (breakdown) {
					breakdown->typeLabel = hit.shieldBash ? "Shield Bash" : "Weapon Bash";
					breakdown->Add(hit.shieldBash ? "Shield Bash" : "Weapon Bash", multiplier);
				}
				break;
			}
			case HitType::creature: {
				const float tierMultiplier = CreatureDamageTierMultiplier(settings, hit.attackerTier);
				damage *= tierMultiplier;
				if (breakdown) {
					breakdown->typeLabel = "Creature";
					if (tierMultiplier != 1.0F) {
						breakdown->Add(AttackerTierLabel(hit.attackerTier), tierMultiplier);
					}
				}
				break;
			}
			case HitType::projectile:
				damage *= settings.projectileMultiplier;
				if (breakdown) {
					breakdown->typeLabel = "Projectile";
					breakdown->Add("Projectile", settings.projectileMultiplier);
				}
				break;
			}
			if (hit.powerAttack) {
				damage *= settings.powerAttackMultiplier;
				if (breakdown) {
					breakdown->Add("Power", settings.powerAttackMultiplier);
				}
			}
			// Burning only makes physical hits land harder; projectiles are unaffected.
			if (hit.type != HitType::projectile) {
				const float scorched = ResolveDebuffEffects(settings, debuffs).scorchedMeleeMult;
				if (scorched != 1.0F) {
					damage *= scorched;
					if (breakdown) {
						breakdown->Add("Scorched", scorched);
					}
				}
			}
			if (hit.blocked) {
				const float blocked = hit.shieldBlocked ? settings.shieldBlockedPhysicalMultiplier :
					settings.weaponBlockedPhysicalMultiplier;
				damage *= blocked;
				if (breakdown) {
					breakdown->Add(hit.shieldBlocked ? "Blocked (Shield)" : "Blocked (Weapon)", blocked);
				}
			}
			if (breakdown) {
				breakdown->finalDamage = damage;
			}
			return ApplyDamage(settings, damage, raw);
		}

		HitResult ApplyDamage(const Settings& settings, float damage, PhysicalBonus raw = {})
		{
			if (maximum <= 0.0F || !settings.enabled) {
				return {};
			}
			if (immunity > 0.0F) {
				return { true, false, 0.0F, raw.bonus, raw.capped, true };
			}
			if (!std::isfinite(damage) || damage <= 0.0F) {
				return { true, false, 0.0F, raw.bonus, raw.capped, false };
			}
			const auto fx = ResolveDebuffEffects(settings, debuffs);
			regenDelay = settings.regenDelaySeconds + fx.delayBonus;
			current = (std::max)(0.0F, current - damage);
			if (current > 0.0F) {
				return { true, false, 0.0F, raw.bonus, raw.capped, false };
			}
			if (isPlayer) {
				current = 0.0F;
				recoveringFromBreak = true;
				regenDelay = fx.delayBonus;
			} else {
				current = maximum;
				recoveringFromBreak = false;
				regenDelay = settings.regenDelaySeconds + fx.delayBonus;
			}
			immunity = creatureImmunity ? settings.creatureStaggerImmunitySeconds : settings.staggerImmunitySeconds;
			return { true, true, std::clamp(damage / maximum, minimumBreakMagnitude, 1.0F), raw.bonus, raw.capped, false };
		}

		void ApplyTier(const Settings& settings, CreatureTier tier)
		{
			regenMultiplier = TierRegenMultiplier(settings, tier);
			minimumBreakMagnitude = TierMinimumBreakMagnitude(tier);
			creatureImmunity = IsCreatureTier(tier);
		}

		void Advance(const Settings& settings, float delta)
		{
			if (!settings.enabled || maximum <= 0.0F || !std::isfinite(delta) || delta <= 0.0F) {
				return;
			}
			const float oldImmunity = immunity;
			immunity = (std::max)(0.0F, oldImmunity - delta);
			const float recoveryDelta = (std::max)(0.0F, delta - oldImmunity);
			const float oldDelay = regenDelay;
			regenDelay = (std::max)(0.0F, regenDelay - recoveryDelta);
			float regenTime = (std::max)(0.0F, recoveryDelta - oldDelay);
			const auto fx = ResolveDebuffEffects(settings, debuffs);
			const float rate = fx.sapped ? 0.0F : settings.regenRatePerSecond * regenMultiplier * fx.regenRateMult;
			// Recovery only climbs toward the ceiling; poise already above it is never reduced.
			const float ceiling = maximum * fx.ceilingRatio;
			if (current < ceiling) {
				const float surgeLimit = maximum * 0.5F;
				float recovered = 0.0F;
				if (recoveringFromBreak && current < surgeLimit && rate > 0.0F) {
					const float surgeRate = rate * kPostBreakRegenMultiplier;
					const float surgeCeiling = (std::min)(ceiling, surgeLimit);
					const float timeToSurgeLimit = (std::max)(0.0F, surgeCeiling - current) / surgeRate;
					const float surgeTime = (std::min)(regenTime, timeToSurgeLimit);
					recovered = surgeRate * surgeTime;
					regenTime -= surgeTime;
					if (current + recovered >= surgeLimit) {
						recoveringFromBreak = false;
					}
				}
				current = (std::min)(ceiling, current + recovered + rate * regenTime);
			}
			debuffs.sappedPulse = (std::max)(0.0F, debuffs.sappedPulse - delta);
		}

		float current = 0.0F;
		float maximum = 0.0F;
		float regenDelay = 0.0F;
		float immunity = 0.0F;
		float regenMultiplier = 1.0F;
		float minimumBreakMagnitude = kDefaultMinimumBreakMagnitude;
		bool creatureImmunity = false;
		bool isPlayer = false;
		bool recoveringFromBreak = false;
		PoiseDebuffs debuffs;
		// Telemetry: contribution of each term to the current maximum and the latest hit's physical-damage bonus.
		float lightArmorBonusPoise = 0.0F;
		float fleshBonusPoise = 0.0F;
		float lastRawBonus = 0.0F;
		bool lastRawCapped = false;
	};
}
