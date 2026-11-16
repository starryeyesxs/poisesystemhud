#include "magic_diagnostic.h"

#ifdef ARPGHUD_MAGIC_DIAGNOSTICS
#include <atomic>
#include <array>
#include <cstring>

namespace
{
	constexpr unsigned kLimit = 200;
	std::atomic<unsigned> effectCount{ 0 };
	std::atomic<unsigned> fireStackCount{ 0 };
	std::atomic<unsigned> shockStackCount{ 0 };
	std::atomic<unsigned> wardCollisionCount{ 0 };
	std::atomic<unsigned> bodyCollisionCount{ 0 };
	thread_local std::uint32_t currentImpactHandle = 0;
	thread_local std::uint32_t currentHitHandle = 0;
	thread_local std::uint32_t currentProcessHandle = 0;
	thread_local std::uint32_t currentSpellCollisionHandle = 0;
	thread_local RE::FormID currentSpellCollisionTarget = 0;
	thread_local std::int32_t currentCollisionLayer = -1;

	bool TakeSample(std::atomic<unsigned>& count)
	{
		const auto sample = count.fetch_add(1, std::memory_order_relaxed);
		if (sample == kLimit) {
			logger::info("Magic diagnostic sample limit reached for this hook; restart the game for more samples.");
		}
		return sample < kLimit;
	}

	using AddTargetFunction = bool (*)(RE::MagicTarget*, RE::MagicTarget::AddTargetData*);
	AddTargetFunction originalAddTarget = nullptr;

	void TraceEffectStack(RE::FormID target, RE::FormID effect, unsigned castingSource)
	{
		auto& count = effect == 0x00013CA9 ? fireStackCount : shockStackCount;
		if (count.fetch_add(1, std::memory_order_relaxed) >= 8) {
			return;
		}
		void* frames[32]{};
		const auto captured = CaptureStackBackTrace(0, static_cast<DWORD>(std::size(frames)), frames, nullptr);
		const auto& module = REL::Module::get();
		const auto text = module.segment(REL::Segment::textx);
		std::string offsets;
		for (USHORT i = 0; i < captured; ++i) {
			const auto address = reinterpret_cast<std::uintptr_t>(frames[i]);
			if (address >= text.address() && address - text.address() < text.size()) {
				offsets += std::format(" {:X}", address - module.base());
			}
		}
		logger::info("Magic diagnostic effect stack: target={:08X} effect={:08X} castingSource={} frames={} gameRVAs={}",
			target, effect, castingSource, captured, offsets);
	}

