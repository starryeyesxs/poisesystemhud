#include "PrismaUI_API.h"
#include "builtin_poise.h"
#include "hud_menu_visibility.h"
#include "stagger_suppression.h"
#ifdef ARPGHUD_MAGIC_DIAGNOSTICS
#include "magic_diagnostic.h"
#endif
#ifdef ARPGHUD_MAGIC_GAMEPLAY
#include "magic_gameplay.h"
#endif
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
#include "stagger_diagnostic.h"
#endif
#include "poise_bridge.h"
#include <keyhandler/keyhandler.h>
#ifdef ARPGHUD_ENABLE_MENU
#include "menu_framework.h"
#endif

#include <atomic>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace
{
	constexpr auto kViewPath = "poisesystemhud/index.html";
	constexpr DWORD kStatsUpdateIntervalMs = 100;

	struct HudSettings
	{
		bool enableWeatherWidget = true;
		bool enableSneakLightGem = true;
		int sideOffsetLeft = 20;
		int bottomOffset = 16;
		bool showPoiseBar = true;
		bool showPoiseNumbers = false;
		bool showTargetPoiseBar = false;
		bool showPoiseTelemetry = false;
		bool showPoiseDebuffIcons = true;
		bool showReactiveBarEffects = true;
		bool dynamicHud = true;
		bool alwaysOn = false;
		float idleOpacity = 0.0F;
		float fadeDelaySeconds = 3.0F;
		bool minBarFiligree = true;
		float minBarShine = 1.0F;
		float minBarShadow = 1.0F;
		bool minBarNotches = false;
	};

	HudSettings settings;
	BuiltinPoise::Settings poiseSettings;
	KeyHandlerEvent reloadKeyHandler = INVALID_REGISTRATION_HANDLE;
	PRISMA_UI_API::IVPrismaUI1* prismaUI = nullptr;
	const SKSE::TaskInterface* taskInterface = nullptr;
	PrismaView hudView = 0;
	bool viewCreationAttempted = false;
	bool viewCreated = false;
	bool viewReady = false;
	bool viewVisible = false;
	bool saveLoaded = false;
	std::unordered_set<std::string> openMenus;
	HANDLE pollTimer = nullptr;
	std::atomic_bool timerEnabled{ false };
	std::atomic_bool taskPending{ false };
	bool hasSentPoise = false;
	bool lastPoiseVisible = false;
	bool lastPoiseNumbers = false;
	float lastPoiseCurrent = 0.0F;
	float lastPoiseMaximum = 0.0F;
	std::string lastTargetScript;
	std::string lastTelemetryScript;
	std::string lastDebuffIconScript;
	std::string lastReactiveBarScript;
	bool meterErrorLogged = false;
	bool invalidViewLogged = false;
	std::string lastWorldScript;
	std::string lastLightScript;
	bool resetMinimalPending = false;

	void RegisterReloadKey();

	void LoadSettings()
	{
		wchar_t executablePath[32768]{};
		const DWORD length = GetModuleFileNameW(nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
		if (length == 0 || length >= std::size(executablePath)) {
			logger::error("Cannot resolve Skyrim executable path for poisesystemhud.ini; retaining current settings.");
			return;
		}
		const auto iniPath = std::filesystem::path(executablePath).parent_path() /
			L"Data" / L"SKSE" / L"Plugins" / L"poisesystemhud.ini";
		if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			logger::warn("Cannot access poisesystemhud.ini (Windows error {}); retaining current settings.", GetLastError());
			return;
		}

		auto readValue = [&](const wchar_t* key, const wchar_t* section = L"HUD") -> std::string {
			wchar_t buffer[128]{};
			const DWORD size = GetPrivateProfileStringW(section, key, L"", buffer,
				static_cast<DWORD>(std::size(buffer)), iniPath.c_str());
			if (size == 0 || size >= std::size(buffer) - 1) {
				logger::warn("Missing or oversized poisesystemhud.ini setting; retaining its previous value.");
				return {};
			}
			std::string value;
			value.reserve(size);
			for (DWORD i = 0; i < size; ++i) {
				if (buffer[i] > 127) {
					logger::warn("Non-ASCII poisesystemhud.ini setting; retaining its previous value.");
					return {};
				}
				value.push_back(static_cast<char>(buffer[i]));
			}
			const auto begin = value.find_first_not_of(" \t");
			const auto end = value.find_last_not_of(" \t");
			return begin == std::string::npos ? std::string{} : value.substr(begin, end - begin + 1);
		};

		auto readOffset = [&](const wchar_t* key, const char* name, int& target, int minimum) {
			const auto text = readValue(key);
			int value{};
			const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
			if (!text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
				value >= minimum && value <= 2000) {
				target = value;
			} else {
				logger::warn("Invalid {} '{}'; retaining {}.", name, text, target);
			}
		};
		readOffset(L"iSideOffsetLeft", "iSideOffsetLeft", settings.sideOffsetLeft, -500);
		readOffset(L"iBottomOffset", "iBottomOffset", settings.bottomOffset, -200);
		auto readBool = [&](const wchar_t* key, const char* name, bool& target, const wchar_t* section = L"HUD") {
			const auto text = readValue(key, section);
			if (text == "true" || text == "1") {
				target = true;
			} else if (text == "false" || text == "0") {
				target = false;
			} else {
				logger::warn("Invalid {} '{}'; retaining {}.", name, text, target);
			}
		};
		auto readRange = [&](const wchar_t* key, const char* name, float& target, float lo, float hi, const wchar_t* section = L"HUD") {
			const auto text = readValue(key, section);
			float value{};
			const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
			if (!text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
				std::isfinite(value) && value >= lo && value <= hi) {
				target = value;
			} else {
				logger::warn("Invalid {} '{}'; retaining {}.", name, text, target);
			}
		};
		readBool(L"bEnableWeatherWidget", "bEnableWeatherWidget", settings.enableWeatherWidget);
		readBool(L"bEnableSneakLightGem", "bEnableSneakLightGem", settings.enableSneakLightGem);
		readBool(L"bShowPoiseBar", "bShowPoiseBar", settings.showPoiseBar);
		readBool(L"bShowPoiseNumbers", "bShowPoiseNumbers", settings.showPoiseNumbers);
		readBool(L"bShowTargetPoiseBar", "bShowTargetPoiseBar", settings.showTargetPoiseBar);
		readBool(L"bShowPoiseTelemetry", "bShowPoiseTelemetry", settings.showPoiseTelemetry);
		readBool(L"bShowPoiseDebuffIcons", "bShowPoiseDebuffIcons", settings.showPoiseDebuffIcons);
		readBool(L"bShowReactiveBarEffects", "bShowReactiveBarEffects", settings.showReactiveBarEffects);
		readBool(L"bDynamicHUD", "bDynamicHUD", settings.dynamicHud);
		readBool(L"bAlwaysOn", "bAlwaysOn", settings.alwaysOn);
		readRange(L"fIdleOpacity", "fIdleOpacity", settings.idleOpacity, 0.0F, 1.0F);
		readRange(L"fFadeDelaySeconds", "fFadeDelaySeconds", settings.fadeDelaySeconds, 0.0F, 600.0F);
		readBool(L"bMinBarFiligree", "bMinBarFiligree", settings.minBarFiligree, L"MinimalistGraphics");
		readRange(L"fMinBarShine", "fMinBarShine", settings.minBarShine, 0.0F, 2.0F, L"MinimalistGraphics");
		readRange(L"fMinBarShadow", "fMinBarShadow", settings.minBarShadow, 0.0F, 2.0F, L"MinimalistGraphics");
		readBool(L"bMinBarNotches", "bMinBarNotches", settings.minBarNotches, L"MinimalistGraphics");
		readBool(L"bEnableBuiltInPoise", "bEnableBuiltInPoise", poiseSettings.enabled, L"Poise");
		readBool(L"bEnableNPCPoise", "bEnableNPCPoise", poiseSettings.npcEnabled, L"Poise");
		readBool(L"bSuppressVanillaStaggers", "bSuppressVanillaStaggers", poiseSettings.suppressVanillaStaggers, L"Poise");
		readBool(L"bAllowTimedBashStaggers", "bAllowTimedBashStaggers", poiseSettings.allowTimedBashStaggers, L"Poise");
		readBool(L"bEnableCreaturePoise", "bEnableCreaturePoise", poiseSettings.enableCreaturePoise, L"Poise");
		readRange(L"fLightCreaturePoiseMultiplier", "fLightCreaturePoiseMultiplier", poiseSettings.lightCreaturePoiseMultiplier, 0.1F, 100.0F, L"Poise");
		readRange(L"fMediumCreaturePoiseMultiplier", "fMediumCreaturePoiseMultiplier", poiseSettings.mediumCreaturePoiseMultiplier, 0.1F, 100.0F, L"Poise");
		readRange(L"fHeavyCreaturePoiseMultiplier", "fHeavyCreaturePoiseMultiplier", poiseSettings.heavyCreaturePoiseMultiplier, 0.1F, 100.0F, L"Poise");
		readRange(L"fBossCreaturePoiseMultiplier", "fBossCreaturePoiseMultiplier", poiseSettings.bossCreaturePoiseMultiplier, 0.1F, 100.0F, L"Poise");
		readRange(L"fBossCreatureRegenMultiplier", "fBossCreatureRegenMultiplier", poiseSettings.bossCreatureRegenMultiplier, 0.0F, 100.0F, L"Poise");
		readRange(L"fBasePoise", "fBasePoise", poiseSettings.basePoise, 1.0F, 100000.0F, L"Poise");
		readRange(L"fArmorWeightMultiplier", "fArmorWeightMultiplier", poiseSettings.armorWeightMultiplier, 0.0F, 100.0F, L"Poise");
		readRange(L"fShieldWeightPoiseMultiplier", "fShieldWeightPoiseMultiplier", poiseSettings.shieldWeightPoiseMultiplier, -1.0F, 2.0F, L"Poise");
		readBool(L"bApplyShieldWeightToNPCs", "bApplyShieldWeightToNPCs", poiseSettings.applyShieldWeightToNPCs, L"Poise");
		readRange(L"fBaseHitPoiseDamage", "fBaseHitPoiseDamage", poiseSettings.baseHitDamage, 0.0F, 10000.0F, L"Poise");
		readRange(L"fWeaponWeightDamageMultiplier", "fWeaponWeightDamageMultiplier", poiseSettings.weaponWeightDamageMultiplier, 0.0F, 100.0F, L"Poise");
		readRange(L"fTwoHandedMultiplier", "fTwoHandedMultiplier", poiseSettings.twoHandedMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fShieldBashMultiplier", "fShieldBashMultiplier", poiseSettings.shieldBashMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fWeaponBashMultiplier", "fWeaponBashMultiplier", poiseSettings.weaponBashMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fCreatureMultiplier", "fCreatureMultiplier", poiseSettings.creatureMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fProjectileMultiplier", "fProjectileMultiplier", poiseSettings.projectileMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fPowerAttackMultiplier", "fPowerAttackMultiplier", poiseSettings.powerAttackMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fShieldBlockedPhysicalMultiplier", "fShieldBlockedPhysicalMultiplier", poiseSettings.shieldBlockedPhysicalMultiplier, 0.0F, 1.0F, L"Poise");
		readRange(L"fWeaponBlockedPhysicalMultiplier", "fWeaponBlockedPhysicalMultiplier", poiseSettings.weaponBlockedPhysicalMultiplier, 0.0F, 1.0F, L"Poise");
		readRange(L"fLightCreatureDamageMultiplier", "fLightCreatureDamageMultiplier", poiseSettings.lightCreatureDamageMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fMediumCreatureDamageMultiplier", "fMediumCreatureDamageMultiplier", poiseSettings.mediumCreatureDamageMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fHeavyCreatureDamageMultiplier", "fHeavyCreatureDamageMultiplier", poiseSettings.heavyCreatureDamageMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fBossCreatureDamageMultiplier", "fBossCreatureDamageMultiplier", poiseSettings.bossCreatureDamageMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fCreatureStaggerImmunitySeconds", "fCreatureStaggerImmunitySeconds", poiseSettings.creatureStaggerImmunitySeconds, 0.0F, 60.0F, L"Poise");
		readRange(L"fSummonPoiseMultiplier", "fSummonPoiseMultiplier", poiseSettings.summonPoiseMultiplier, 0.1F, 100.0F, L"Poise");
		readRange(L"fNPCMagicPoiseMultiplier", "fNPCMagicPoiseMultiplier", poiseSettings.npcMagicPoiseMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fUnblockedPhysicalMultiplier", "fUnblockedPhysicalMultiplier", poiseSettings.unblockedPhysicalMultiplier, 0.0F, 10.0F, L"Poise");
		readRange(L"fRegenDelaySeconds", "fRegenDelaySeconds", poiseSettings.regenDelaySeconds, 0.0F, 600.0F, L"Poise");
		readRange(L"fRegenRatePerSecond", "fRegenRatePerSecond", poiseSettings.regenRatePerSecond, 0.0F, 10000.0F, L"Poise");
		readRange(L"fStaggerImmunitySeconds", "fStaggerImmunitySeconds", poiseSettings.staggerImmunitySeconds, 0.0F, 60.0F, L"Poise");
		readRange(L"fPhysicalDamagePoiseMultiplier", "fPhysicalDamagePoiseMultiplier", poiseSettings.physicalDamagePoiseMultiplier, 0.0F, 0.25F, L"Poise");
		readRange(L"fPhysicalDamagePoiseCap", "fPhysicalDamagePoiseCap", poiseSettings.physicalDamagePoiseCap, 0.0F, 25.0F, L"Poise");
		readRange(L"fLightArmorRatingPoiseMultiplier", "fLightArmorRatingPoiseMultiplier", poiseSettings.lightArmorRatingPoiseMultiplier, 0.0F, 0.5F, L"Poise");
		readRange(L"fLightArmorRatingPoiseCap", "fLightArmorRatingPoiseCap", poiseSettings.lightArmorRatingPoiseCap, 0.0F, 50.0F, L"Poise");
		readRange(L"fLightArmorFleshMultiplier", "fLightArmorFleshMultiplier", poiseSettings.lightArmorFleshMultiplier, 0.0F, 1.0F, L"Poise");
		readRange(L"fHeavyArmorFleshMultiplier", "fHeavyArmorFleshMultiplier", poiseSettings.heavyArmorFleshMultiplier, 0.0F, 1.0F, L"Poise");
		readBool(L"bEnablePoiseDebuffs", "bEnablePoiseDebuffs", poiseSettings.enablePoiseDebuffs, L"Poise Debuffs");
		readBool(L"bEnableSappedDebuff", "bEnableSappedDebuff", poiseSettings.enableSappedDebuff, L"Poise Debuffs");
		readBool(L"bEnableEnfeebledDebuff", "bEnableEnfeebledDebuff", poiseSettings.enableEnfeebledDebuff, L"Poise Debuffs");
		readBool(L"bEnableNumbedDebuff", "bEnableNumbedDebuff", poiseSettings.enableNumbedDebuff, L"Poise Debuffs");
		readBool(L"bEnableHemorrhagingDebuff", "bEnableHemorrhagingDebuff", poiseSettings.enableHemorrhagingDebuff, L"Poise Debuffs");
		readBool(L"bEnableScorchedDebuff", "bEnableScorchedDebuff", poiseSettings.enableScorchedDebuff, L"Poise Debuffs");
		readRange(L"fEnfeebledRegenCeilingMin", "fEnfeebledRegenCeilingMin", poiseSettings.enfeebledRegenCeilingMin, 0.0F, 1.0F, L"Poise Debuffs");
		readRange(L"fEnfeebledRegenCeilingMax", "fEnfeebledRegenCeilingMax", poiseSettings.enfeebledRegenCeilingMax, 0.0F, 1.0F, L"Poise Debuffs");
		readRange(L"fEnfeebledMagnitudeForMin", "fEnfeebledMagnitudeForMin", poiseSettings.enfeebledMagnitudeForMin, 0.1F, 1000.0F, L"Poise Debuffs");
		readRange(L"fNumbedRegenDelayBonus", "fNumbedRegenDelayBonus", poiseSettings.numbedRegenDelayBonus, 0.0F, 10.0F, L"Poise Debuffs");
		readRange(L"fHemorrhagingRegenRateMultiplier", "fHemorrhagingRegenRateMultiplier", poiseSettings.hemorrhagingRegenRateMultiplier, 0.0F, 1.0F, L"Poise Debuffs");
		readRange(L"fScorchedMeleeDamageMultiplier", "fScorchedMeleeDamageMultiplier", poiseSettings.scorchedMeleeDamageMultiplier, 0.5F, 3.0F, L"Poise Debuffs");
		readRange(L"fMinGlobalRegenCeiling", "fMinGlobalRegenCeiling", poiseSettings.minGlobalRegenCeiling, 0.0F, 1.0F, L"Poise Debuffs");
		if (const auto keywords = readValue(L"sHemorrhagingEffectKeywords", L"Poise Debuffs"); !keywords.empty()) {
			poiseSettings.hemorrhagingKeywords = keywords;
		}
		if (poiseSettings.enfeebledRegenCeilingMin > poiseSettings.enfeebledRegenCeilingMax) {
			logger::warn("fEnfeebledRegenCeilingMin exceeds fEnfeebledRegenCeilingMax; the two values are swapped.");
			std::swap(poiseSettings.enfeebledRegenCeilingMin, poiseSettings.enfeebledRegenCeilingMax);
		}
#ifdef ARPGHUD_MAGIC_GAMEPLAY
		readRange(L"fFleshSpellPoiseMultiplier", "fFleshSpellPoiseMultiplier", poiseSettings.fleshSpellPoiseMultiplier, 0.0F, 100.0F, L"Poise");
		readRange(L"fDragonhidePoiseBonus", "fDragonhidePoiseBonus", poiseSettings.dragonhidePoiseBonus, 0.0F, 100000.0F, L"Poise");
		readRange(L"fMagicBurstPoiseMultiplier", "fMagicBurstPoiseMultiplier", poiseSettings.magicBurstPoiseMultiplier, 0.0F, 100.0F, L"Poise");
		readRange(L"fMagicStaggerPoiseMultiplier", "fMagicStaggerPoiseMultiplier", poiseSettings.magicStaggerPoiseMultiplier, 0.0F, 100.0F, L"Poise");
		readRange(L"fConcentrationPoiseMultiplier", "fConcentrationPoiseMultiplier", poiseSettings.concentrationPoiseMultiplier, 0.0F, 100.0F, L"Poise");
		readRange(L"fConcentrationIntervalSeconds", "fConcentrationIntervalSeconds", poiseSettings.concentrationIntervalSeconds, 0.01F, 60.0F, L"Poise");
		readRange(L"fConcentrationMaxPoisePerInterval", "fConcentrationMaxPoisePerInterval", poiseSettings.concentrationMaxPoisePerInterval, 0.0F, 10000.0F, L"Poise");
		readBool(L"bConcentrationSpellsDealPoise", "bConcentrationSpellsDealPoise", poiseSettings.concentrationSpellsDealPoise, L"Magic Poise");
		readRange(L"fWardMagicDamageMultiplier", "fWardMagicDamageMultiplier", poiseSettings.wardMagicDamageMultiplier, 0.0F, 1.0F, L"Poise");
		readRange(L"fShieldBlockedMagicMultiplier", "fShieldBlockedMagicMultiplier", poiseSettings.shieldBlockedMagicMultiplier, 0.0F, 1.0F, L"Poise");
		readRange(L"fWeaponBlockedMagicMultiplier", "fWeaponBlockedMagicMultiplier", poiseSettings.weaponBlockedMagicMultiplier, 0.0F, 1.0F, L"Poise");
#endif
		BuiltinPoise::Configure(poiseSettings);
		BuiltinPoise::SetTelemetryEnabled(settings.showPoiseTelemetry);
		BuiltinPoise::SetReactiveBarsEnabled(settings.showReactiveBarEffects);
		logger::info("HUD position: left {}, bottom {}.", settings.sideOffsetLeft, settings.bottomOffset);
	}

	bool HasValidView()
	{
		if (!prismaUI || !viewCreated || hudView == 0) {
			return false;
		}

		logger::info("PrismaUI IsValid({}) starting", hudView);
		const bool valid = prismaUI->IsValid(hudView);
		logger::info("PrismaUI IsValid({}) completed: {}", hudView, valid);
		if (!valid && !invalidViewLogged) {
			logger::error("PrismaUI HUD view {} is no longer valid.", hudView);
			invalidViewLogged = true;
		} else if (valid) {
			invalidViewLogged = false;
		}
		return valid;
	}

	bool IsGameplayReady()
	{
		auto* ui = RE::UI::GetSingleton();
		return saveLoaded && ui &&
			!openMenus.contains(std::string(RE::MainMenu::MENU_NAME)) &&
			!openMenus.contains(std::string(RE::LoadingMenu::MENU_NAME)) &&
			!ui->IsMenuOpen(RE::MainMenu::MENU_NAME) &&
			!ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
	}

	bool HasBlockingMenus()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return true;
		}
		std::string blockers;
		std::string overlays;
		for (const auto& name : openMenus) {
			auto menu = ui->GetMenu(name);
			if (menu && HudMenuVisibility::IsPassiveOverlay(menu->AlwaysOpen(), menu->PausesGame(),
					menu->UsesMenuContext(), menu->UsesCursor(), menu->ApplicationMenu())) {
				if (!overlays.empty()) {
					overlays += ", ";
				}
				overlays += name;
				continue;
			}
			if (!blockers.empty()) {
				blockers += ", ";
			}
			blockers += name;
		}
		static std::string lastBlockers;
		static std::string lastOverlays;
		if (blockers != lastBlockers) {
			if (blockers.empty()) {
				logger::info("HUD menu blockers cleared.");
			} else {
				logger::info("HUD hidden by open menus: {}.", blockers);
			}
			lastBlockers = blockers;
		}
		if (overlays != lastOverlays) {
			if (overlays.empty()) {
				logger::info("No passive HUD overlay menus are open.");
			} else {
				logger::info("HUD ignoring passive overlay menus: {}.", overlays);
			}
			lastOverlays = overlays;
		}
		return !blockers.empty();
	}

	void UpdateHudVisibility()
	{
		if (!prismaUI || !HasValidView()) {
			return;
		}

		const bool shouldShow = viewReady && IsGameplayReady() && !HasBlockingMenus();
		if (shouldShow == viewVisible) {
			return;
		}
		if (shouldShow) {
			logger::info("PrismaUI Show({}) starting", hudView);
			prismaUI->Show(hudView);
			logger::info("PrismaUI Show({}) completed", hudView);
		} else {
			logger::info("PrismaUI Hide({}) starting", hudView);
			prismaUI->Hide(hudView);
			logger::info("PrismaUI Hide({}) completed", hudView);
		}
		viewVisible = shouldShow;
	}

	void SendPlayerStats()
	{
		if (!prismaUI || !IsGameplayReady() || !HasValidView()) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		auto* actorValues = player->AsActorValueOwner();
		const auto health = actorValues->GetActorValue(RE::ActorValue::kHealth);
		const auto maxHealth = player->GetActorValueMax(RE::ActorValue::kHealth);
		const auto magicka = actorValues->GetActorValue(RE::ActorValue::kMagicka);
		const auto maxMagicka = player->GetActorValueMax(RE::ActorValue::kMagicka);
		const auto stamina = actorValues->GetActorValue(RE::ActorValue::kStamina);
		const auto maxStamina = player->GetActorValueMax(RE::ActorValue::kStamina);
		if (!std::isfinite(health) || !std::isfinite(maxHealth) ||
			!std::isfinite(magicka) || !std::isfinite(maxMagicka) ||
			!std::isfinite(stamina) || !std::isfinite(maxStamina)) {
			logger::error("Player Health, Magicka, or Stamina contains a non-finite value.");
			return;
		}

		const auto* actorState = player->AsActorState();
		const bool combat = player->IsInCombat() || (actorState && actorState->IsWeaponDrawn());
		const bool moving = actorState && actorState->IsSprinting();
		const auto script = std::format("updateMinimalStats({:.9g},{:.9g},{:.9g},{:.9g},{:.9g},{:.9g},{},{})",
			health, maxHealth, magicka, maxMagicka, stamina, maxStamina, combat, moving);
		prismaUI->Invoke(hudView, script.c_str());
	}

	void SendTargetPoise(bool force);

	void SendPoise()
	{
		SendTargetPoise(!hasSentPoise);
		PoiseBridge::Values values;
		const bool visible = settings.showPoiseBar &&
			(BuiltinPoise::Read(values) || PoiseBridge::Read(RE::PlayerCharacter::GetSingleton(), values));
		if (!visible) {
			values = {};
		}
		if (hasSentPoise && visible == lastPoiseVisible && settings.showPoiseNumbers == lastPoiseNumbers &&
			values.current == lastPoiseCurrent && values.maximum == lastPoiseMaximum) {
			return;
		}
		const auto script = std::format("updateMinimalPoise({}, {:.9g}, {:.9g}, {})",
			visible, values.current, values.maximum, settings.showPoiseNumbers);
		prismaUI->Invoke(hudView, script.c_str());
		lastPoiseVisible = visible;
		lastPoiseNumbers = settings.showPoiseNumbers;
		lastPoiseCurrent = values.current;
		lastPoiseMaximum = values.maximum;
		hasSentPoise = true;
	}

	// Skyrim names can be UTF-8 or legacy Windows text; emit only escaped UTF-16 in JS source.
	std::string QuoteGameString(const char* text)
	{
		if (!text || !*text) {
			return "\"\"";
		}
		UINT codePage = CP_UTF8;
		DWORD flags = MB_ERR_INVALID_CHARS;
		int count = MultiByteToWideChar(codePage, flags, text, -1, nullptr, 0);
		if (count == 0) {
			codePage = CP_ACP;
			flags = 0;
			count = MultiByteToWideChar(codePage, flags, text, -1, nullptr, 0);
		}
		if (count == 0) {
			logger::error("Could not decode location name (Windows error {}).", GetLastError());
			return "\"Unknown Location\"";
		}
		std::wstring wide(static_cast<std::size_t>(count), L'\0');
		if (!MultiByteToWideChar(codePage, flags, text, -1, wide.data(), count)) {
			logger::error("Could not convert location name (Windows error {}).", GetLastError());
			return "\"Unknown Location\"";
		}
		std::string result = "\"";
		for (int i = 0; i < count - 1; ++i) {
			result += std::format("\\u{:04x}", static_cast<unsigned int>(wide[static_cast<std::size_t>(i)]));
		}
		return result + "\"";
	}

	void SendTargetPoise(bool force)
	{
		BuiltinPoise::TargetValues target;
		const bool visible = settings.showTargetPoiseBar && BuiltinPoise::ReadTarget(target);
		std::string script;
		if (visible && std::isfinite(target.current) && std::isfinite(target.maximum)) {
			script = std::format("updateTargetPoise(true, {}, {}, {:.1f}, {:.1f}, {})",
				QuoteGameString(target.name.c_str()), QuoteGameString(target.tier.c_str()),
				target.current, target.maximum, target.immune);
		} else {
			script = "updateTargetPoise(false, \"\", \"\", 0, 0, false)";
		}
		if (!force && script == lastTargetScript) {
			return;
		}
		prismaUI->Invoke(hudView, script.c_str());
		lastTargetScript = std::move(script);
	}

	// Pushes the live poise dev overlay; the 10 Hz poll is the throttle, and unchanged payloads are not resent.
	void SendPoiseTelemetry()
	{
		if (!settings.showPoiseTelemetry) {
			return;
		}
		BuiltinPoise::TelemetrySnapshot snapshot;
		BuiltinPoise::RefreshTelemetryTarget();
		const auto script = BuiltinPoise::ReadTelemetry(snapshot) ?
			std::format("updatePoiseTelemetry({})", BuiltinPoise::TelemetryJson(snapshot)) :
			std::string{ "updatePoiseTelemetry(null)" };
		if (script == lastTelemetryScript) {
			return;
		}
		prismaUI->Invoke(hudView, script.c_str());
		lastTelemetryScript = script;
	}

	// Must run on the UI task: shows or hides the overlay container to match settings.showPoiseTelemetry.
	void ConfigurePoiseTelemetryView()
	{
		lastTelemetryScript.clear();
		if (!prismaUI || !viewReady || !HasValidView()) {
			return;
		}
		const auto script = std::format("configureTelemetry({})", settings.showPoiseTelemetry);
		prismaUI->Invoke(hudView, script.c_str());
	}

	// Drives the Minimalist HUD debuff icons and the poise bar ceiling tick from the player's resolved debuffs. Runs
	// whenever the icons are enabled, independently of the telemetry overlay.
	void SendPoiseDebuffIcons()
	{
		if (!settings.showPoiseDebuffIcons) {
			return;
		}
		BuiltinPoise::PlayerDebuffView view;
		std::string script = "updatePoiseDebuffs([], 1)";
		if (BuiltinPoise::ReadPlayerDebuffs(view)) {
			std::string ids;
			for (std::size_t i = 0; i < BuiltinPoise::kDebuffCount; ++i) {
				if (view.active[i]) {
					ids += std::format("{}\"{}\"", ids.empty() ? "" : ",", BuiltinPoise::DebuffName(static_cast<BuiltinPoise::DebuffId>(i)));
				}
			}
			script = std::format("updatePoiseDebuffs([{}], {:.3f})", ids, view.ceilingRatio);
		}
		if (script == lastDebuffIconScript) {
			return;
		}
		prismaUI->Invoke(hudView, script.c_str());
		lastDebuffIconScript = std::move(script);
	}

	// Must run on the UI task: enables or hides the icon strip to match settings.showPoiseDebuffIcons.
	void ConfigurePoiseDebuffIconsView()
	{
		lastDebuffIconScript.clear();
		if (!prismaUI || !viewReady || !HasValidView()) {
			return;
		}
		const auto script = std::format("configurePoiseDebuffIcons({})", settings.showPoiseDebuffIcons);
		prismaUI->Invoke(hudView, script.c_str());
	}

	// Sends the player's active hostile-effect states for the Health, Magicka and Stamina bar visuals. Skipped
	// completely when the reactive bar effects are off.
	void SendReactiveBars()
	{
		if (!settings.showReactiveBarEffects) {
			return;
		}
		BuiltinPoise::ReactiveBarView view;
		std::string ids;
		if (BuiltinPoise::ReadReactiveBars(view)) {
			for (const auto& id : BuiltinPoise::ReactiveBarIds(view)) {
				ids += std::format("{}\"{}\"", ids.empty() ? "" : ",", id);
			}
		}
		auto script = std::format("updateReactiveBars([{}])", ids);
		if (script == lastReactiveBarScript) {
			return;
		}
		prismaUI->Invoke(hudView, script.c_str());
		lastReactiveBarScript = std::move(script);
	}

	// Must run on the UI task: enables the bar effects, or clears every reactive class when switched off.
	void ConfigureReactiveBarsView()
	{
		lastReactiveBarScript.clear();
		if (!prismaUI || !viewReady || !HasValidView()) {
			return;
		}
		const auto script = std::format("configureReactiveBars({})", settings.showReactiveBarEffects);
		prismaUI->Invoke(hudView, script.c_str());
	}

	// showpoisedev: toggles the same state as bShowPoiseTelemetry and the menu checkbox. Not written to the INI.
	bool ExecuteShowPoiseDev(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
		RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
	{
		settings.showPoiseTelemetry = !settings.showPoiseTelemetry;
		BuiltinPoise::SetTelemetryEnabled(settings.showPoiseTelemetry);
		const char* state = settings.showPoiseTelemetry ? "ON" : "OFF";
		if (auto* console = RE::ConsoleLog::GetSingleton()) {
			console->Print("Poise Telemetry Overlay: %s", state);
		}
		logger::info("showpoisedev toggled the poise telemetry overlay {}.", state);
		if (taskInterface) {
			taskInterface->AddUITask([]() { ConfigurePoiseTelemetryView(); });
		}
		return true;
	}

	// Skyrim has no API for new console commands, so an unused built-in command slot is repurposed.
	void RegisterShowPoiseDevCommand()
	{
		auto* command = RE::SCRIPT_FUNCTION::LocateConsoleCommand("ClearScreenBlood");
		if (!command) {
			logger::warn("Console command showpoisedev not registered: the donor console command slot was not found. Use bShowPoiseTelemetry or the menu checkbox.");
			return;
		}
		command->functionName = "showpoisedev";
		command->shortName = "";
		command->helpString = "Toggles the live poise telemetry dev overlay.";
		command->referenceFunction = false;
		command->SetParameters();
		command->executeFunction = ExecuteShowPoiseDev;
		command->conditionFunction = nullptr;
		logger::info("Console command showpoisedev registered.");
	}

	void SendWorldTelemetry(RE::PlayerCharacter* player)
	{
		if (!settings.enableWeatherWidget) {
			return;
		}
		auto* cell = player->GetParentCell();
		auto* calendar = RE::Calendar::GetSingleton();
		if (!cell || !calendar || !calendar->gameHour || !calendar->gameDay ||
			!calendar->gameMonth || !calendar->gameYear) {
			static bool logged = false;
			if (!logged) {
				logger::warn("World telemetry unavailable: waiting for player cell and calendar.");
				logged = true;
			}
			return;
		}
		const float hourValue = calendar->GetHour();
		const float dayValue = calendar->GetDay();
		if (!std::isfinite(hourValue) || hourValue < 0 || hourValue >= 24 ||
			!std::isfinite(dayValue) || dayValue < 1 || dayValue > 31 ||
			calendar->GetMonth() > 11 || calendar->GetYear() < 1) {
			logger::warn("Invalid game calendar values; world widget was not updated.");
			return;
		}
		const bool interior = cell->IsInteriorCell();
		const char* name = nullptr;
		if (auto* location = player->GetCurrentLocation()) {
			name = location->GetName();
		}
		if ((!name || !*name) && interior) {
			name = cell->GetName();
		}
		if (!name || !*name) {
			if (auto* world = player->GetWorldspace()) {
				name = world->GetName();
			}
		}
		if (!name || !*name) {
			name = interior ? "Unknown Location" : "Wilderness";
		}
		const char* weather = "unknown";
		if (!interior) {
			auto* sky = RE::Sky::GetSingleton();
			if (sky && sky->currentWeather) {
				const auto& data = sky->currentWeather->data;
				using Flag = RE::TESWeather::WeatherDataFlag;
				if (data.flags.any(Flag::kSnow)) weather = "snow";
				else if (data.flags.any(Flag::kRainy)) weather = data.thunderLightningFrequency > 0 ? "storm" : "rain";
				else if (data.flags.any(Flag::kCloudy)) weather = "cloudy";
				else if (data.flags.any(Flag::kPleasant)) weather = "clear";
			}
		}
		const auto script = std::format("updateWorldWidget({},{},{},{},{},{},\"{}\",{})",
			QuoteGameString(name), static_cast<int>(hourValue), calendar->GetMinutes(),
			static_cast<int>(dayValue), calendar->GetMonth() + 1, calendar->GetYear(), weather, interior);
		if (script != lastWorldScript) {
			prismaUI->Invoke(hudView, script.c_str());
			lastWorldScript = script;
		}
	}

	void SendLightTelemetry(RE::PlayerCharacter* player)
	{
		if (!settings.enableSneakLightGem) {
			return;
		}
		const bool sneaking = player->IsSneaking();
		std::string level = "null";
		if (sneaking) {
			// HighProcessData exposes lightLevel; its equivalence to the condition function needs in-game verification.
			auto* process = player->GetHighProcess();
			if (process && std::isfinite(process->lightLevel)) {
				level = std::format("{:.3f}", std::clamp(process->lightLevel, 0.0F, 100.0F));
			} else {
				static bool logged = false;
				if (!logged) {
					logger::warn("Player high-process light level unavailable; hiding light gem.");
					logged = true;
				}
			}
		}
		const auto script = std::format("updateLightGem({},{})", sneaking, level);
		if (script != lastLightScript) {
			prismaUI->Invoke(hudView, script.c_str());
			lastLightScript = script;
		}
	}

	void HideVanillaMeters()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !IsGameplayReady() || !ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)) {
			return;
		}

		auto hudMenu = ui->GetMenu<RE::HUDMenu>();
		if (!hudMenu || !hudMenu->uiMovie) {
			return;
		}

		auto& root = hudMenu->GetRuntimeData().root;
		RE::GFxValue::DisplayInfo hidden;
		hidden.SetVisible(false);
		hidden.SetAlpha(0.0);
		hidden.SetY(-2000.0);
		bool success = root.IsDisplayObject();
		for (const auto* name : { "Health", "Magica", "Stamina" }) {
			RE::GFxValue meter;
			const bool hiddenMeter = root.IsDisplayObject() &&
				root.GetMember(name, &meter) && meter.IsDisplayObject() &&
				meter.SetDisplayInfo(hidden);
			success = hiddenMeter && success;
		}
		if (!success && !meterErrorLogged) {
			logger::warn("Could not hide all vanilla Health, Magicka and Stamina clips; retrying.");
			meterErrorLogged = true;
		} else if (success) {
			meterErrorLogged = false;
		}
	}

	void OnDomReady(PrismaView readyView)
	{
		logger::info("HUD view DOM ready callback received for {}", readyView);
		if (!taskInterface) {
			logger::error("Cannot process HUD DOM readiness: SKSE task interface is unavailable.");
			return;
		}
		taskInterface->AddUITask([readyView]() {
			if (!prismaUI || readyView == 0 || readyView != hudView || !HasValidView()) {
				return;
			}
			const auto positionScript = std::format("updateHudPosition({}, {})", settings.sideOffsetLeft, settings.bottomOffset);
			prismaUI->Invoke(hudView, positionScript.c_str());
			const auto layoutScript = std::format("configureLayouts({}, {})",
				settings.enableWeatherWidget, settings.enableSneakLightGem);
			prismaUI->Invoke(hudView, layoutScript.c_str());
			const auto visibilityScript = std::format("configureVisibility({}, {}, {:.6f}, {:.6f})",
				settings.alwaysOn, settings.dynamicHud, settings.idleOpacity, settings.fadeDelaySeconds);
			prismaUI->Invoke(hudView, visibilityScript.c_str());
			const auto minGraphicsScript = std::format("updateMinimalGraphics({}, {:.6f}, {:.6f}, {})",
				settings.minBarFiligree, settings.minBarShine, settings.minBarShadow, settings.minBarNotches);
			prismaUI->Invoke(hudView, minGraphicsScript.c_str());
			viewReady = true;
			hasSentPoise = false;
			ConfigurePoiseTelemetryView();
			ConfigurePoiseDebuffIconsView();
			ConfigureReactiveBarsView();
			lastWorldScript.clear();
			lastLightScript.clear();
			resetMinimalPending = false;
			UpdateHudVisibility();
		});
	}

	void CreateHudView()
	{
		if (!prismaUI || viewCreationAttempted) {
			return;
		}

		viewCreationAttempted = true;
		logger::info("Before PrismaUI->CreateView('{}')", kViewPath);
		const auto newView = prismaUI->CreateView(kViewPath, OnDomReady);
		logger::info("After PrismaUI->CreateView('{}'): {}", kViewPath, newView);
		if (newView == 0) {
			logger::error("PrismaUI failed to create the HUD view at '{}'.", kViewPath);
			timerEnabled = false;
			return;
		}
		logger::info("PrismaUI IsValid({}) starting", newView);
		const bool valid = prismaUI->IsValid(newView);
		logger::info("PrismaUI IsValid({}) completed: {}", newView, valid);
		if (!valid) {
			logger::error("PrismaUI failed to create the HUD view at '{}'.", kViewPath);
			timerEnabled = false;
			return;
		}

		hudView = newView;
		viewCreated = true;
		viewReady = false;
		viewVisible = false;
		logger::info("PrismaUI Unfocus({}) starting", hudView);
		prismaUI->Unfocus(hudView);
		logger::info("PrismaUI Unfocus({}) completed", hudView);
		logger::info("PrismaUI Hide({}) starting", hudView);
		prismaUI->Hide(hudView);
		logger::info("PrismaUI Hide({}) completed", hudView);
	}

	void PollHud()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!prismaUI || !ui || !IsGameplayReady() || !ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)) {
			return;
		}

		if (!viewCreated) {
			if (!RE::PlayerCharacter::GetSingleton()) {
				return;
			}
			CreateHudView();
		}

		if (viewReady && HasValidView()) {
			HideVanillaMeters();
			UpdateHudVisibility();
			if (resetMinimalPending) {
				prismaUI->Invoke(hudView, "resetMinimal()");
				resetMinimalPending = false;
			}
			SendPlayerStats();
			SendPoise();
			SendPoiseTelemetry();
			SendPoiseDebuffIcons();
			SendReactiveBars();
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				SendWorldTelemetry(player);
				SendLightTelemetry(player);
			}
		}
	}

	VOID CALLBACK TimerCallback(PVOID, BOOLEAN)
	{
		if (!taskInterface || !timerEnabled.load() || taskPending.exchange(true)) {
			return;
		}
		taskInterface->AddUITask([]() {
			if (timerEnabled.load()) {
				PollHud();
#ifdef ARPGHUD_ENABLE_MENU
				MenuFramework::UpdatePendingWrites();
#endif
			}
			taskPending = false;
		});
	}

	void StartPolling()
	{
		if (!taskInterface || !prismaUI) {
			return;
		}
		timerEnabled = saveLoaded;
		if (!pollTimer && !CreateTimerQueueTimer(
				&pollTimer, nullptr, TimerCallback, nullptr,
				kStatsUpdateIntervalMs, kStatsUpdateIntervalMs, WT_EXECUTEDEFAULT)) {
			timerEnabled = false;
			logger::error("Could not start HUD polling timer (Windows error {}).", GetLastError());
		} else if (pollTimer) {
			logger::info("HUD polling timer started; gameplay polling {}.", timerEnabled.load() ? "enabled" : "waiting for a save");
		}
	}

	void ReloadHud()
	{
		if (!prismaUI || !viewReady || !IsGameplayReady() || !HasValidView()) {
			logger::debug("Ignoring HUD reload outside gameplay.");
			return;
		}
		LoadSettings();
		viewReady = false;
		UpdateHudVisibility();
		logger::info("PrismaUI Invoke({}) reload starting", hudView);
		prismaUI->Invoke(hudView, "window.location.reload()");
		logger::info("PrismaUI Invoke({}) reload completed", hudView);
		hasSentPoise = false;
	}

	void RegisterReloadKey()
	{
		if (reloadKeyHandler != INVALID_REGISTRATION_HANDLE) {
			return;
		}
		constexpr std::uint32_t scanCode = 0x44;
		const auto handle = KeyHandler::GetSingleton()->Register(scanCode, KeyEventType::KEY_DOWN, ReloadHud);
		if (handle == INVALID_REGISTRATION_HANDLE) {
			logger::error("Could not register F10 reload key.");
			return;
		}
		reloadKeyHandler = handle;
		logger::info("Bound F10 reload to DirectInput scan code 0x{:X}.", scanCode);
	}

	class MenuVisibilitySink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent* event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!event || event->menuName.empty() || event->menuName == RE::HUDMenu::MENU_NAME) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const std::string menuName(event->menuName.c_str());
			if (event->opening) {
				openMenus.insert(menuName);
				if (menuName == RE::MainMenu::MENU_NAME) {
					BuiltinPoise::EndGame();
					saveLoaded = false;
					timerEnabled = false;
					hasSentPoise = false;
				} else if (menuName == RE::LoadingMenu::MENU_NAME) {
					timerEnabled = false;
				}
			} else {
				openMenus.erase(menuName);
				if (saveLoaded && openMenus.count(std::string(RE::MainMenu::MENU_NAME)) == 0 &&
					openMenus.count(std::string(RE::LoadingMenu::MENU_NAME)) == 0) {
					timerEnabled = true;
				}
			}

			UpdateHudVisibility();
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	MenuVisibilitySink menuVisibilitySink;
}

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	if (!message) {
		return;
	}

	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded: {
		LoadSettings();
		BuiltinPoise::Install();
		StaggerSuppression::Install();
#ifdef ARPGHUD_MAGIC_GAMEPLAY
		MagicGameplay::Install();
#endif
#ifdef ARPGHUD_MAGIC_DIAGNOSTICS
		MagicDiagnostic::Install();
#endif
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
		StaggerDiagnostic::Install();
#endif
		taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface) {
			logger::critical("SKSE task interface is unavailable. The HUD cannot be initialized.");
			return;
		}
		logger::info("PrismaUI RequestPluginAPI starting");
		prismaUI = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
		logger::info("PrismaUI RequestPluginAPI completed: {}", prismaUI != nullptr);
		if (!prismaUI) {
			logger::critical("PrismaUI API is unavailable. The HUD view cannot be created.");
			return;
		}

		CreateHudView();
		if (!viewCreated) {
			logger::error("HUD startup stopped because view creation failed.");
			return;
		}
		StartPolling();
		if (auto* ui = RE::UI::GetSingleton()) {
			logger::info("Registering HUD menu visibility listener.");
			ui->AddEventSink(&menuVisibilitySink);
			logger::info("HUD menu visibility listener registered.");
		} else {
			logger::critical("Could not access Skyrim UI to register menu visibility events.");
			return;
		}
		KeyHandler::RegisterSink();
		RegisterReloadKey();
		RegisterShowPoiseDevCommand();
