#include "../src/builtin_poise_telemetry.h"
#include "../src/magic_poise_math.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <string>

namespace
{
	bool Near(float a, float b) { return std::abs(a - b) < 0.001F; }
	bool Has(const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; }
}

int main()
{
	using namespace BuiltinPoise;
	Settings settings;

	// Reactive bar ids are emitted in a fixed order and only for active effects.
	{
		ReactiveBarView bars;
		assert(ReactiveBarIds(bars).empty());
		bars.staminaFrost = true;
		bars.barShock = true;
		bars.healthPoison = true;
		bars.healthSapped = true;
		const auto ids = ReactiveBarIds(bars);
		assert((ids == std::vector<std::string>{ "health-poison", "health-sapped", "bar-shock", "stamina-frost" }));
		bars = {};
		bars.healthFire = true;
		bars.healthBleed = true;
		assert((ReactiveBarIds(bars) == std::vector<std::string>{ "health-fire", "health-bleed" }));
	}

	// Breakdown mirrors the damage chain in order and carries the raw bonus and the final damage.
	State victim;
	victim.SetMaximum(1000.0F);
	Hit hit;
	hit.weaponWeight = 20.0F;
	hit.twoHanded = true;
	hit.powerAttack = true;
	hit.physicalDamage = 200.0F;
	HitBreakdown breakdown;
	auto result = victim.Apply(settings, hit, &breakdown);
	assert(result.handled && !result.absorbed);
	assert(std::string(breakdown.typeLabel) == "2H Melee");
	assert(Near(breakdown.baseDamage, 13.0F) && Near(breakdown.rawBonus, 6.0F) && breakdown.rawCapped);
	assert(breakdown.tagCount == 2);
	assert(std::string(breakdown.tags[0].label) == "2H" && Near(breakdown.tags[0].factor, 1.15F));
	assert(std::string(breakdown.tags[1].label) == "Power" && Near(breakdown.tags[1].factor, 1.5F));
	assert(Near(breakdown.finalDamage, (13.0F + 6.0F) * 1.15F * 1.5F));
	assert(Near(1000.0F - victim.current, breakdown.finalDamage));

	// Passing a breakdown never changes the outcome.
	State plain;
	plain.SetMaximum(1000.0F);
	plain.Apply(settings, hit);
	assert(Near(plain.current, victim.current));

	auto describe = [&](Hit input) {
		State sample;
		sample.SetMaximum(1000.0F);
		HitBreakdown out;
		sample.Apply(settings, input, &out);
		return out;
	};
	Hit bash;
	bash.type = HitType::bash;
	bash.shieldBash = true;
	assert(std::string(describe(bash).typeLabel) == "Shield Bash");
	bash.shieldBash = false;
	assert(std::string(describe(bash).typeLabel) == "Weapon Bash");
	Hit slam;
	slam.type = HitType::creature;
	slam.attackerTier = CreatureTier::Boss;
	slam.blocked = true;
	slam.shieldBlocked = true;
	const auto slamInfo = describe(slam);
	assert(std::string(slamInfo.typeLabel) == "Creature" && slamInfo.tagCount == 2);
	assert(std::string(slamInfo.tags[0].label) == "Boss" && Near(slamInfo.tags[0].factor, 2.40F));
	assert(std::string(slamInfo.tags[1].label) == "Blocked (Shield)" && Near(slamInfo.tags[1].factor, 0.25F));
	assert(Near(slamInfo.finalDamage, 5.5F * 2.40F * 0.25F));
	slam.shieldBlocked = false;
	const auto weaponBlockInfo = describe(slam);
	assert(weaponBlockInfo.tagCount == 2 && std::string(weaponBlockInfo.tags[1].label) == "Blocked (Weapon)");
	assert(Near(weaponBlockInfo.finalDamage, 5.5F * 2.40F * 0.45F));
	slam.blocked = false;
	const auto openInfo = describe(slam);
	assert(openInfo.tagCount == 1);
	assert(Near(openInfo.finalDamage, 5.5F * 2.40F));
	Hit arrow;
	arrow.type = HitType::projectile;
	assert(std::string(describe(arrow).typeLabel) == "Projectile");
	// A light creature has a non-neutral tier multiplier too, but an unknown creature adds no tier tag.
	Hit beast;
	beast.type = HitType::creature;
	beast.attackerTier = CreatureTier::Unknown;
	assert(describe(beast).tagCount == 0);
	HitBreakdown overflow;
	for (int i = 0; i < 20; ++i) {
		overflow.Add("x", 1.0F);
	}
	assert(overflow.tagCount == kMaxHitTags);

	// Stagger immunity is reported as absorbed with the damage that would have landed.
	State immune;
	immune.SetPlayerTarget(true);
	immune.SetMaximum(10.0F);
	immune.current = 1.0F;
	Hit finisher;
	finisher.physicalDamage = 80.0F;
	assert(immune.Apply(settings, finisher).broke);
	HitBreakdown absorbedInfo;
	auto absorbed = immune.Apply(settings, finisher, &absorbedInfo);
	assert(absorbed.handled && absorbed.absorbed && !absorbed.broke && absorbedInfo.finalDamage > 0.0F);

	// The log keeps the newest five, newest first, and clears.
	HitLog log;
	assert(log.Size() == 0);
	for (int i = 1; i <= 7; ++i) {
		HitRecord record;
		record.finalDamage = static_cast<float>(i);
		log.Push(record);
	}
	assert(log.Size() == HitLog::kCapacity);
	for (std::size_t i = 0; i < log.Size(); ++i) {
		assert(Near(log.Newest(i).finalDamage, static_cast<float>(7 - i)));
	}
	log.Clear();
	assert(log.Size() == 0);

	// Weapon benchmark: base + weight + capped estimated physical bonus, scaled by handedness, then unblocked and power.
	const auto sword = ComputeWeaponBenchmark(settings, true, 10.0F, 40.0F, false, false);
	assert(sword.armed && Near(sword.estimatedRawBonus, 2.0F) && !sword.rawCapped);
	assert(Near(sword.lightHit, 6.0F + 3.5F + 2.0F) && Near(sword.powerHit, sword.lightHit * 1.5F));
	const auto greatsword = ComputeWeaponBenchmark(settings, true, 20.0F, 500.0F, true, false);
	assert(greatsword.rawCapped && Near(greatsword.estimatedRawBonus, 6.0F));
	assert(Near(greatsword.lightHit, (6.0F + 7.0F + 6.0F) * 1.15F));
	const auto bow = ComputeWeaponBenchmark(settings, true, 5.0F, 20.0F, false, true);
	assert(bow.ranged && Near(bow.lightHit, (6.0F + 1.75F + 1.0F) * 0.65F));
	const auto fists = ComputeWeaponBenchmark(settings, false, 0.0F, 99.0F, false, false);
	assert(!fists.armed && Near(fists.estimatedRawBonus, 0.0F) && Near(fists.lightHit, 6.0F));

	// Magic explanation multiplies back to the damage the poise math produced.
	MagicPoise::DamageInput magic;
	magic.magnitude = 20.0F;
	magic.magnitudeMultiplier = 2.0F;
	magic.elementalResistance = 50.0F;
	magic.magicResistance = 10.0F;
	magic.wardIntercepted = true;
	magic.wardMultiplier = 0.35F;
	const auto explained = MagicPoise::Explain(magic);
	assert(Near(explained.raw, 40.0F) && Near(explained.elemental, 0.5F) && Near(explained.magic, 0.9F) &&
		Near(explained.ward, 0.35F));
	assert(Near(explained.raw * explained.elemental * explained.magic * explained.ward, MagicPoise::Damage(magic)));

	// JSON escaping: quotes, control characters, UTF-8, Latin-1 fallbacks and astral characters.
	auto escaped = [](std::string_view text) {
		std::string out;
		AppendJsonString(out, text);
		return out;
	};
	assert(escaped("Jarl \"Balgruuf\"\\") == "\"Jarl \\\"Balgruuf\\\"\\\\\"");
	assert(escaped("a\nb\t\x01") == "\"a\\nb\\t\\u0001\"");
	assert(escaped("D\xC3\xBCrnherz") == "\"D\\u00fcrnherz\"");
	assert(escaped("D\xFCrnherz") == "\"D\\u00fcrnherz\"");
	assert(escaped("\xF0\x9F\x90\x89") == "\"\\ud83d\\udc09\"");
	assert(escaped("\xC3") == "\"\\u00c3\"");
	assert(escaped("</script>") == "\"</script>\"");

	// The payload carries every section, finite numbers only, and an empty debuff list with a full ceiling.
	TelemetrySnapshot snapshot;
	snapshot.player.name = "Player";
	snapshot.player.tier = "Player";
	snapshot.player.current = 55.0F;
	snapshot.player.maximum = 73.0F;
	snapshot.player.ceilingPoise = 73.0F;
	snapshot.player.regenRate = 8.0F;
	snapshot.player.regenDelayMax = 3.0F;
	snapshot.player.fleshBonus = 18.0F;
	snapshot.player.immunity = std::numeric_limits<float>::infinity();
	snapshot.benchmark = sword;
	HitRecord hitRecord;
	hitRecord.outgoing = true;
	hitRecord.attacker = "Player";
	hitRecord.target = "Frost \"Troll\"";
	hitRecord.type = "2H Melee";
	hitRecord.baseDamage = 13.6F;
	hitRecord.rawBonus = 2.3F;
	hitRecord.rawCapped = true;
	hitRecord.tags = { { "2H", 1.15F }, { "Blocked (Weapon)", 0.45F } };
	hitRecord.finalDamage = 38.0F;
	hitRecord.broke = true;
	snapshot.hits.push_back(hitRecord);
	std::string json = TelemetryJson(snapshot);
	assert(Has(json, "\"player\":{\"name\":\"Player\""));
	assert(Has(json, "\"ceilingRatio\":1.000") && Has(json, "\"debuffs\":[]"));
	assert(Has(json, "\"flesh\":18.000") && Has(json, "\"immune\":0.000") && Has(json, "\"stale\":false"));
	assert(Has(json, "\"target\":null"));
	assert(Has(json, "\"weapon\":{\"armed\":true,\"twoHanded\":false,\"ranged\":false"));
	assert(Has(json, "\"dir\":\"OUT\"") && Has(json, "\"target\":\"Frost \\\"Troll\\\"\""));
	assert(Has(json, "\"mults\":[[\"2H\",1.150],[\"Blocked (Weapon)\",0.450]]"));
	assert(Has(json, "\"rawCapped\":true") && Has(json, "\"final\":38.000,\"broke\":true,\"immune\":false"));
	assert(!Has(json, "inf") && !Has(json, "nan"));
	assert(json.front() == '{' && json.back() == '}');

	snapshot.hasTarget = true;
	snapshot.target.name = "Draugr";
	snapshot.target.tier = "Medium";
	ActorTelemetry::Debuff debuff{ "sapped", "SAPPED", "0.0/s regen", "Frostbite" };
	snapshot.target.debuffs.push_back(debuff);
	snapshot.target.stale = true;
	json = TelemetryJson(snapshot);
	assert(Has(json, "\"target\":{\"name\":\"Draugr\",\"tier\":\"Medium\""));
	assert(Has(json, "\"stale\":true,\"debuffs\":[{\"id\":\"sapped\""));
	assert(Has(json, "\"debuffs\":[{\"id\":\"sapped\",\"label\":\"SAPPED\",\"detail\":\"0.0/s regen\",\"source\":\"Frostbite\"}]"));
}