	bool TraceAddTarget(RE::MagicTarget* target, RE::MagicTarget::AddTargetData* data)
	{
		const bool sample = target && data && data->magicItem && TakeSample(effectCount);
		RE::FormID targetID = 0;
		RE::FormID casterID = 0;
		RE::FormID spellID = 0;
		RE::FormID effectID = 0;
		RE::FormID sourceID = 0;
		float magnitude = 0.0F;
		float elementalResistance = 0.0F;
		float magicResistance = 0.0F;
		int resistanceActorValue = -1;
		unsigned castingSource = 0;
		std::uint32_t casterHandle = 0;
		const auto impactHandle = currentImpactHandle;
		const auto hitHandle = currentHitHandle;
		const auto processHandle = currentProcessHandle;
		const auto spellCollisionHandle = currentSpellCollisionHandle;
		const auto spellCollisionTarget = currentSpellCollisionTarget;
		const auto collisionLayer = currentCollisionLayer;
		if (sample) {
			auto* targetRef = target->GetTargetStatsObject();
			targetID = targetRef ? targetRef->GetFormID() : 0;
			casterID = data->caster ? data->caster->GetFormID() : 0;
			spellID = data->magicItem->GetFormID();
			effectID = data->effect && data->effect->baseEffect ? data->effect->baseEffect->GetFormID() : 0;
			const auto* effect = data->effect ? data->effect->baseEffect : nullptr;
			auto* targetActor = targetRef ? targetRef->As<RE::Actor>() : nullptr;
			if (effect) {
				const auto resist = effect->data.resistVariable;
				resistanceActorValue = static_cast<int>(resist);
				if (targetActor && (resist == RE::ActorValue::kResistFire ||
					resist == RE::ActorValue::kResistFrost || resist == RE::ActorValue::kResistShock)) {
					auto* owner = targetActor->AsActorValueOwner();
					elementalResistance = owner->GetActorValue(resist);
					magicResistance = owner->GetActorValue(RE::ActorValue::kResistMagic);
				}
			}
			sourceID = data->source ? data->source->GetFormID() : 0;
			magnitude = data->magnitude;
			castingSource = static_cast<unsigned>(data->castingSource);
			if (data->caster && data->caster->AsProjectile()) {
				casterHandle = data->caster->GetHandle().native_handle();
			}
		}
		const bool result = originalAddTarget(target, data);
		if (sample) {
			logger::info("Magic diagnostic effect: target={:08X} caster={:08X} casterProjectileHandle={:08X} enclosingImpactHandle={:08X} enclosingHitHandle={:08X} enclosingProcessHandle={:08X} spellCollisionHandle={:08X} spellCollisionTarget={:08X} enclosingCollisionLayer={} spell={:08X} effect={:08X} source={:08X} magnitude={} resistAV={} elementalResistance={} magicResistance={} castingSource={} accepted={}",
				targetID, casterID, casterHandle, impactHandle, hitHandle, processHandle, spellCollisionHandle, spellCollisionTarget, collisionLayer, spellID, effectID, sourceID, magnitude, resistanceActorValue, elementalResistance, magicResistance, castingSource, result);
			if (result && (effectID == 0x00013CA9 || effectID == 0x0005DBAE)) {
				TraceEffectStack(targetID, effectID, castingSource);
			}
		}
		return result;
	}

	using ImpactFunction = RE::Projectile::ImpactData* (*)(RE::Projectile*, RE::TESObjectREFR*, const RE::NiPoint3&,
		const RE::NiPoint3&, RE::hkpCollidable*, std::int32_t, bool);
	using HandleHitsFunction = bool (*)(RE::Projectile*, RE::hkpCollidable*);
	using ProcessImpactsFunction = bool (*)(RE::Projectile*);
	using SpellCollisionFunction = void (*)(RE::Projectile*, RE::TESObjectREFR*, const void*, float, float);
	SpellCollisionFunction originalSpellCollision = nullptr;
	using CollisionFunction = bool (*)(RE::Projectile*, RE::TESObjectREFR*, const void*, const void*,
		std::int32_t, std::int32_t, bool*);
	CollisionFunction originalCollision = nullptr;