#ifdef ARPGHUD_ENABLE_MENU
		MenuFramework::Initialize();
#endif
		logger::info("SKSE kDataLoaded initialization completed.");
		break;
	}
	case SKSE::MessagingInterface::kPreLoadGame:
		logger::info("SKSE kPreLoadGame received.");
		BuiltinPoise::EndGame();
		saveLoaded = false;
		timerEnabled = false;
		hasSentPoise = false;
		UpdateHudVisibility();
		break;
	case SKSE::MessagingInterface::kPostLoadGame:
		logger::info("SKSE kPostLoadGame received; success={}.", message->data != nullptr);
		if (!message->data) {
			logger::warn("Save load failed; HUD polling remains disabled.");
			break;
		}
		[[fallthrough]];
	case SKSE::MessagingInterface::kNewGame:
		logger::info("Initializing HUD for loaded or new game.");
		BuiltinPoise::BeginGame();
		saveLoaded = true;
		hasSentPoise = false;
		lastWorldScript.clear();
		lastLightScript.clear();
		resetMinimalPending = true;
		StartPolling();
		UpdateHudVisibility();
		break;
	default:
		break;
	}
}

extern "C" DLLEXPORT bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	REL::Module::reset();

	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	SKSE::Init(a_skse, { .trampoline = true, .trampolineSize = 1 << 10 });
	if (!g_messaging->RegisterListener("SKSE", SKSEMessageHandler)) {
		logger::critical("Failed to register SKSE message listener.");
		return false;
	}
	return true;
}

