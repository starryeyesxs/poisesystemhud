#pragma once

#include "builtin_poise_state.h"

#include <array>
#include <cctype>
#include <string_view>

namespace BuiltinPoise
{
	struct RaceTraits
	{
		bool npc = false;
		bool dragon = false;
		bool giant = false;
	};

	namespace detail
	{
		[[nodiscard]] inline bool ContainsI(std::string_view text, std::string_view needle) noexcept
		{
			if (needle.empty() || text.size() < needle.size()) {
				return false;
			}
			for (std::size_t i = 0; i + needle.size() <= text.size(); ++i) {
				bool match = true;
				for (std::size_t j = 0; j < needle.size(); ++j) {
					if (std::tolower(static_cast<unsigned char>(text[i + j])) !=
						std::tolower(static_cast<unsigned char>(needle[j]))) {
						match = false;
						break;
					}
				}
				if (match) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] inline bool StartsWithI(std::string_view text, std::string_view prefix) noexcept
		{
			return text.size() >= prefix.size() && ContainsI(text.substr(0, prefix.size()), prefix);
		}

		template <std::size_t N>
		[[nodiscard]] bool ContainsAny(std::string_view text, const std::array<std::string_view, N>& needles) noexcept
		{
			for (const auto needle : needles) {
				if (ContainsI(text, needle)) {
					return true;
				}
			}
			return false;
		}
	}

	// Classifies a race by editor ID first and keyword traits second; the first matching rule wins.
	[[nodiscard]] inline CreatureTier ClassifyRace(std::string_view editorID, const RaceTraits& traits) noexcept
	{
		using detail::ContainsAny;
		using detail::ContainsI;
		constexpr std::array<std::string_view, 6> excluded{
			"rabbit", "chicken", "slaughterfish", "butterfly", "dragonfly", "torchbug"
		};
		constexpr std::array<std::string_view, 5> boss{ "dragonpriest", "dragonrace", "alduin", "centurion", "lurker" };
		constexpr std::array<std::string_view, 7> heavy{
			"werewolf", "werebear", "bear", "troll", "hagraven", "gargoyle", "sphere"
		};
		constexpr std::array<std::string_view, 8> medium{
			"draugr", "skeleton", "falmer", "chaurus", "sabrecat", "frostbite", "wraith", "spriggan"
		};
		constexpr std::array<std::string_view, 5> light{ "wolf", "skeever", "mudcrab", "fox", "horker" };

		// "mammoth" contains "moth", so moths are only matched by prefix.
		if (ContainsAny(editorID, excluded) || detail::StartsWithI(editorID, "moth")) {
			return CreatureTier::Excluded;
		}
		// Mudcrabs and frostbite spiders also come in "giant" variants that must not be promoted to the boss tier.
		if (ContainsI(editorID, "mudcrab")) {
			return CreatureTier::Light;
		}
		if (ContainsI(editorID, "frostbite")) {
			return CreatureTier::Medium;
		}
		if (ContainsAny(editorID, boss) || ContainsI(editorID, "mammoth") || ContainsI(editorID, "giant") ||
			traits.dragon || traits.giant) {
			return CreatureTier::Boss;
		}
		// "chaurushunter" must be matched before the generic chaurus rule below.
		if (ContainsI(editorID, "chaurushunter") || ContainsAny(editorID, heavy)) {
			return CreatureTier::Heavy;
		}
		if (ContainsAny(editorID, medium)) {
			return CreatureTier::Medium;
		}
		if (ContainsAny(editorID, light)) {
			return CreatureTier::Light;
		}
		return traits.npc ? CreatureTier::Standard : CreatureTier::Unknown;
	}

	[[nodiscard]] inline bool IsDragonPriestName(std::string_view name) noexcept
	{
		constexpr std::array<std::string_view, 9> names{
			"dragon priest", "hevnoraak", "krosis", "morokei", "nahkriin", "otar the mad", "rahgot", "vokun", "miraak"
		};
		return detail::ContainsAny(name, names);
	}

	// Vanilla Ice Wolves share WolfRace with ordinary wolves, so they are told apart by display name.
	[[nodiscard]] inline bool IsIceWolfName(std::string_view name) noexcept
	{
		return detail::ContainsI(name, "ice wolf");
	}

	// Resolves the race tier against the actor's own NPC keyword, its display name and the creature master toggle.
	[[nodiscard]] inline CreatureTier ResolveActorTier(CreatureTier raceTier, bool actorIsNpc, std::string_view actorName,
		bool creatureEnabled) noexcept
	{
		if (raceTier == CreatureTier::Excluded) {
			return CreatureTier::Excluded;
		}
		if (actorIsNpc && raceTier == CreatureTier::Unknown) {
			raceTier = CreatureTier::Standard;
		} else if (!actorIsNpc && raceTier == CreatureTier::Standard) {
			raceTier = CreatureTier::Unknown;
		}
		if (actorIsNpc && raceTier == CreatureTier::Standard && IsDragonPriestName(actorName)) {
			raceTier = CreatureTier::Boss;
		} else if (!actorIsNpc && raceTier == CreatureTier::Light && IsIceWolfName(actorName)) {
			raceTier = CreatureTier::Medium;
		}
		if (!IsCreatureTier(raceTier) || creatureEnabled) {
			return raceTier;
		}
		// With creature poise off, armored races such as Draugr keep their pre-existing NPC behavior.
		return actorIsNpc ? CreatureTier::Standard : CreatureTier::Excluded;
	}
}
