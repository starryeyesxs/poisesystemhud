#include "../src/magic_poise_math.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{
	bool Near(float a, float b) { return std::abs(a - b) < 0.001F; }
}

int main()
{
	using namespace MagicPoise;
	DamageInput input;
	input.magnitude = 40.0F;
	input.magnitudeMultiplier = 2.0F;
	input.stagger = 0.25F;
	input.staggerMultiplier = 10.0F;
	assert(Near(Damage(input), 82.5F));
	assert(Damage(input) >= 40.0F);
	input.elementalResistance = 50.0F;
	input.magicResistance = 50.0F;
	assert(Near(Damage(input), 20.625F));
	input.elementalResistance = -50.0F;
	input.magicResistance = 0.0F;
	assert(Near(Damage(input), 123.75F));
	input.elementalResistance = 200.0F;
	assert(Near(Damage(input), 12.375F));
	input.elementalResistance = 0.0F;
	input.wardMultiplier = 0.4F;
	assert(Near(Damage(input), 82.5F));
	input.wardIntercepted = true;
	assert(Near(Damage(input), 33.0F));
	input.wardMultiplier = 0.35F;
	assert(Near(Damage(input), 28.875F));
	input.elementalResistance = 50.0F;
	input.magicResistance = 50.0F;
	assert(Near(Damage(input), 7.21875F));
	input.wardIntercepted = false;
	assert(Near(Damage(input), 20.625F));
	input.elementalResistance = std::numeric_limits<float>::quiet_NaN();
	assert(Damage(input) == 0.0F);

	// Channeled concentration spells deal no direct poise damage unless allowed; impact spells are unaffected.
	input.elementalResistance = 0.0F;
	input.magicResistance = 0.0F;
	input.concentrationSpell = true;
	input.concentrationAllowed = false;
	assert(Damage(input) == 0.0F);
	input.concentrationAllowed = true;
	assert(Near(Damage(input), 82.5F));
	input.concentrationSpell = false;
	input.concentrationAllowed = false;
	assert(Near(Damage(input), 82.5F));
	assert(DamageInput{}.concentrationAllowed);

	BurstDeduplicator bursts(3);
	assert(bursts.Accept({ 0, 1 }) == BurstResult::invalid);
	assert(bursts.Accept({ 1, 10 }) == BurstResult::accepted);
	assert(bursts.Accept({ 1, 10 }) == BurstResult::duplicate);
	assert(bursts.Accept({ 1, 11 }) == BurstResult::accepted);
	assert(bursts.Accept({ 2, 10 }) == BurstResult::accepted);
	assert(bursts.Accept({ 3, 10 }) == BurstResult::full);
	bursts.Retire(1);
	assert(bursts.Accept({ 3, 10 }) == BurstResult::accepted);
	assert(bursts.Accept({ 1, 10 }) == BurstResult::accepted);
	bursts.Clear();
	assert(bursts.Accept({ 1, 10 }) == BurstResult::accepted);

	ConcentrationPool pool;
	pool.Add(8.0F, 1.0F, 15.0F);
	pool.Add(8.0F, 1.0F, 15.0F);
	assert(pool.Advance(0.5F, 15.0F) == 0.0F);
	assert(Near(pool.Advance(0.5F, 15.0F), 15.0F));
	assert(pool.Advance(1.0F, 15.0F) == 0.0F);
	pool.Add(7.0F, 1.0F, 15.0F);
	assert(Near(pool.Advance(2.0F, 15.0F), 7.0F));
	pool.Add(7.0F, 1.0F, 15.0F);
	assert(Near(pool.Advance(1.0F, 3.0F), 3.0F));
	pool.Add(7.0F, 1.0F, 15.0F);
	pool.Reset();
	assert(pool.Advance(2.0F, 15.0F) == 0.0F);
}