	bool TraceCollision(RE::Projectile* projectile, RE::TESObjectREFR* target,
		const void* location, const void* velocity, std::int32_t layer, std::int32_t arg6, bool* applied)
	{
		const auto previousLayer = currentCollisionLayer;
		currentCollisionLayer = layer;
		const bool sample = projectile && projectile->GetProjectileRuntimeData().spell &&
			(layer == 29 || layer == 30) &&
			TakeSample(layer == 29 ? wardCollisionCount : bodyCollisionCount);
		std::uint32_t handle = 0;
		RE::FormID targetID = 0;
		RE::FormID spellID = 0;
		float wardBefore = 0.0F;
		RE::NiPointer<RE::Actor> actor;
		if (sample) {
			handle = projectile->GetHandle().native_handle();
			spellID = projectile->GetProjectileRuntimeData().spell->GetFormID();
			targetID = target ? target->GetFormID() : 0;
			actor = RE::NiPointer<RE::Actor>(target ? target->As<RE::Actor>() : nullptr);
			if (actor) {
				wardBefore = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWardPower);
			}
		}
		const bool result = originalCollision(projectile, target, location, velocity, layer, arg6, applied);
		currentCollisionLayer = previousLayer;
		if (sample) {
			const float wardAfter = actor ? actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWardPower) : 0.0F;
			logger::info("Magic diagnostic collision outcome: projectileHandle={:08X} target={:08X} spell={:08X} layer={} arg6={} wardBefore={} wardAfter={} result={} applied={}",
				handle, targetID, spellID, layer, arg6, wardBefore, wardAfter, result, applied ? *applied : false);
		}
		return result;
	}

	void TraceSpellCollision(RE::Projectile* projectile, RE::TESObjectREFR* target,
		const void* location, float arg4, float arg5)
	{
		const auto previousHandle = currentSpellCollisionHandle;
		const auto previousTarget = currentSpellCollisionTarget;
		currentSpellCollisionHandle = projectile ? projectile->GetHandle().native_handle() : 0;
		currentSpellCollisionTarget = target ? target->GetFormID() : 0;
		originalSpellCollision(projectile, target, location, arg4, arg5);
		currentSpellCollisionHandle = previousHandle;
		currentSpellCollisionTarget = previousTarget;
	}

	template <class T>
	struct ProcessTrace
	{
		static inline ProcessImpactsFunction original = nullptr;
		static inline std::atomic<unsigned> count{ 0 };

		static bool Thunk(RE::Projectile* projectile)
		{
			const auto previousHandle = currentProcessHandle;
			const bool magic = projectile && projectile->GetProjectileRuntimeData().spell;
			currentProcessHandle = magic ? projectile->GetHandle().native_handle() : 0;
			const auto handle = currentProcessHandle;
			const auto spellID = magic ? projectile->GetProjectileRuntimeData().spell->GetFormID() : 0;
			const bool result = original(projectile);
			currentProcessHandle = previousHandle;
			if (magic && TakeSample(count)) {
				logger::info("Magic diagnostic ProcessImpacts: kind={} projectileHandle={:08X} spell={:08X} result={}",
					T::name, handle, spellID, result);
			}
			return result;
		}
	};

	template <class T>
	struct HitTrace
	{
		static inline HandleHitsFunction original = nullptr;

		static bool Thunk(RE::Projectile* projectile, RE::hkpCollidable* collidable)
		{
			const auto previousHandle = currentHitHandle;
			currentHitHandle = projectile && projectile->GetProjectileRuntimeData().spell ?
				projectile->GetHandle().native_handle() : 0;
			const bool result = original(projectile, collidable);
			currentHitHandle = previousHandle;
			return result;
		}
	};

	template <class T>
	struct ImpactTrace
	{
		static inline ImpactFunction original = nullptr;
		static inline std::atomic<unsigned> count{ 0 };

		static RE::Projectile::ImpactData* Thunk(RE::Projectile* projectile, RE::TESObjectREFR* target,
			const RE::NiPoint3& location, const RE::NiPoint3& velocity,
			RE::hkpCollidable* collidable, std::int32_t arg6, bool arg7)
		{
			const bool sample = projectile && projectile->GetProjectileRuntimeData().spell && TakeSample(count);
			const auto previousHandle = currentImpactHandle;
			currentImpactHandle = sample ? projectile->GetHandle().native_handle() : 0;
			std::uint32_t projectileID = 0;
			std::uint32_t spellID = 0;
			std::uint32_t shooterHandle = 0;
			std::uint32_t targetID = 0;
			int layer = -1;
			float wardBefore = 0.0F;
			RE::NiPointer<RE::Actor> actor;
			if (sample) {
				const auto& runtime = projectile->GetProjectileRuntimeData();
				projectileID = projectile->GetFormID();
				spellID = runtime.spell->GetFormID();
				shooterHandle = runtime.shooter.native_handle();
				targetID = target ? target->GetFormID() : 0;
				layer = collidable ? static_cast<int>(collidable->GetCollisionLayer()) : -1;
				actor = RE::NiPointer<RE::Actor>(target ? target->As<RE::Actor>() : nullptr);
				if (actor) {
					wardBefore = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWardPower);
				}
			}
			const auto impactHandle = currentImpactHandle;
			auto* result = original(projectile, target, location, velocity, collidable, arg6, arg7);
			currentImpactHandle = previousHandle;
			if (sample) {
				const float wardAfter = actor ? actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWardPower) : 0.0F;
				logger::info("Magic diagnostic impact: kind={} projectile={:08X} projectileHandle={:08X} spell={:08X} shooterHandle={:08X} target={:08X} layer={} arg6={} arg7={} wardBefore={} wardAfter={} result={:X}",
					T::name, projectileID, impactHandle, spellID, shooterHandle, targetID, layer, arg6, arg7,
					wardBefore, wardAfter, reinterpret_cast<std::uintptr_t>(result));
			}
			return result;
		}
	};

	struct Beam { static constexpr auto name = "beam"; };
	struct Flame { static constexpr auto name = "flame"; };
	struct Missile { static constexpr auto name = "missile"; };

	template <class T>
	void InstallImpact(REL::Relocation<std::uintptr_t>& vtable, std::uintptr_t expected)
	{
		const auto slot = vtable.address() + 0xBD * sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		if (current != expected) {
			logger::warn("Magic diagnostic {} impact hook validation FAILED; leaving the existing vtable untouched.", T::name);
			return;
		}
		ImpactTrace<T>::original = reinterpret_cast<ImpactFunction>(current);
		vtable.write_vfunc(0xBD, ImpactTrace<T>::Thunk);
		const bool installed = *reinterpret_cast<const std::uintptr_t*>(slot) ==
			reinterpret_cast<std::uintptr_t>(&ImpactTrace<T>::Thunk);
		logger::info("Magic diagnostic {} impact hook installation: {}.", T::name, installed ? "passed" : "FAILED");
	}

	template <class T>
	void InstallHitTrace(REL::Relocation<std::uintptr_t>& vtable, std::uintptr_t expected)
	{
		const auto slot = vtable.address() + 0xBE * sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		if (current != expected) {
			logger::warn("Magic diagnostic {} HandleHits validation FAILED; leaving the existing vtable untouched.", T::name);
			return;
		}
		HitTrace<T>::original = reinterpret_cast<HandleHitsFunction>(current);
		vtable.write_vfunc(0xBE, HitTrace<T>::Thunk);
		const bool installed = *reinterpret_cast<const std::uintptr_t*>(slot) ==
			reinterpret_cast<std::uintptr_t>(&HitTrace<T>::Thunk);
		logger::info("Magic diagnostic {} HandleHits hook installation: {}.", T::name, installed ? "passed" : "FAILED");
	}

	template <class T>
	void InstallProcessTrace(REL::Relocation<std::uintptr_t>& vtable, std::uintptr_t expected)
	{
		const auto slot = vtable.address() + 0xAC * sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		if (current != expected) {
			logger::warn("Magic diagnostic {} ProcessImpacts validation FAILED; leaving the existing vtable untouched.", T::name);
			return;
		}
		ProcessTrace<T>::original = reinterpret_cast<ProcessImpactsFunction>(current);
		vtable.write_vfunc(0xAC, ProcessTrace<T>::Thunk);
		const bool installed = *reinterpret_cast<const std::uintptr_t*>(slot) ==
			reinterpret_cast<std::uintptr_t>(&ProcessTrace<T>::Thunk);
		logger::info("Magic diagnostic {} ProcessImpacts hook installation: {}.", T::name, installed ? "passed" : "FAILED");
	}
}