extern "C" DLLEXPORT void SKSEPlugin_Unload()
{
#ifdef ARPGHUD_ENABLE_MENU
	MenuFramework::Shutdown();
#endif
	logger::info("Poise System and Minimalist HUD plugin unloaded.");
}

#ifdef ARPGHUD_ENABLE_MENU
// ========== Menu Integration Exports ==========

void SendSettingsToHudImpl(const std::string& changedKey)
{
	if (changedKey == "resetHud" || changedKey == "resetPoise") {
		LoadSettings();
	}
	if (changedKey == "fShieldWeightPoiseMultiplier" || changedKey == "bApplyShieldWeightToNPCs") {
		BuiltinPoise::Configure(poiseSettings);
	}
	if (changedKey == "bShowPoiseTelemetry") {
		BuiltinPoise::SetTelemetryEnabled(settings.showPoiseTelemetry);
	}
	if (changedKey == "bShowReactiveBarEffects") {
		BuiltinPoise::SetReactiveBarsEnabled(settings.showReactiveBarEffects);
	}
	if (!prismaUI || !viewReady || !HasValidView()) {
		return;
	}
	if (changedKey == "bShowPoiseTelemetry" || changedKey == "resetHud") {
		ConfigurePoiseTelemetryView();
	}
	if (changedKey == "bShowPoiseDebuffIcons" || changedKey == "resetHud") {
		ConfigurePoiseDebuffIconsView();
	}
	if (changedKey == "bShowReactiveBarEffects" || changedKey == "resetHud") {
		ConfigureReactiveBarsView();
	}

	const auto positionScript = std::format("updateHudPosition({}, {})", settings.sideOffsetLeft, settings.bottomOffset);
	const auto layoutScript = std::format("configureLayouts({}, {})",
		settings.enableWeatherWidget, settings.enableSneakLightGem);
	const auto visibilityScript = std::format("configureVisibility({}, {}, {:.6f}, {:.6f})",
		settings.alwaysOn, settings.dynamicHud, settings.idleOpacity, settings.fadeDelaySeconds);

	// Send minimalist graphics settings
	const auto minGraphicsScript = std::format("updateMinimalGraphics({}, {:.6f}, {:.6f}, {})",
		settings.minBarFiligree, settings.minBarShine, settings.minBarShadow, settings.minBarNotches);

	prismaUI->Invoke(hudView, positionScript.c_str());
	prismaUI->Invoke(hudView, layoutScript.c_str());
	prismaUI->Invoke(hudView, visibilityScript.c_str());
	prismaUI->Invoke(hudView, minGraphicsScript.c_str());
	if (changedKey == "bShowPoiseBar" || changedKey == "bShowPoiseNumbers" || changedKey == "resetHud") {
		SendPoise();
	}
	logger::debug("Sent HUD settings update (triggered by: {})", changedKey);
}

