#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace MagicPoise
{
	struct DamageInput
	{
		float magnitude = 0.0F;
		float stagger = 0.0F;
		float magnitudeMultiplier = 0.0F;
		float staggerMultiplier = 0.0F;
		float elementalResistance = 0.0F;
		float magicResistance = 0.0F;
		float wardMultiplier = 1.0F;
		bool wardIntercepted = false;
		bool magicBurst = false;
		// A channeled (kConcentration) spell deals no direct poise damage unless the setting allows it.
		bool concentrationSpell = false;
		bool concentrationAllowed = true;
	};

	inline float BurstMagnitudeDamage(float magnitude, float multiplier) noexcept
	{
		constexpr float softCapStartMagnitude = 40.0F;
		constexpr float softCapSlope = 0.55F;
		constexpr float softCapMaximum = 90.0F;
		if (magnitude <= softCapStartMagnitude) {
			return magnitude * multiplier;
		}
		return (std::min)(softCapMaximum,
			softCapStartMagnitude * multiplier + (magnitude - softCapStartMagnitude) * softCapSlope);
	}

	inline float Damage(const DamageInput& input)
	{
		if (!std::isfinite(input.magnitude) || !std::isfinite(input.stagger) ||
			!std::isfinite(input.magnitudeMultiplier) || !std::isfinite(input.staggerMultiplier) ||
			!std::isfinite(input.elementalResistance) || !std::isfinite(input.magicResistance) ||
			!std::isfinite(input.wardMultiplier) ||
			input.magnitude < 0.0F || input.stagger < 0.0F ||
			input.magnitudeMultiplier < 0.0F || input.staggerMultiplier < 0.0F ||
			input.wardMultiplier < 0.0F || input.wardMultiplier > 1.0F) {
			return 0.0F;
		}
		if (input.concentrationSpell && !input.concentrationAllowed) {
			return 0.0F;
		}
		const float elemental = 1.0F - std::clamp(input.elementalResistance, -100.0F, 85.0F) / 100.0F;
		const float magic = 1.0F - std::clamp(input.magicResistance, -100.0F, 85.0F) / 100.0F;
		const float magnitudeDamage = input.magicBurst ?
			BurstMagnitudeDamage(input.magnitude, input.magnitudeMultiplier) :
			input.magnitude * input.magnitudeMultiplier;
		const float raw = magnitudeDamage + input.stagger * input.staggerMultiplier;
		const float mitigation = input.wardIntercepted ? input.wardMultiplier : 1.0F;
		const float damage = raw * elemental * magic * mitigation;
		return std::isfinite(damage) ? damage : 0.0F;
	}

	// The factors Damage multiplies together, for the telemetry hit log; valid only for inputs Damage accepts.
	struct Explanation
	{
		float raw = 0.0F;
		float elemental = 1.0F;
		float magic = 1.0F;
		float ward = 1.0F;
	};

	inline Explanation Explain(const DamageInput& input)
	{
		Explanation result;
		const float magnitudeDamage = input.magicBurst ?
			BurstMagnitudeDamage(input.magnitude, input.magnitudeMultiplier) :
			input.magnitude * input.magnitudeMultiplier;
		result.raw = magnitudeDamage + input.stagger * input.staggerMultiplier;
		result.elemental = 1.0F - std::clamp(input.elementalResistance, -100.0F, 85.0F) / 100.0F;
		result.magic = 1.0F - std::clamp(input.magicResistance, -100.0F, 85.0F) / 100.0F;
		result.ward = input.wardIntercepted ? input.wardMultiplier : 1.0F;
		return result;
	}

	struct BurstKey
	{
		std::uint32_t projectile = 0;
		std::uint32_t target = 0;
		bool operator==(const BurstKey&) const = default;
	};

	struct BurstHash
	{
		std::size_t operator()(const BurstKey& key) const
		{
			const auto packed = (static_cast<std::uint64_t>(key.projectile) << 32) | key.target;
			return std::hash<std::uint64_t>{}(packed);
		}
	};

	enum class BurstResult { accepted, duplicate, invalid, full };

	class BurstDeduplicator
	{
	public:
		explicit BurstDeduplicator(std::size_t limit) : limit_(limit) {}

		// Keys must be generation-bearing handles; retire only when the projectile lifetime ends.
		BurstResult Accept(BurstKey key)
		{
			if (key.projectile == 0 || key.target == 0) {
				return BurstResult::invalid;
			}
			if (hits_.contains(key)) {
				return BurstResult::duplicate;
			}
			if (hits_.size() >= limit_) {
				return BurstResult::full;
			}
			hits_.insert(key);
			return BurstResult::accepted;
		}

		void Retire(std::uint32_t projectile)
		{
			std::erase_if(hits_, [projectile](const auto& key) { return key.projectile == projectile; });
		}

		void Clear() { hits_.clear(); }

	private:
		std::size_t limit_;
		std::unordered_set<BurstKey, BurstHash> hits_;
	};

	class ConcentrationPool
	{
	public:
		void Add(float damage, float interval, float cap)
		{
			if (!std::isfinite(damage) || !std::isfinite(interval) || !std::isfinite(cap) ||
				damage <= 0.0F || interval <= 0.0F || cap <= 0.0F) {
				return;
			}
			if (remaining_ <= 0.0F) {
				remaining_ = interval;
			}
			pending_ = (std::min)(cap, pending_ + damage);
		}

		float Advance(float delta, float cap)
		{
			if (!std::isfinite(delta) || !std::isfinite(cap) || delta <= 0.0F || cap < 0.0F ||
				remaining_ <= 0.0F) {
				return 0.0F;
			}
			remaining_ = (std::max)(0.0F, remaining_ - delta);
			if (remaining_ > 0.0F) {
				return 0.0F;
			}
			const float result = (std::min)(pending_, cap);
			pending_ = 0.0F;
			return result;
		}

		void Reset()
		{
			pending_ = 0.0F;
			remaining_ = 0.0F;
		}

	private:
		float pending_ = 0.0F;
		float remaining_ = 0.0F;
	};
}
