#pragma once

#include "builtin_poise_state.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace BuiltinPoise
{
	// One poise-managed actor as shown on the dev overlay (the player or the last-hit target).
	struct ActorTelemetry
	{
		std::string name;
		std::string tier;
		float current = 0.0F;
		float maximum = 0.0F;
		float ceilingRatio = 1.0F;
		float ceilingPoise = 0.0F;
		float regenRate = 0.0F;
		float regenDelay = 0.0F;
		float regenDelayMax = 0.0F;
		float immunity = 0.0F;
		float fleshBonus = 0.0F;
		float lightArmorBonus = 0.0F;
		// True when the values are the last ones seen before the actor died, unloaded or stopped being tracked.
		bool stale = false;
		// Filled by the poise-debuff pass; the overlay payload already carries the list.
		struct Debuff
		{
			std::string id;
			std::string label;
			std::string detail;
			std::string source;
		};
		std::vector<Debuff> debuffs;
	};

	struct TelemetrySnapshot
	{
		ActorTelemetry player;
		bool hasTarget = false;
		ActorTelemetry target;
		WeaponBenchmark benchmark;
		std::vector<HitRecord> hits;
	};

	// The player's hostile effects as the Minimalist HUD resource bars show them; independent of the poise debuff
	// switches because it reads the raw effect scan.
	struct ReactiveBarView
	{
		bool healthPoison = false;
		bool healthFire = false;
		bool healthBleed = false;
		bool healthSapped = false;
		bool barShock = false;
		bool staminaFrost = false;
	};

	// Fixed-order ids for the HUD payload.
	[[nodiscard]] inline std::vector<std::string> ReactiveBarIds(const ReactiveBarView& view)
	{
		std::vector<std::string> ids;
		if (view.healthPoison) ids.push_back("health-poison");
		if (view.healthFire) ids.push_back("health-fire");
		if (view.healthBleed) ids.push_back("health-bleed");
		if (view.healthSapped) ids.push_back("health-sapped");
		if (view.barShock) ids.push_back("bar-shock");
		if (view.staminaFrost) ids.push_back("stamina-frost");
		return ids;
	}

	// The player's resolved debuffs for the Minimalist HUD icons: which are active and where recovery stops.
	struct PlayerDebuffView
	{
		std::array<bool, kDebuffCount> active{};
		float ceilingRatio = 1.0F;
	};

	// Active, enabled debuffs as the overlay pills show them, in fixed id order.
	[[nodiscard]] inline std::vector<ActorTelemetry::Debuff> BuildDebuffList(const Settings& settings, const PoiseDebuffs& debuffs)
	{
		std::vector<ActorTelemetry::Debuff> list;
		const auto fx = ResolveDebuffEffects(settings, debuffs);
		for (std::size_t i = 0; i < kDebuffCount; ++i) {
			if (!fx.active[i]) {
				continue;
			}
			const auto id = static_cast<DebuffId>(i);
			std::string detail;
			switch (id) {
			case DebuffId::sapped: detail = "0.0/s regen"; break;
			case DebuffId::enfeebled: detail = std::format("Cap {:.0f}%", fx.ceilingRatio * 100.0F); break;
			case DebuffId::numbed: detail = std::format("+{:.1f}s delay", fx.delayBonus); break;
			case DebuffId::hemorrhaging: detail = std::format("{:.2f}x regen", fx.regenRateMult); break;
			case DebuffId::scorched: detail = std::format("{:.2f}x melee taken", fx.scorchedMeleeMult); break;
			}
			list.push_back({ DebuffName(id), DebuffLabel(id), std::move(detail), debuffs.source[i] });
		}
		return list;
	}

	// JSON string escaping that never emits raw non-ASCII: valid UTF-8 becomes \uXXXX (surrogate pairs included) and
	// bytes that are not valid UTF-8, such as legacy Windows names, are taken as Latin-1.
	inline void AppendJsonString(std::string& out, std::string_view text)
	{
		const auto unicode = [&](std::uint32_t code) {
			if (code >= 0x10000) {
				code -= 0x10000;
				out += std::format("\\u{:04x}\\u{:04x}", 0xD800 + (code >> 10), 0xDC00 + (code & 0x3FF));
			} else {
				out += std::format("\\u{:04x}", code);
			}
		};
		out += '"';
		for (std::size_t i = 0; i < text.size();) {
			const auto byte = static_cast<unsigned char>(text[i]);
			if (byte < 0x80) {
				switch (byte) {
				case '"': out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					if (byte < 0x20 || byte == 0x7F) {
						unicode(byte);
					} else {
						out += static_cast<char>(byte);
					}
				}
				++i;
				continue;
			}
			std::size_t length = 0;
			std::uint32_t code = 0;
			if ((byte & 0xE0) == 0xC0) {
				length = 2;
				code = byte & 0x1F;
			} else if ((byte & 0xF0) == 0xE0) {
				length = 3;
				code = byte & 0x0F;
			} else if ((byte & 0xF8) == 0xF0) {
				length = 4;
				code = byte & 0x07;
			}
			bool valid = length != 0 && i + length <= text.size();
			for (std::size_t n = 1; valid && n < length; ++n) {
				const auto next = static_cast<unsigned char>(text[i + n]);
				valid = (next & 0xC0) == 0x80;
				code = (code << 6) | (next & 0x3F);
			}
			const std::uint32_t minimum = length == 2 ? 0x80 : length == 3 ? 0x800 : 0x10000;
			if (valid && code >= minimum && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF)) {
				unicode(code);
				i += length;
			} else {
				unicode(byte);
				++i;
			}
		}
		out += '"';
	}

	inline void AppendJsonNumber(std::string& out, float value)
	{
		out += std::format("{:.3f}", std::isfinite(value) ? value : 0.0F);
	}

	inline void AppendActorJson(std::string& out, const ActorTelemetry& actor)
	{
		out += "{\"name\":";
		AppendJsonString(out, actor.name);
		out += ",\"tier\":";
		AppendJsonString(out, actor.tier);
		const std::pair<const char*, float> numbers[] = {
			{ "current", actor.current }, { "max", actor.maximum }, { "ceilingRatio", actor.ceilingRatio },
			{ "ceilingPoise", actor.ceilingPoise }, { "regen", actor.regenRate }, { "delay", actor.regenDelay },
			{ "delayMax", actor.regenDelayMax }, { "immune", actor.immunity }, { "flesh", actor.fleshBonus },
			{ "lightArmor", actor.lightArmorBonus }
		};
		for (const auto& [key, value] : numbers) {
			out += std::format(",\"{}\":", key);
			AppendJsonNumber(out, value);
		}
		out += std::format(",\"stale\":{}", actor.stale);
		out += ",\"debuffs\":[";
		for (std::size_t i = 0; i < actor.debuffs.size(); ++i) {
			const auto& debuff = actor.debuffs[i];
			out += i ? ",{\"id\":" : "{\"id\":";
			AppendJsonString(out, debuff.id);
			out += ",\"label\":";
			AppendJsonString(out, debuff.label);
			out += ",\"detail\":";
			AppendJsonString(out, debuff.detail);
			out += ",\"source\":";
			AppendJsonString(out, debuff.source);
			out += '}';
		}
		out += "]}";
	}

	[[nodiscard]] inline std::string TelemetryJson(const TelemetrySnapshot& snapshot)
	{
		std::string out = "{\"player\":";
		AppendActorJson(out, snapshot.player);
		out += ",\"target\":";
		if (snapshot.hasTarget) {
			AppendActorJson(out, snapshot.target);
		} else {
			out += "null";
		}
		const auto& weapon = snapshot.benchmark;
		out += std::format(",\"weapon\":{{\"armed\":{},\"twoHanded\":{},\"ranged\":{},\"light\":",
			weapon.armed, weapon.twoHanded, weapon.ranged);
		AppendJsonNumber(out, weapon.lightHit);
		out += ",\"power\":";
		AppendJsonNumber(out, weapon.powerHit);
		out += ",\"rawEstimate\":";
		AppendJsonNumber(out, weapon.estimatedRawBonus);
		out += std::format(",\"rawCapped\":{}}},\"hits\":[", weapon.rawCapped);
		for (std::size_t i = 0; i < snapshot.hits.size(); ++i) {
			const auto& hit = snapshot.hits[i];
			out += i ? ",{\"dir\":" : "{\"dir\":";
			AppendJsonString(out, hit.outgoing ? "OUT" : "IN");
			out += ",\"attacker\":";
			AppendJsonString(out, hit.attacker);
			out += ",\"target\":";
			AppendJsonString(out, hit.target);
			out += ",\"type\":";
			AppendJsonString(out, hit.type);
			out += ",\"base\":";
			AppendJsonNumber(out, hit.baseDamage);
			out += ",\"raw\":";
			AppendJsonNumber(out, hit.rawBonus);
			out += std::format(",\"rawCapped\":{},\"mults\":[", hit.rawCapped);
			for (std::size_t n = 0; n < hit.tags.size(); ++n) {
				out += n ? ",[" : "[";
				AppendJsonString(out, hit.tags[n].first);
				out += ',';
				AppendJsonNumber(out, hit.tags[n].second);
				out += ']';
			}
			out += "],\"final\":";
			AppendJsonNumber(out, hit.finalDamage);
			out += std::format(",\"broke\":{},\"immune\":{}}}", hit.broke, hit.immune);
		}
		out += "]}";
		return out;
	}
}