void MagicDiagnostic::Install()
{
	const auto& module = REL::Module::get();
	if (module.version() != SKSE::RUNTIME_SSE_1_7_104) {
		logger::warn("Magic diagnostic hooks disabled: only runtime 1.7.104.0 is validated.");
		return;
	}
	logger::warn("Magic diagnostic build active (ward collision outcomes): logging only; magic poise and ward drain are NOT implemented.");
	const auto site = REL::RelocationID(33742, 34526).address() + 0x20B;
	constexpr std::array<std::uint8_t, 16> expected{
		0x48, 0x8B, 0xD7, 0x49, 0x8B, 0xCE, 0xE8, 0x20,
		0x11, 0x00, 0x00, 0x0F, 0xB6, 0xD8, 0x84, 0xC0
	};
	if (std::memcmp(reinterpret_cast<const void*>(site - 6), expected.data(), expected.size()) == 0) {
		originalAddTarget = reinterpret_cast<AddTargetFunction>(module.base() + 0x5D30D0);
		SKSE::GetTrampoline().write_call<5>(site, TraceAddTarget);
		logger::info("Magic diagnostic effect hook installed after exact call-site validation.");
	} else {
		logger::warn("Magic diagnostic effect call-site validation FAILED; leaving the existing call untouched.");
	}
	const auto collisionSite = REL::ID(44205).address() + 0x1E1;
	constexpr std::array<std::uint8_t, 17> collisionBytes{
		0x4C, 0x8B, 0xC5, 0x49, 0x8B, 0xD6, 0x48, 0x8B, 0xCF,
		0xE8, 0xDA, 0x00, 0x00, 0x00, 0x41, 0x83, 0xFF
	};
	if (std::memcmp(reinterpret_cast<const void*>(collisionSite - 9), collisionBytes.data(), collisionBytes.size()) == 0) {
		originalSpellCollision = reinterpret_cast<SpellCollisionFunction>(module.base() + 0x8010C0);
		SKSE::GetTrampoline().write_call<5>(collisionSite, TraceSpellCollision);
		logger::info("Magic diagnostic spell collision call-site validation passed; hook installed.");
	} else {
		logger::warn("Magic diagnostic spell collision call-site validation FAILED; leaving the existing call untouched.");
	}
	const auto wardSite = REL::ID(44166).address() + 0x4DE;
	constexpr std::array<std::uint8_t, 17> wardBytes{
		0x4C, 0x8D, 0x4C, 0x24, 0x68, 0x4C, 0x8D, 0x44, 0x24, 0x58,
		0x49, 0x8B, 0xD6, 0x49, 0x8B, 0xCC, 0xE8
	};
	constexpr std::array<std::uint8_t, 4> wardDisplacement{ 0xED, 0x2E, 0x00, 0x00 };
	if (std::memcmp(reinterpret_cast<const void*>(wardSite - 16), wardBytes.data(), wardBytes.size()) == 0 &&
		std::memcmp(reinterpret_cast<const void*>(wardSite + 1), wardDisplacement.data(), wardDisplacement.size()) == 0) {
		originalCollision = reinterpret_cast<CollisionFunction>(module.base() + 0x800E00);
		SKSE::GetTrampoline().write_call<5>(wardSite, TraceCollision);
		logger::info("Magic diagnostic ward collision call-site validation passed; hook installed.");
	} else {
		logger::warn("Magic diagnostic ward collision call-site validation FAILED; leaving the existing call untouched.");
	}
	REL::Relocation<std::uintptr_t> beam{ RE::VTABLE_BeamProjectile[0] };
	REL::Relocation<std::uintptr_t> flame{ RE::VTABLE_FlameProjectile[0] };
	REL::Relocation<std::uintptr_t> missile{ RE::VTABLE_MissileProjectile[0] };
	InstallImpact<Beam>(beam, module.base() + 0x7E0460);
	InstallImpact<Flame>(flame, module.base() + 0x7EB270);
	InstallImpact<Missile>(missile, module.base() + 0x7F4F90);
	InstallHitTrace<Beam>(beam, module.base() + 0x8009C0);
	InstallHitTrace<Flame>(flame, module.base() + 0x8009C0);
	InstallHitTrace<Missile>(missile, module.base() + 0x8009C0);
	InstallProcessTrace<Beam>(beam, module.base() + 0x7FA910);
	InstallProcessTrace<Flame>(flame, module.base() + 0x7FA910);
	InstallProcessTrace<Missile>(missile, module.base() + 0x7F2AB0);
}
#endif