// Export settings for menu access
namespace menu_exports {
	bool* get_enableWeatherWidget() { return &settings.enableWeatherWidget; }
	bool* get_enableSneakLightGem() { return &settings.enableSneakLightGem; }
	int* get_sideOffsetLeft() { return &settings.sideOffsetLeft; }
	int* get_bottomOffset() { return &settings.bottomOffset; }
	bool* get_showPoiseBar() { return &settings.showPoiseBar; }
	bool* get_showPoiseNumbers() { return &settings.showPoiseNumbers; }
	bool* get_dynamicHud() { return &settings.dynamicHud; }
	bool* get_alwaysOn() { return &settings.alwaysOn; }
	float* get_idleOpacity() { return &settings.idleOpacity; }
	float* get_fadeDelaySeconds() { return &settings.fadeDelaySeconds; }

	// Poise settings
	bool* get_poiseEnabled() { return &poiseSettings.enabled; }
	bool* get_poiseNpcEnabled() { return &poiseSettings.npcEnabled; }
	bool* get_poiseSuppressVanillaStaggers() { return &poiseSettings.suppressVanillaStaggers; }
	bool* get_allowTimedBashStaggers() { return &poiseSettings.allowTimedBashStaggers; }
	bool* get_enableCreaturePoise() { return &poiseSettings.enableCreaturePoise; }
	float* get_lightCreaturePoiseMultiplier() { return &poiseSettings.lightCreaturePoiseMultiplier; }
	float* get_mediumCreaturePoiseMultiplier() { return &poiseSettings.mediumCreaturePoiseMultiplier; }
	float* get_heavyCreaturePoiseMultiplier() { return &poiseSettings.heavyCreaturePoiseMultiplier; }
	float* get_bossCreaturePoiseMultiplier() { return &poiseSettings.bossCreaturePoiseMultiplier; }
	float* get_bossCreatureRegenMultiplier() { return &poiseSettings.bossCreatureRegenMultiplier; }
	float* get_basePoise() { return &poiseSettings.basePoise; }
	float* get_armorWeightMultiplier() { return &poiseSettings.armorWeightMultiplier; }
	float* get_shieldWeightPoiseMultiplier() { return &poiseSettings.shieldWeightPoiseMultiplier; }
	bool* get_applyShieldWeightToNPCs() { return &poiseSettings.applyShieldWeightToNPCs; }
	float* get_baseHitDamage() { return &poiseSettings.baseHitDamage; }
	float* get_weaponWeightDamageMultiplier() { return &poiseSettings.weaponWeightDamageMultiplier; }
	float* get_twoHandedMultiplier() { return &poiseSettings.twoHandedMultiplier; }
	float* get_shieldBashMultiplier() { return &poiseSettings.shieldBashMultiplier; }
	float* get_weaponBashMultiplier() { return &poiseSettings.weaponBashMultiplier; }
	float* get_creatureMultiplier() { return &poiseSettings.creatureMultiplier; }
	float* get_projectileMultiplier() { return &poiseSettings.projectileMultiplier; }
	float* get_powerAttackMultiplier() { return &poiseSettings.powerAttackMultiplier; }
	float* get_shieldBlockedPhysicalMultiplier() { return &poiseSettings.shieldBlockedPhysicalMultiplier; }
	float* get_weaponBlockedPhysicalMultiplier() { return &poiseSettings.weaponBlockedPhysicalMultiplier; }
	float* get_lightCreatureDamageMultiplier() { return &poiseSettings.lightCreatureDamageMultiplier; }
	float* get_mediumCreatureDamageMultiplier() { return &poiseSettings.mediumCreatureDamageMultiplier; }
	float* get_heavyCreatureDamageMultiplier() { return &poiseSettings.heavyCreatureDamageMultiplier; }
	float* get_bossCreatureDamageMultiplier() { return &poiseSettings.bossCreatureDamageMultiplier; }
	float* get_creatureStaggerImmunitySeconds() { return &poiseSettings.creatureStaggerImmunitySeconds; }
	float* get_summonPoiseMultiplier() { return &poiseSettings.summonPoiseMultiplier; }
	float* get_npcMagicPoiseMultiplier() { return &poiseSettings.npcMagicPoiseMultiplier; }
	bool* get_showTargetPoiseBar() { return &settings.showTargetPoiseBar; }
	bool* get_showPoiseTelemetry() { return &settings.showPoiseTelemetry; }
	bool* get_showPoiseDebuffIcons() { return &settings.showPoiseDebuffIcons; }
	bool* get_showReactiveBarEffects() { return &settings.showReactiveBarEffects; }
	bool* get_enablePoiseDebuffs() { return &poiseSettings.enablePoiseDebuffs; }
	bool* get_enableSappedDebuff() { return &poiseSettings.enableSappedDebuff; }
	bool* get_enableEnfeebledDebuff() { return &poiseSettings.enableEnfeebledDebuff; }
	bool* get_enableNumbedDebuff() { return &poiseSettings.enableNumbedDebuff; }
	bool* get_enableHemorrhagingDebuff() { return &poiseSettings.enableHemorrhagingDebuff; }
	bool* get_enableScorchedDebuff() { return &poiseSettings.enableScorchedDebuff; }
	float* get_enfeebledRegenCeilingMin() { return &poiseSettings.enfeebledRegenCeilingMin; }
	float* get_enfeebledRegenCeilingMax() { return &poiseSettings.enfeebledRegenCeilingMax; }
	float* get_enfeebledMagnitudeForMin() { return &poiseSettings.enfeebledMagnitudeForMin; }
	float* get_numbedRegenDelayBonus() { return &poiseSettings.numbedRegenDelayBonus; }
	float* get_hemorrhagingRegenRateMultiplier() { return &poiseSettings.hemorrhagingRegenRateMultiplier; }
	float* get_scorchedMeleeDamageMultiplier() { return &poiseSettings.scorchedMeleeDamageMultiplier; }
	float* get_minGlobalRegenCeiling() { return &poiseSettings.minGlobalRegenCeiling; }
	float* get_unblockedPhysicalMultiplier() { return &poiseSettings.unblockedPhysicalMultiplier; }
	float* get_regenDelaySeconds() { return &poiseSettings.regenDelaySeconds; }
	float* get_regenRatePerSecond() { return &poiseSettings.regenRatePerSecond; }
	float* get_staggerImmunitySeconds() { return &poiseSettings.staggerImmunitySeconds; }
	float* get_wardMagicDamageMultiplier() { return &poiseSettings.wardMagicDamageMultiplier; }
	float* get_shieldBlockedMagicMultiplier() { return &poiseSettings.shieldBlockedMagicMultiplier; }
	float* get_weaponBlockedMagicMultiplier() { return &poiseSettings.weaponBlockedMagicMultiplier; }
	bool* get_concentrationSpellsDealPoise() { return &poiseSettings.concentrationSpellsDealPoise; }
	float* get_physicalDamagePoiseMultiplier() { return &poiseSettings.physicalDamagePoiseMultiplier; }
	float* get_physicalDamagePoiseCap() { return &poiseSettings.physicalDamagePoiseCap; }
	float* get_lightArmorRatingPoiseMultiplier() { return &poiseSettings.lightArmorRatingPoiseMultiplier; }
	float* get_lightArmorRatingPoiseCap() { return &poiseSettings.lightArmorRatingPoiseCap; }
	float* get_lightArmorFleshMultiplier() { return &poiseSettings.lightArmorFleshMultiplier; }
	float* get_heavyArmorFleshMultiplier() { return &poiseSettings.heavyArmorFleshMultiplier; }

	// Minimalist graphics settings
	bool* get_minBarFiligree() { return &settings.minBarFiligree; }
	float* get_minBarShine() { return &settings.minBarShine; }
	float* get_minBarShadow() { return &settings.minBarShadow; }
	bool* get_minBarNotches() { return &settings.minBarNotches; }
}
#endif
