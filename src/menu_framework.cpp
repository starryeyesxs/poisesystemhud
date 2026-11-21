#include "menu_framework.h"
#include <algorithm>
#include <filesystem>
#include <map>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>

// Forward declaration - defined in main.cpp
extern void SendSettingsToHudImpl(const std::string& changedKey);

// Global state for menu framework (file-scoped)
namespace {
	bool g_frameworkAvailable = false;
	int64_t g_menuEventId = -1;
	std::map<std::string, std::string> g_pendingIniWrites;
	std::chrono::steady_clock::time_point g_lastIniWriteTime;
	constexpr auto kIniWriteDebounceMs = 500;

	// Two-click confirmation state for reset buttons
	bool g_hudResetConfirm = false;
	std::chrono::steady_clock::time_point g_hudResetTime;
	bool g_poiseResetConfirm = false;
	std::chrono::steady_clock::time_point g_poiseResetTime;
	constexpr auto kResetConfirmWindowMs = 3000;  // 3 second window to confirm

	// Slider persistence state (replaces IsItemDeactivatedAfterEdit)
	bool g_iniDirty = false;
	std::chrono::steady_clock::time_point g_lastSliderChangeTime;
	constexpr auto kSliderDebounceMs = 300;
}

namespace menu_exports {
	bool* get_enableCustomHud();
	bool* get_enableWeatherWidget();
	bool* get_enableSneakLightGem();
	int* get_hudTheme();
	float* get_hudScale();
	int* get_sideOffsetLeft();
	int* get_bottomOffset();
	bool* get_showPoiseBar();
	bool* get_enablePoiseShield();
	int* get_poiseShieldOffsetX();
	int* get_poiseShieldOffsetY();
	bool* get_showPoiseNumbers();
	bool* get_dynamicHud();
	bool* get_alwaysOn();
	float* get_idleOpacity();
	float* get_fadeDelaySeconds();
	bool* get_poiseEnabled();
	bool* get_poiseNpcEnabled();
	bool* get_poiseSuppressVanillaStaggers();
	bool* get_poiseEnableHitPushBack();
	bool* get_enableCreaturePoise();
	float* get_lightCreaturePoiseMultiplier();
	float* get_mediumCreaturePoiseMultiplier();
	float* get_heavyCreaturePoiseMultiplier();
	float* get_bossCreaturePoiseMultiplier();
	float* get_bossCreatureRegenMultiplier();
	float* get_basePoise();
	float* get_armorWeightMultiplier();
	float* get_shieldWeightPoiseMultiplier();
	bool* get_applyShieldWeightToNPCs();
	float* get_baseHitDamage();
	float* get_creatureBaseHitPoiseDamage();
	float* get_weaponWeightDamageMultiplier();
	float* get_twoHandedMultiplier();
	float* get_shieldBashMultiplier();
	float* get_weaponBashMultiplier();
	float* get_shieldBlockedMagicMultiplier();
	float* get_weaponBlockedMagicMultiplier();
	bool* get_concentrationSpellsDealPoise();
	bool* get_allowTimedBashStaggers();
	float* get_projectileMultiplier();
	float* get_powerAttackMultiplier();
	float* get_shieldBlockedPhysicalMultiplier();
	float* get_weaponBlockedPhysicalMultiplier();
	float* get_lightCreatureDamageMultiplier();
	float* get_mediumCreatureDamageMultiplier();
	float* get_heavyCreatureDamageMultiplier();
	float* get_bossCreatureDamageMultiplier();
	float* get_creatureStaggerImmunitySeconds();
	float* get_summonPoiseMultiplier();
	float* get_npcMagicPoiseMultiplier();
	bool* get_showTargetPoiseBar();
	bool* get_showPoiseDebuffIcons();
	bool* get_showReactiveBarEffects();
	bool* get_enablePoiseDebuffs();
	bool* get_enableSappedDebuff();
	bool* get_enableEnfeebledDebuff();
	bool* get_enableNumbedDebuff();
	bool* get_enableHemorrhagingDebuff();
	bool* get_enableScorchedDebuff();
	float* get_enfeebledRegenCeilingMin();
	float* get_enfeebledRegenCeilingMax();
	float* get_enfeebledMagnitudeForMin();
	float* get_numbedRegenDelayBonus();
	float* get_hemorrhagingRegenRateMultiplier();
	float* get_scorchedMeleeDamageMultiplier();
	float* get_minGlobalRegenCeiling();
	float* get_regenDelaySeconds();
	float* get_regenRatePerSecond();
	float* get_staggerImmunitySeconds();
	float* get_wardMagicDamageMultiplier();
	float* get_clothingMagicPoiseMultiplier();
	float* get_lightArmorMagicPoiseMultiplier();
	float* get_heavyArmorMagicPoiseMultiplier();
	float* get_physicalDamagePoiseMultiplier();
	float* get_physicalDamagePoiseCap();
	float* get_lightArmorRatingPoiseMultiplier();
	float* get_lightArmorRatingPoiseCap();
	float* get_lightArmorFleshMultiplier();
	float* get_heavyArmorFleshMultiplier();
	bool* get_minBarFiligree();
	float* get_minBarShine();
	float* get_minBarShadow();
	bool* get_minBarNotches();
}

namespace MenuFramework
{
	// Forward declarations
	void ResetHudDefaults();
	void ResetPoiseDefaults();

	std::string FloatToString(float value)
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(3) << value;
		return oss.str();
	}

	void OnMenuRender()
	{
		// ===== SECTION 1: Layout & Positioning =====
		if (ImGuiMCP::CollapsingHeader("Layout & Positioning")) {
			auto* sideOffsetLeft = ::menu_exports::get_sideOffsetLeft();
			if (ImGuiMCP::SliderInt("Left Offset##layout", sideOffsetLeft, -500, 2000)) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("iSideOffsetLeft", std::to_string(*sideOffsetLeft), "HUD");
				SendSettingsToHudImpl("iSideOffsetLeft");
			}

			auto* bottomOffset = ::menu_exports::get_bottomOffset();
			if (ImGuiMCP::SliderInt("Bottom Offset##layout", bottomOffset, -200, 2000)) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("iBottomOffset", std::to_string(*bottomOffset), "HUD");
				SendSettingsToHudImpl("iBottomOffset");
			}

			auto* poiseShieldOffsetX = ::menu_exports::get_poiseShieldOffsetX();
			if (ImGuiMCP::SliderInt("Shield X Offset##layout", poiseShieldOffsetX, -500, 2000)) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("iPoiseShieldOffsetX", std::to_string(*poiseShieldOffsetX), "HUD");
				SendSettingsToHudImpl("iPoiseShieldOffsetX");
			}

			auto* poiseShieldOffsetY = ::menu_exports::get_poiseShieldOffsetY();
			if (ImGuiMCP::SliderInt("Shield Y Offset##layout", poiseShieldOffsetY, -200, 2000)) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("iPoiseShieldOffsetY", std::to_string(*poiseShieldOffsetY), "HUD");
				SendSettingsToHudImpl("iPoiseShieldOffsetY");
			}

			auto* hudScale = ::menu_exports::get_hudScale();
			if (ImGuiMCP::SliderFloat("HUD Scale##layout", hudScale, 0.70f, 1.50f, "%.2f")) {
				*hudScale = std::clamp(std::round(*hudScale * 20.0f) / 20.0f, 0.70f, 1.50f);
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fHudScale", FloatToString(*hudScale), "HUD");
				SendSettingsToHudImpl("fHudScale");
			}

		}

		// ===== SECTION 2: Widget Visibility & Display =====
		if (ImGuiMCP::CollapsingHeader("Widget Visibility & Display")) {
			auto* enableCustomHud = ::menu_exports::get_enableCustomHud();
			if (ImGuiMCP::Checkbox("Enable Custom Player HUD##widgets", enableCustomHud)) {
				const char* val = *enableCustomHud ? "true" : "false";
				QueueIniWrite("bEnableCustomHud", val, "HUD");
				SendSettingsToHudImpl("bEnableCustomHud");
			}

			auto* hudTheme = ::menu_exports::get_hudTheme();
			if (ImGuiMCP::Combo("HUD Theme##widgets", hudTheme, "Daedric Iron\0Minimalist\0")) {
				QueueIniWrite("sHudTheme", *hudTheme == 1 ? "Minimalist" : "Daedric Iron", "HUD");
				SendSettingsToHudImpl("sHudTheme");
			}

			auto* enableWeatherWidget = ::menu_exports::get_enableWeatherWidget();
			if (ImGuiMCP::Checkbox("Enable Weather Widget##widgets", enableWeatherWidget)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *enableWeatherWidget ? "true" : "false";
					QueueIniWrite("bEnableWeatherWidget", val, "HUD");
					SendSettingsToHudImpl("bEnableWeatherWidget");
				}
			}

			auto* enableSneakLightGem = ::menu_exports::get_enableSneakLightGem();
			if (ImGuiMCP::Checkbox("Enable Sneak Light Gem##widgets", enableSneakLightGem)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *enableSneakLightGem ? "true" : "false";
					QueueIniWrite("bEnableSneakLightGem", val, "HUD");
					SendSettingsToHudImpl("bEnableSneakLightGem");
				}
			}

			auto* showPoiseBar = ::menu_exports::get_showPoiseBar();
			if (ImGuiMCP::Checkbox("Show Poise Bar##widgets", showPoiseBar)) {
				const char* val = *showPoiseBar ? "true" : "false";
				QueueIniWrite("bShowPoiseBar", val, "HUD");
				SendSettingsToHudImpl("bShowPoiseBar");
			}

			auto* enablePoiseShield = ::menu_exports::get_enablePoiseShield();
			if (ImGuiMCP::Checkbox("Enable Forged Shield Crest##widgets", enablePoiseShield)) {
				const char* val = *enablePoiseShield ? "true" : "false";
				QueueIniWrite("bEnablePoiseShield", val, "HUD");
				SendSettingsToHudImpl("bEnablePoiseShield");
			}

			auto* showPoiseNumbers = ::menu_exports::get_showPoiseNumbers();
			if (ImGuiMCP::Checkbox("Show Poise Numbers##widgets", showPoiseNumbers)) {
				const char* val = *showPoiseNumbers ? "true" : "false";
				QueueIniWrite("bShowPoiseNumbers", val, "HUD");
				SendSettingsToHudImpl("bShowPoiseNumbers");
			}

			auto* showTargetPoiseBar = ::menu_exports::get_showTargetPoiseBar();
			if (ImGuiMCP::Checkbox("Show Target Poise Bar##widgets", showTargetPoiseBar)) {
				const char* val = *showTargetPoiseBar ? "true" : "false";
				QueueIniWrite("bShowTargetPoiseBar", val, "HUD");
				SendSettingsToHudImpl("bShowTargetPoiseBar");
			}

			auto* showPoiseDebuffIcons = ::menu_exports::get_showPoiseDebuffIcons();
			if (ImGuiMCP::Checkbox("Show Poise Debuff Icons##widgets", showPoiseDebuffIcons)) {
				const char* val = *showPoiseDebuffIcons ? "true" : "false";
				QueueIniWrite("bShowPoiseDebuffIcons", val, "HUD");
				SendSettingsToHudImpl("bShowPoiseDebuffIcons");
			}

			auto* showReactiveBarEffects = ::menu_exports::get_showReactiveBarEffects();
			if (ImGuiMCP::Checkbox("Show Reactive Resource Bar Effects##widgets", showReactiveBarEffects)) {
				const char* val = *showReactiveBarEffects ? "true" : "false";
				QueueIniWrite("bShowReactiveBarEffects", val, "HUD");
				SendSettingsToHudImpl("bShowReactiveBarEffects");
			}
		}

		// ===== SECTION 3: Dynamic Visibility & Opacity =====
		if (ImGuiMCP::CollapsingHeader("Dynamic Visibility & Opacity")) {
			auto* dynamicHud = ::menu_exports::get_dynamicHud();
			if (ImGuiMCP::Checkbox("Dynamic HUD##visibility", dynamicHud)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *dynamicHud ? "true" : "false";
					QueueIniWrite("bDynamicHUD", val, "HUD");
					SendSettingsToHudImpl("bDynamicHUD");
				}
			}

			auto* alwaysOn = ::menu_exports::get_alwaysOn();
			if (ImGuiMCP::Checkbox("Always On##visibility", alwaysOn)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *alwaysOn ? "true" : "false";
					QueueIniWrite("bAlwaysOn", val, "HUD");
					SendSettingsToHudImpl("bAlwaysOn");
				}
			}

			ImGuiMCP::Separator();

			auto* idleOpacity = ::menu_exports::get_idleOpacity();
			if (ImGuiMCP::SliderFloat("Idle Opacity##visibility", idleOpacity, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fIdleOpacity", FloatToString(*idleOpacity), "HUD");
				SendSettingsToHudImpl("fIdleOpacity");
			}

			auto* fadeDelaySeconds = ::menu_exports::get_fadeDelaySeconds();
			if (ImGuiMCP::SliderFloat("Fade Delay (seconds)##visibility", fadeDelaySeconds, 0.0f, 10.0f, "%.1f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fFadeDelaySeconds", FloatToString(*fadeDelaySeconds), "HUD");
				SendSettingsToHudImpl("fFadeDelaySeconds");
			}
		}

		// ===== SECTION 4B: Visual Effects (Minimalist Bars) =====
		if (ImGuiMCP::CollapsingHeader("Visual Effects (Minimalist Bars)")) {
			ImGuiMCP::TextDisabled("Minimalist Style Only");
			ImGuiMCP::Separator();
			
			auto* minBarFiligree = ::menu_exports::get_minBarFiligree();
			if (ImGuiMCP::Checkbox("Diamond End Caps & Trim##minbar", minBarFiligree)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *minBarFiligree ? "true" : "false";
					QueueIniWrite("bMinBarFiligree", val, "MinimalistGraphics");
					SendSettingsToHudImpl("bMinBarFiligree");
				}
			}

			auto* minBarShine = ::menu_exports::get_minBarShine();
			if (ImGuiMCP::SliderFloat("Specular Highlight & Sheen##minbar", minBarShine, 0.0f, 2.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fMinBarShine", FloatToString(*minBarShine), "MinimalistGraphics");
				SendSettingsToHudImpl("fMinBarShine");
			}

			auto* minBarShadow = ::menu_exports::get_minBarShadow();
			if (ImGuiMCP::SliderFloat("Drop Shadow & Depth##minbar", minBarShadow, 0.0f, 2.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fMinBarShadow", FloatToString(*minBarShadow), "MinimalistGraphics");
				SendSettingsToHudImpl("fMinBarShadow");
			}

			auto* minBarNotches = ::menu_exports::get_minBarNotches();
			if (ImGuiMCP::Checkbox("25/50/75% Notch Ticks##minbar", minBarNotches)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *minBarNotches ? "true" : "false";
					QueueIniWrite("bMinBarNotches", val, "MinimalistGraphics");
					SendSettingsToHudImpl("bMinBarNotches");
				}
			}
		}

		// ===== SECTION 5: Combat Balance (Poise Tuning) =====
		if (ImGuiMCP::CollapsingHeader("Combat Balance (Poise Tuning)")) {
			auto* poiseEnabled = ::menu_exports::get_poiseEnabled();
			if (ImGuiMCP::Checkbox("Enable Poise System##poise", poiseEnabled)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *poiseEnabled ? "true" : "false";
					QueueIniWrite("bEnableBuiltInPoise", val, "Poise");
					SendSettingsToHudImpl("bEnableBuiltInPoise");
				}
			}

			auto* poiseNpcEnabled = ::menu_exports::get_poiseNpcEnabled();
			if (ImGuiMCP::Checkbox("Enable NPC Poise##poise", poiseNpcEnabled)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *poiseNpcEnabled ? "true" : "false";
					QueueIniWrite("bEnableNPCPoise", val, "Poise");
					SendSettingsToHudImpl("bEnableNPCPoise");
				}
			}

			auto* enableCreaturePoise = ::menu_exports::get_enableCreaturePoise();
			if (ImGuiMCP::Checkbox("Enable Creature Poise##poise", enableCreaturePoise)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *enableCreaturePoise ? "true" : "false";
					QueueIniWrite("bEnableCreaturePoise", val, "Poise");
					SendSettingsToHudImpl("bEnableCreaturePoise");
				}
			}

			auto* suppressVanillaStaggers = ::menu_exports::get_poiseSuppressVanillaStaggers();
			if (ImGuiMCP::Checkbox("Suppress Vanilla Staggers##poise", suppressVanillaStaggers)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *suppressVanillaStaggers ? "true" : "false";
					QueueIniWrite("bSuppressVanillaStaggers", val, "Poise");
					SendSettingsToHudImpl("bSuppressVanillaStaggers");
				}
			}

			auto* allowTimedBashStaggers = ::menu_exports::get_allowTimedBashStaggers();
			auto* enableHitPushBack = ::menu_exports::get_poiseEnableHitPushBack();
			if (ImGuiMCP::Checkbox("Enable Hit Pushback##poise", enableHitPushBack)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *enableHitPushBack ? "true" : "false";
					QueueIniWrite("bEnableHitPushBack", val, "Poise");
					SendSettingsToHudImpl("bEnableHitPushBack");
				}
			}

			if (ImGuiMCP::Checkbox("Allow Timed Bash Staggers##poise", allowTimedBashStaggers)) {
				if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
					const char* val = *allowTimedBashStaggers ? "true" : "false";
					QueueIniWrite("bAllowTimedBashStaggers", val, "Poise");
					SendSettingsToHudImpl("bAllowTimedBashStaggers");
				}
			}

			ImGuiMCP::Separator();

			auto* basePoise = ::menu_exports::get_basePoise();
			if (ImGuiMCP::SliderFloat("Base Poise##poise", basePoise, 10.0f, 500.0f, "%.0f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fBasePoise", FloatToString(*basePoise), "Poise");
				SendSettingsToHudImpl("fBasePoise");
			}

			auto* armorWeightMult = ::menu_exports::get_armorWeightMultiplier();
			if (ImGuiMCP::SliderFloat("Armor Weight Multiplier##poise", armorWeightMult, 0.0f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fArmorWeightMultiplier", FloatToString(*armorWeightMult), "Poise");
				SendSettingsToHudImpl("fArmorWeightMultiplier");
			}

			auto* shieldWeightMult = ::menu_exports::get_shieldWeightPoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Shield Weight Poise Multiplier##poise", shieldWeightMult, -1.0f, 2.0f, "%.2f")) {
				*shieldWeightMult = std::clamp(std::round(*shieldWeightMult * 20.0f) / 20.0f, -1.0f, 2.0f);
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fShieldWeightPoiseMultiplier", FloatToString(*shieldWeightMult), "Poise");
				SendSettingsToHudImpl("fShieldWeightPoiseMultiplier");
			}

			auto* applyShieldWeightToNPCs = ::menu_exports::get_applyShieldWeightToNPCs();
			if (ImGuiMCP::Checkbox("Apply Shield Weight to NPCs##poise", applyShieldWeightToNPCs)) {
				QueueIniWrite("bApplyShieldWeightToNPCs", *applyShieldWeightToNPCs ? "true" : "false", "Poise");
				FlushIniWrites();
				SendSettingsToHudImpl("bApplyShieldWeightToNPCs");
			}

			auto* lightArmorRatingMult = ::menu_exports::get_lightArmorRatingPoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Light Armor Rating Poise Mult##poise", lightArmorRatingMult, 0.0f, 0.5f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fLightArmorRatingPoiseMultiplier", FloatToString(*lightArmorRatingMult), "Poise");
				SendSettingsToHudImpl("fLightArmorRatingPoiseMultiplier");
			}

			auto* lightArmorRatingCap = ::menu_exports::get_lightArmorRatingPoiseCap();
			if (ImGuiMCP::SliderFloat("Light Armor Rating Poise Cap##poise", lightArmorRatingCap, 0.0f, 50.0f, "%.1f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fLightArmorRatingPoiseCap", FloatToString(*lightArmorRatingCap), "Poise");
				SendSettingsToHudImpl("fLightArmorRatingPoiseCap");
			}

			auto* lightArmorFleshMult = ::menu_exports::get_lightArmorFleshMultiplier();
			if (ImGuiMCP::SliderFloat("Light Armor Flesh Efficiency##poise", lightArmorFleshMult, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fLightArmorFleshMultiplier", FloatToString(*lightArmorFleshMult), "Poise");
				SendSettingsToHudImpl("fLightArmorFleshMultiplier");
			}

			auto* heavyArmorFleshMult = ::menu_exports::get_heavyArmorFleshMultiplier();
			if (ImGuiMCP::SliderFloat("Heavy Armor Flesh Efficiency##poise", heavyArmorFleshMult, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fHeavyArmorFleshMultiplier", FloatToString(*heavyArmorFleshMult), "Poise");
				SendSettingsToHudImpl("fHeavyArmorFleshMultiplier");
			}

			auto* baseHitDamage = ::menu_exports::get_baseHitDamage();
			if (ImGuiMCP::SliderFloat("Base Hit Poise Damage##poise", baseHitDamage, 0.0f, 100.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fBaseHitPoiseDamage", FloatToString(*baseHitDamage), "Poise");
				SendSettingsToHudImpl("fBaseHitPoiseDamage");
			}

			auto* weaponWeightDamMult = ::menu_exports::get_weaponWeightDamageMultiplier();
			if (ImGuiMCP::SliderFloat("Weapon Weight Damage Mult##poise", weaponWeightDamMult, 0.0f, 5.0f, "%.3f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fWeaponWeightDamageMultiplier", FloatToString(*weaponWeightDamMult), "Poise");
				SendSettingsToHudImpl("fWeaponWeightDamageMultiplier");
			}

			auto* physicalDamageMult = ::menu_exports::get_physicalDamagePoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Physical Damage Poise Mult##poise", physicalDamageMult, 0.0f, 0.25f, "%.3f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fPhysicalDamagePoiseMultiplier", FloatToString(*physicalDamageMult), "Poise");
				SendSettingsToHudImpl("fPhysicalDamagePoiseMultiplier");
			}

			auto* physicalDamageCap = ::menu_exports::get_physicalDamagePoiseCap();
			if (ImGuiMCP::SliderFloat("Physical Damage Poise Cap##poise", physicalDamageCap, 0.0f, 25.0f, "%.1f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fPhysicalDamagePoiseCap", FloatToString(*physicalDamageCap), "Poise");
				SendSettingsToHudImpl("fPhysicalDamagePoiseCap");
			}

			ImGuiMCP::Separator();

			auto* twoHandedMult = ::menu_exports::get_twoHandedMultiplier();
			if (ImGuiMCP::SliderFloat("Two-Handed Multiplier##poise", twoHandedMult, 0.5f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fTwoHandedMultiplier", FloatToString(*twoHandedMult), "Poise");
				SendSettingsToHudImpl("fTwoHandedMultiplier");
			}

			auto* shieldBashMult = ::menu_exports::get_shieldBashMultiplier();
			if (ImGuiMCP::SliderFloat("Shield Bash Multiplier##poise", shieldBashMult, 0.0f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fShieldBashMultiplier", FloatToString(*shieldBashMult), "Poise");
				SendSettingsToHudImpl("fShieldBashMultiplier");
			}

			auto* weaponBashMult = ::menu_exports::get_weaponBashMultiplier();
			if (ImGuiMCP::SliderFloat("Weapon Bash Multiplier##poise", weaponBashMult, 0.0f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fWeaponBashMultiplier", FloatToString(*weaponBashMult), "Poise");
				SendSettingsToHudImpl("fWeaponBashMultiplier");
			}

			auto* lightCreaturePoise = ::menu_exports::get_lightCreaturePoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Light Creature Poise Mult##poise", lightCreaturePoise, 0.1f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fLightCreaturePoiseMultiplier", FloatToString(*lightCreaturePoise), "Poise");
				SendSettingsToHudImpl("fLightCreaturePoiseMultiplier");
			}

			auto* mediumCreaturePoise = ::menu_exports::get_mediumCreaturePoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Medium Creature Poise Mult##poise", mediumCreaturePoise, 0.1f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fMediumCreaturePoiseMultiplier", FloatToString(*mediumCreaturePoise), "Poise");
				SendSettingsToHudImpl("fMediumCreaturePoiseMultiplier");
			}

			auto* heavyCreaturePoise = ::menu_exports::get_heavyCreaturePoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Heavy Creature Poise Mult##poise", heavyCreaturePoise, 0.1f, 10.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fHeavyCreaturePoiseMultiplier", FloatToString(*heavyCreaturePoise), "Poise");
				SendSettingsToHudImpl("fHeavyCreaturePoiseMultiplier");
			}

			auto* bossCreaturePoise = ::menu_exports::get_bossCreaturePoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Boss Creature Poise Mult##poise", bossCreaturePoise, 0.1f, 20.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fBossCreaturePoiseMultiplier", FloatToString(*bossCreaturePoise), "Poise");
				SendSettingsToHudImpl("fBossCreaturePoiseMultiplier");
			}

			auto* bossCreatureRegen = ::menu_exports::get_bossCreatureRegenMultiplier();
			if (ImGuiMCP::SliderFloat("Boss Creature Regen Mult##poise", bossCreatureRegen, 0.0f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fBossCreatureRegenMultiplier", FloatToString(*bossCreatureRegen), "Poise");
				SendSettingsToHudImpl("fBossCreatureRegenMultiplier");
			}

			auto* creatureBaseHitDamage = ::menu_exports::get_creatureBaseHitPoiseDamage();
			if (ImGuiMCP::SliderFloat("Creature Base Hit Poise Damage##poise", creatureBaseHitDamage, 0.0f, 100.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fCreatureBaseHitPoiseDamage", FloatToString(*creatureBaseHitDamage), "Poise");
				SendSettingsToHudImpl("fCreatureBaseHitPoiseDamage");
			}

			auto* lightCreatureDamage = ::menu_exports::get_lightCreatureDamageMultiplier();
			if (ImGuiMCP::SliderFloat("Light Creature Damage Mult##poise", lightCreatureDamage, 0.0f, 5.0f, "%.3f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fLightCreatureDamageMultiplier", FloatToString(*lightCreatureDamage), "Poise");
				SendSettingsToHudImpl("fLightCreatureDamageMultiplier");
			}

			auto* mediumCreatureDamage = ::menu_exports::get_mediumCreatureDamageMultiplier();
			if (ImGuiMCP::SliderFloat("Medium Creature Damage Mult##poise", mediumCreatureDamage, 0.0f, 5.0f, "%.3f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fMediumCreatureDamageMultiplier", FloatToString(*mediumCreatureDamage), "Poise");
				SendSettingsToHudImpl("fMediumCreatureDamageMultiplier");
			}

			auto* heavyCreatureDamage = ::menu_exports::get_heavyCreatureDamageMultiplier();
			if (ImGuiMCP::SliderFloat("Heavy Creature Damage Mult##poise", heavyCreatureDamage, 0.0f, 5.0f, "%.3f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fHeavyCreatureDamageMultiplier", FloatToString(*heavyCreatureDamage), "Poise");
				SendSettingsToHudImpl("fHeavyCreatureDamageMultiplier");
			}

			auto* bossCreatureDamage = ::menu_exports::get_bossCreatureDamageMultiplier();
			if (ImGuiMCP::SliderFloat("Boss Creature Damage Mult##poise", bossCreatureDamage, 0.0f, 5.0f, "%.3f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fBossCreatureDamageMultiplier", FloatToString(*bossCreatureDamage), "Poise");
				SendSettingsToHudImpl("fBossCreatureDamageMultiplier");
			}

			auto* creatureImmunity = ::menu_exports::get_creatureStaggerImmunitySeconds();
			if (ImGuiMCP::SliderFloat("Creature Stagger Immunity (s)##poise", creatureImmunity, 0.0f, 10.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fCreatureStaggerImmunitySeconds", FloatToString(*creatureImmunity), "Poise");
				SendSettingsToHudImpl("fCreatureStaggerImmunitySeconds");
			}

			auto* summonPoise = ::menu_exports::get_summonPoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Summon Poise Multiplier##poise", summonPoise, 0.5f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fSummonPoiseMultiplier", FloatToString(*summonPoise), "Poise");
				SendSettingsToHudImpl("fSummonPoiseMultiplier");
			}

			auto* npcMagicPoise = ::menu_exports::get_npcMagicPoiseMultiplier();
			if (ImGuiMCP::SliderFloat("NPC Magic Poise Multiplier##poise", npcMagicPoise, 0.0f, 3.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fNPCMagicPoiseMultiplier", FloatToString(*npcMagicPoise), "Poise");
				SendSettingsToHudImpl("fNPCMagicPoiseMultiplier");
			}

			auto* projectileMult = ::menu_exports::get_projectileMultiplier();
			if (ImGuiMCP::SliderFloat("Projectile Multiplier##poise", projectileMult, 0.1f, 2.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fProjectileMultiplier", FloatToString(*projectileMult), "Poise");
				SendSettingsToHudImpl("fProjectileMultiplier");
			}

			auto* powerAttackMult = ::menu_exports::get_powerAttackMultiplier();
			if (ImGuiMCP::SliderFloat("Power Attack Multiplier##poise", powerAttackMult, 0.5f, 5.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fPowerAttackMultiplier", FloatToString(*powerAttackMult), "Poise");
				SendSettingsToHudImpl("fPowerAttackMultiplier");
			}

			auto* shieldBlockedMult = ::menu_exports::get_shieldBlockedPhysicalMultiplier();
			if (ImGuiMCP::SliderFloat("Shield Blocked Physical Mult##poise", shieldBlockedMult, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fShieldBlockedPhysicalMultiplier", FloatToString(*shieldBlockedMult), "Poise");
				SendSettingsToHudImpl("fShieldBlockedPhysicalMultiplier");
			}

			auto* weaponBlockedMult = ::menu_exports::get_weaponBlockedPhysicalMultiplier();
			if (ImGuiMCP::SliderFloat("Weapon Blocked Physical Mult##poise", weaponBlockedMult, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fWeaponBlockedPhysicalMultiplier", FloatToString(*weaponBlockedMult), "Poise");
				SendSettingsToHudImpl("fWeaponBlockedPhysicalMultiplier");
			}

			ImGuiMCP::Separator();

			auto* regenDelaySeconds = ::menu_exports::get_regenDelaySeconds();
			if (ImGuiMCP::SliderFloat("Regen Delay (seconds)##poise", regenDelaySeconds, 0.0f, 15.0f, "%.1f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fRegenDelaySeconds", FloatToString(*regenDelaySeconds), "Poise");
				SendSettingsToHudImpl("fRegenDelaySeconds");
			}

			auto* regenRatePerSecond = ::menu_exports::get_regenRatePerSecond();
			if (ImGuiMCP::SliderFloat("Regen Rate/Second##poise", regenRatePerSecond, 1.0f, 50.0f, "%.0f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fRegenRatePerSecond", FloatToString(*regenRatePerSecond), "Poise");
				SendSettingsToHudImpl("fRegenRatePerSecond");
			}

			auto* staggerImmunitySeconds = ::menu_exports::get_staggerImmunitySeconds();
			if (ImGuiMCP::SliderFloat("Stagger Immunity (seconds)##poise", staggerImmunitySeconds, 0.0f, 5.0f, "%.1f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fStaggerImmunitySeconds", FloatToString(*staggerImmunitySeconds), "Poise");
				SendSettingsToHudImpl("fStaggerImmunitySeconds");
			}

			auto* wardMagicMult = ::menu_exports::get_wardMagicDamageMultiplier();
			if (ImGuiMCP::SliderFloat("Ward Magic Damage Mult##poise", wardMagicMult, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fWardMagicDamageMultiplier", FloatToString(*wardMagicMult), "Poise");
				SendSettingsToHudImpl("fWardMagicDamageMultiplier");
			}

			auto* clothingMagicPoise = ::menu_exports::get_clothingMagicPoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Clothing Magic Poise Mult##poise", clothingMagicPoise, 0.0f, 3.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fClothingMagicPoiseMultiplier", FloatToString(*clothingMagicPoise), "Poise");
				SendSettingsToHudImpl("fClothingMagicPoiseMultiplier");
			}

			auto* lightArmorMagicPoise = ::menu_exports::get_lightArmorMagicPoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Light Armor Magic Poise Mult##poise", lightArmorMagicPoise, 0.0f, 3.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fLightArmorMagicPoiseMultiplier", FloatToString(*lightArmorMagicPoise), "Poise");
				SendSettingsToHudImpl("fLightArmorMagicPoiseMultiplier");
			}

			auto* heavyArmorMagicPoise = ::menu_exports::get_heavyArmorMagicPoiseMultiplier();
			if (ImGuiMCP::SliderFloat("Heavy Armor Magic Poise Mult##poise", heavyArmorMagicPoise, 0.0f, 3.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fHeavyArmorMagicPoiseMultiplier", FloatToString(*heavyArmorMagicPoise), "Poise");
				SendSettingsToHudImpl("fHeavyArmorMagicPoiseMultiplier");
			}

			auto* shieldBlockedMagic = ::menu_exports::get_shieldBlockedMagicMultiplier();
			if (ImGuiMCP::SliderFloat("Shield Blocked Magic Mult##poise", shieldBlockedMagic, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fShieldBlockedMagicMultiplier", FloatToString(*shieldBlockedMagic), "Poise");
				SendSettingsToHudImpl("fShieldBlockedMagicMultiplier");
			}

			auto* weaponBlockedMagic = ::menu_exports::get_weaponBlockedMagicMultiplier();
			if (ImGuiMCP::SliderFloat("Weapon Blocked Magic Mult##poise", weaponBlockedMagic, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fWeaponBlockedMagicMultiplier", FloatToString(*weaponBlockedMagic), "Poise");
				SendSettingsToHudImpl("fWeaponBlockedMagicMultiplier");
			}

			auto* concentrationDealsPoise = ::menu_exports::get_concentrationSpellsDealPoise();
			if (ImGuiMCP::Checkbox("Concentration Spells Deal Poise##poise", concentrationDealsPoise)) {
				QueueIniWrite("bConcentrationSpellsDealPoise", *concentrationDealsPoise ? "true" : "false", "Magic Poise");
				SendSettingsToHudImpl("bConcentrationSpellsDealPoise");
			}
		}

		// ===== SECTION 6: Poise Status Debuffs =====
		if (ImGuiMCP::CollapsingHeader("Poise Status Debuffs")) {
			auto* enablePoiseDebuffs = ::menu_exports::get_enablePoiseDebuffs();
			if (ImGuiMCP::Checkbox("Enable Poise Status Debuffs##debuffs", enablePoiseDebuffs)) {
				const char* val = *enablePoiseDebuffs ? "true" : "false";
				QueueIniWrite("bEnablePoiseDebuffs", val, "Poise Debuffs");
				SendSettingsToHudImpl("bEnablePoiseDebuffs");
			}

			ImGuiMCP::Separator();

			auto* enableSapped = ::menu_exports::get_enableSappedDebuff();
			if (ImGuiMCP::Checkbox("Sapped (absorb stops recovery)##debuffs", enableSapped)) {
				QueueIniWrite("bEnableSappedDebuff", *enableSapped ? "true" : "false", "Poise Debuffs");
				SendSettingsToHudImpl("bEnableSappedDebuff");
			}

			auto* enableEnfeebled = ::menu_exports::get_enableEnfeebledDebuff();
			if (ImGuiMCP::Checkbox("Enfeebled (poison caps recovery)##debuffs", enableEnfeebled)) {
				QueueIniWrite("bEnableEnfeebledDebuff", *enableEnfeebled ? "true" : "false", "Poise Debuffs");
				SendSettingsToHudImpl("bEnableEnfeebledDebuff");
			}

			auto* enableNumbed = ::menu_exports::get_enableNumbedDebuff();
			if (ImGuiMCP::Checkbox("Numbed (frost delays recovery)##debuffs", enableNumbed)) {
				QueueIniWrite("bEnableNumbedDebuff", *enableNumbed ? "true" : "false", "Poise Debuffs");
				SendSettingsToHudImpl("bEnableNumbedDebuff");
			}

			auto* enableHemorrhaging = ::menu_exports::get_enableHemorrhagingDebuff();
			if (ImGuiMCP::Checkbox("Hemorrhaging (bleeding slows recovery)##debuffs", enableHemorrhaging)) {
				QueueIniWrite("bEnableHemorrhagingDebuff", *enableHemorrhaging ? "true" : "false", "Poise Debuffs");
				SendSettingsToHudImpl("bEnableHemorrhagingDebuff");
			}

			auto* enableScorched = ::menu_exports::get_enableScorchedDebuff();
			if (ImGuiMCP::Checkbox("Scorched (burning raises melee poise damage)##debuffs", enableScorched)) {
				QueueIniWrite("bEnableScorchedDebuff", *enableScorched ? "true" : "false", "Poise Debuffs");
				SendSettingsToHudImpl("bEnableScorchedDebuff");
			}

			ImGuiMCP::Separator();

			auto* ceilingMax = ::menu_exports::get_enfeebledRegenCeilingMax();
			if (ImGuiMCP::SliderFloat("Enfeebled Ceiling (weakest poison)##debuffs", ceilingMax, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fEnfeebledRegenCeilingMax", FloatToString(*ceilingMax), "Poise Debuffs");
				SendSettingsToHudImpl("fEnfeebledRegenCeilingMax");
			}

			auto* ceilingMin = ::menu_exports::get_enfeebledRegenCeilingMin();
			if (ImGuiMCP::SliderFloat("Enfeebled Ceiling (strongest poison)##debuffs", ceilingMin, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fEnfeebledRegenCeilingMin", FloatToString(*ceilingMin), "Poise Debuffs");
				SendSettingsToHudImpl("fEnfeebledRegenCeilingMin");
			}

			auto* magnitudeForMin = ::menu_exports::get_enfeebledMagnitudeForMin();
			if (ImGuiMCP::SliderFloat("Poison Magnitude For Min Ceiling##debuffs", magnitudeForMin, 0.1f, 100.0f, "%.1f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fEnfeebledMagnitudeForMin", FloatToString(*magnitudeForMin), "Poise Debuffs");
				SendSettingsToHudImpl("fEnfeebledMagnitudeForMin");
			}

			auto* minGlobalCeiling = ::menu_exports::get_minGlobalRegenCeiling();
			if (ImGuiMCP::SliderFloat("Global Recovery Ceiling Floor##debuffs", minGlobalCeiling, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fMinGlobalRegenCeiling", FloatToString(*minGlobalCeiling), "Poise Debuffs");
				SendSettingsToHudImpl("fMinGlobalRegenCeiling");
			}

			auto* numbedDelay = ::menu_exports::get_numbedRegenDelayBonus();
			if (ImGuiMCP::SliderFloat("Numbed Regen Delay Bonus (s)##debuffs", numbedDelay, 0.0f, 10.0f, "%.1f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fNumbedRegenDelayBonus", FloatToString(*numbedDelay), "Poise Debuffs");
				SendSettingsToHudImpl("fNumbedRegenDelayBonus");
			}

			auto* hemorrhagingRate = ::menu_exports::get_hemorrhagingRegenRateMultiplier();
			if (ImGuiMCP::SliderFloat("Hemorrhaging Regen Multiplier##debuffs", hemorrhagingRate, 0.0f, 1.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fHemorrhagingRegenRateMultiplier", FloatToString(*hemorrhagingRate), "Poise Debuffs");
				SendSettingsToHudImpl("fHemorrhagingRegenRateMultiplier");
			}

			auto* scorchedMelee = ::menu_exports::get_scorchedMeleeDamageMultiplier();
			if (ImGuiMCP::SliderFloat("Scorched Melee Damage Multiplier##debuffs", scorchedMelee, 0.5f, 3.0f, "%.2f")) {
				g_iniDirty = true;
				g_lastSliderChangeTime = std::chrono::steady_clock::now();
				QueueIniWrite("fScorchedMeleeDamageMultiplier", FloatToString(*scorchedMelee), "Poise Debuffs");
				SendSettingsToHudImpl("fScorchedMeleeDamageMultiplier");
			}
		}

		// ===== SLIDER PERSISTENCE: Debounce INI Writes =====
		// Flush pending writes if mouse button released OR debounce time elapsed
		if (g_iniDirty) {
			const auto now = std::chrono::steady_clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastSliderChangeTime).count();
			
			// Check if left mouse button is released or debounce time has passed
			bool mouseReleased = !ImGuiMCP::IsMouseDown(0);
			bool debounceExpired = elapsed >= kSliderDebounceMs;
			
			if (mouseReleased || debounceExpired) {
				FlushIniWrites();
				g_iniDirty = false;
				logger::debug("Slider changes flushed to INI (mouse released: {}, debounce expired: {})", mouseReleased, debounceExpired);
			}
		}

		// ===== RESET BUTTONS =====
		ImGuiMCP::Separator();
		ImGuiMCP::SeparatorText("Reset to Defaults");

		const char* hudResetLabel = g_hudResetConfirm ? "CONFIRM Reset HUD?" : "Reset HUD Defaults";
		if (ImGuiMCP::Button(hudResetLabel, ImGuiMCP::ImVec2(-1, 0))) {
			ResetHudDefaults();
		}

		const char* poiseResetLabel = g_poiseResetConfirm ? "CONFIRM Reset Poise?" : "Reset Poise Defaults";
		if (ImGuiMCP::Button(poiseResetLabel, ImGuiMCP::ImVec2(-1, 0))) {
			ResetPoiseDefaults();
		}
	}

	bool IsAvailable()
	{
		return SKSEMenuFramework::IsInstalled();
	}

	void Initialize()
	{
		// Check if SKSE Menu Framework DLL is available
		g_frameworkAvailable = IsAvailable();

		if (!g_frameworkAvailable) {
			logger::info("SKSE Menu Framework not installed; menu will be unavailable.");
			return;
		}

		logger::info("SKSE Menu Framework is available; initializing menu integration.");

		SKSEMenuFramework::Model::Internal::key = "TruePoise";

		// Register the main menu section
		try {
			RegisterMenuSection("Settings", OnMenuRender);
			logger::info("SKSE Menu Framework initialized successfully.");
		} catch (const std::exception& e) {
			logger::error("Failed to initialize SKSE Menu Framework: {}", e.what());
			g_frameworkAvailable = false;
		}
	}

	void Shutdown()
	{
		if (g_menuEventId >= 0) {
			UnregisterMenuEvent(g_menuEventId);
			g_menuEventId = -1;
		}
		// Flush any pending INI writes on shutdown
		FlushIniWrites();
		logger::info("SKSE Menu Framework shutdown complete.");
	}

	void RegisterMenuSection(const std::string& path, SKSEMenuFramework::Model::RenderFunction renderFn)
	{
		if (!g_frameworkAvailable) {
			return;
		}

		try {
			SKSEMenuFramework::AddSectionItem(path, renderFn);
			logger::debug("Registered menu section: {}", path);
		} catch (const std::exception& e) {
			logger::error("Failed to register menu section '{}': {}", path, e.what());
		}
	}

	int64_t RegisterMenuEvent(SKSEMenuFramework::Model::EventCallback callback)
	{
		if (!g_frameworkAvailable) {
			return -1;
		}

		try {
			auto eventPtr = SKSEMenuFramework::AddEvent(callback, 0.0f);
			if (eventPtr) {
				logger::debug("Menu event registered successfully.");
				return 0;
			}
		} catch (const std::exception& e) {
			logger::error("Failed to register menu event: {}", e.what());
		}

		return -1;
	}

	void UnregisterMenuEvent(int64_t eventId)
	{
		if (!g_frameworkAvailable || eventId < 0) {
			return;
		}

		logger::debug("Menu event unregistered.");
	}

	// ========== INI Persistence & Debouncing ==========

	std::filesystem::path GetIniPath()
	{
		wchar_t executablePath[32768]{};
		const DWORD length = GetModuleFileNameW(nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
		if (length == 0 || length >= std::size(executablePath)) {
			return {};
		}
		return std::filesystem::path(executablePath).parent_path() / L"Data" / L"SKSE" / L"Plugins" / L"TruePoise.ini";
	}

	std::string ReadIniValue(const std::string& key, const std::string& section)
	{
		auto iniPath = GetIniPath();
		if (iniPath.empty() || GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			return {};
		}

		std::wstring wkey(key.begin(), key.end());
		std::wstring wsection(section.begin(), section.end());
		wchar_t buffer[512]{};
		
		GetPrivateProfileStringW(wsection.c_str(), wkey.c_str(), L"", buffer, 512, iniPath.c_str());
		
		std::string result(buffer, buffer + wcslen(buffer));
		const auto begin = result.find_first_not_of(" \t");
		const auto end = result.find_last_not_of(" \t");
		return begin == std::string::npos ? std::string{} : result.substr(begin, end - begin + 1);
	}

	void QueueIniWrite(const std::string& key, const std::string& value, const std::string& section)
	{
		g_pendingIniWrites[section + "/" + key] = value;
		if (g_pendingIniWrites.size() == 1) {
			g_lastIniWriteTime = std::chrono::steady_clock::now();
		}
	}

	void FlushIniWrites()
	{
		if (g_pendingIniWrites.empty()) {
			return;
		}

		auto iniPath = GetIniPath();
		if (iniPath.empty() || GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			logger::warn("Cannot write to TruePoise.ini (file not accessible)");
			return;
		}

		for (const auto& [fullKey, value] : g_pendingIniWrites) {
			const auto slashPos = fullKey.find('/');
			const auto section = fullKey.substr(0, slashPos);
			const auto key = fullKey.substr(slashPos + 1);

			std::wstring wsection(section.begin(), section.end());
			std::wstring wkey(key.begin(), key.end());
			std::wstring wvalue(value.begin(), value.end());

			if (!WritePrivateProfileStringW(wsection.c_str(), wkey.c_str(), wvalue.c_str(), iniPath.c_str())) {
				logger::warn("Failed to write {}={} to section [{}]", key, value, section);
			}
		}

		const auto count = g_pendingIniWrites.size();
		g_pendingIniWrites.clear();
		logger::debug("Flushed {} INI writes", count);
	}

	void UpdatePendingWrites()
	{
		if (g_pendingIniWrites.empty()) {
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastIniWriteTime).count();

		if (elapsed >= kIniWriteDebounceMs) {
			FlushIniWrites();
		}
	}

	void ResetHudDefaults()
	{
		const auto now = std::chrono::steady_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_hudResetTime).count();

		if (g_hudResetConfirm && elapsed < kResetConfirmWindowMs) {
			// Second click within window - execute reset
			logger::info("Resetting HUD defaults to factory settings");
			
			// Reset all HUD settings to defaults in memory
			// (externally defined in main.cpp - will be read during reload)
			QueueIniWrite("bEnableCustomHud", "true", "HUD");
			QueueIniWrite("bEnableWeatherWidget", "true", "HUD");
			QueueIniWrite("bEnableSneakLightGem", "true", "HUD");
			QueueIniWrite("sHudTheme", "Daedric Iron", "HUD");
			QueueIniWrite("fHudScale", "1.00", "HUD");
			QueueIniWrite("iSideOffsetLeft", "20", "HUD");
			QueueIniWrite("iBottomOffset", "16", "HUD");
			QueueIniWrite("bShowPoiseBar", "true", "HUD");
			QueueIniWrite("bEnablePoiseShield", "true", "HUD");
			QueueIniWrite("iPoiseShieldOffsetX", "0", "HUD");
			QueueIniWrite("iPoiseShieldOffsetY", "0", "HUD");
			QueueIniWrite("bShowPoiseNumbers", "false", "HUD");
			QueueIniWrite("bShowTargetPoiseBar", "false", "HUD");
			QueueIniWrite("bShowPoiseTelemetry", "false", "HUD");
			QueueIniWrite("bShowPoiseDebuffIcons", "true", "HUD");
			QueueIniWrite("bShowReactiveBarEffects", "true", "HUD");
			QueueIniWrite("bDynamicHUD", "true", "HUD");
			QueueIniWrite("bAlwaysOn", "false", "HUD");
			QueueIniWrite("fIdleOpacity", "0.0", "HUD");
			QueueIniWrite("fFadeDelaySeconds", "3.0", "HUD");
			QueueIniWrite("bMinBarFiligree", "true", "MinimalistGraphics");
			QueueIniWrite("fMinBarShine", "1.0", "MinimalistGraphics");
			QueueIniWrite("fMinBarShadow", "1.0", "MinimalistGraphics");
			QueueIniWrite("bMinBarNotches", "false", "MinimalistGraphics");
			
			FlushIniWrites();
			SendSettingsToHudImpl("resetHud");
			
			g_hudResetConfirm = false;
		} else {
			// First click - arm the confirmation
			g_hudResetConfirm = true;
			g_hudResetTime = now;
			logger::info("HUD reset armed - click again within 3 seconds to confirm");
		}
	}

	void ResetPoiseDefaults()
	{
		const auto now = std::chrono::steady_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_poiseResetTime).count();

		if (g_poiseResetConfirm && elapsed < kResetConfirmWindowMs) {
			// Second click within window - execute reset
			logger::info("Resetting Poise defaults to factory settings");
			
			QueueIniWrite("bEnableBuiltInPoise", "true", "Poise");
			QueueIniWrite("bEnableNPCPoise", "true", "Poise");
			QueueIniWrite("bSuppressVanillaStaggers", "true", "Poise");
			QueueIniWrite("bEnableHitPushBack", "false", "Poise");
			QueueIniWrite("bAllowTimedBashStaggers", "false", "Poise");
			QueueIniWrite("bEnableCreaturePoise", "true", "Poise");
			QueueIniWrite("fLightCreaturePoiseMultiplier", "0.75", "Poise");
			QueueIniWrite("fMediumCreaturePoiseMultiplier", "1.7", "Poise");
			QueueIniWrite("fHeavyCreaturePoiseMultiplier", "3.75", "Poise");
			QueueIniWrite("fBossCreaturePoiseMultiplier", "6.0", "Poise");
			QueueIniWrite("fBossCreatureRegenMultiplier", "1.5", "Poise");
			QueueIniWrite("fLightCreatureDamageMultiplier", "0.75", "Poise");
			QueueIniWrite("fMediumCreatureDamageMultiplier", "1.25", "Poise");
			QueueIniWrite("fHeavyCreatureDamageMultiplier", "1.65", "Poise");
			QueueIniWrite("fBossCreatureDamageMultiplier", "2.40", "Poise");
			QueueIniWrite("fCreatureStaggerImmunitySeconds", "2.0", "Poise");
			QueueIniWrite("fSummonPoiseMultiplier", "1.5", "Poise");
			QueueIniWrite("fNPCMagicPoiseMultiplier", "0.6", "Poise");
			QueueIniWrite("fBasePoise", "40.0", "Poise");
			QueueIniWrite("fArmorWeightMultiplier", "1.0", "Poise");
			QueueIniWrite("fShieldWeightPoiseMultiplier", "0.0", "Poise");
			QueueIniWrite("bApplyShieldWeightToNPCs", "true", "Poise");
			QueueIniWrite("fBaseHitPoiseDamage", "6.0", "Poise");
			QueueIniWrite("fCreatureBaseHitPoiseDamage", "5.5", "Poise");
			QueueIniWrite("fWeaponWeightDamageMultiplier", "0.35", "Poise");
			QueueIniWrite("fTwoHandedMultiplier", "1.15", "Poise");
			QueueIniWrite("fShieldBashMultiplier", "1.0", "Poise");
			QueueIniWrite("fWeaponBashMultiplier", "0.35", "Poise");
			QueueIniWrite("fProjectileMultiplier", "0.65", "Poise");
			QueueIniWrite("fPowerAttackMultiplier", "1.5", "Poise");
			QueueIniWrite("fShieldBlockedPhysicalMultiplier", "0.25", "Poise");
			QueueIniWrite("fWeaponBlockedPhysicalMultiplier", "0.45", "Poise");
			QueueIniWrite("fRegenDelaySeconds", "3.0", "Poise");
			QueueIniWrite("fRegenRatePerSecond", "8.0", "Poise");
			QueueIniWrite("fStaggerImmunitySeconds", "1.0", "Poise");
			QueueIniWrite("fPhysicalDamagePoiseMultiplier", "0.05", "Poise");
			QueueIniWrite("fPhysicalDamagePoiseCap", "6.0", "Poise");
			QueueIniWrite("fLightArmorRatingPoiseMultiplier", "0.15", "Poise");
			QueueIniWrite("fLightArmorRatingPoiseCap", "15.0", "Poise");
			QueueIniWrite("fLightArmorFleshMultiplier", "0.75", "Poise");
			QueueIniWrite("fHeavyArmorFleshMultiplier", "0.45", "Poise");
			QueueIniWrite("fFleshSpellPoiseMultiplier", "0.45", "Poise");
			QueueIniWrite("fDragonhidePoiseBonus", "60.0", "Poise");
			QueueIniWrite("fMagicBurstPoiseMultiplier", "1.2", "Poise");
			QueueIniWrite("fClothingMagicPoiseMultiplier", "0.70", "Poise");
			QueueIniWrite("fLightArmorMagicPoiseMultiplier", "1.00", "Poise");
			QueueIniWrite("fHeavyArmorMagicPoiseMultiplier", "1.15", "Poise");
			QueueIniWrite("fMagicStaggerPoiseMultiplier", "10.0", "Poise");
			QueueIniWrite("fConcentrationPoiseMultiplier", "1.0", "Poise");
			QueueIniWrite("fConcentrationIntervalSeconds", "1.0", "Poise");
			QueueIniWrite("fConcentrationMaxPoisePerInterval", "15.0", "Poise");
			QueueIniWrite("fWardMagicDamageMultiplier", "0.35", "Poise");
			QueueIniWrite("fShieldBlockedMagicMultiplier", "0.4", "Poise");
			QueueIniWrite("fWeaponBlockedMagicMultiplier", "0.7", "Poise");
			QueueIniWrite("bConcentrationSpellsDealPoise", "false", "Magic Poise");
			QueueIniWrite("bEnablePoiseDebuffs", "true", "Poise Debuffs");
			QueueIniWrite("bEnableSappedDebuff", "true", "Poise Debuffs");
			QueueIniWrite("bEnableEnfeebledDebuff", "true", "Poise Debuffs");
			QueueIniWrite("bEnableNumbedDebuff", "true", "Poise Debuffs");
			QueueIniWrite("bEnableHemorrhagingDebuff", "true", "Poise Debuffs");
			QueueIniWrite("bEnableScorchedDebuff", "true", "Poise Debuffs");
			QueueIniWrite("fEnfeebledRegenCeilingMin", "0.70", "Poise Debuffs");
			QueueIniWrite("fEnfeebledRegenCeilingMax", "0.85", "Poise Debuffs");
			QueueIniWrite("fEnfeebledMagnitudeForMin", "10.0", "Poise Debuffs");
			QueueIniWrite("fMinGlobalRegenCeiling", "0.60", "Poise Debuffs");
			QueueIniWrite("fNumbedRegenDelayBonus", "1.0", "Poise Debuffs");
			QueueIniWrite("fHemorrhagingRegenRateMultiplier", "0.65", "Poise Debuffs");
			QueueIniWrite("sHemorrhagingEffectKeywords", "bleed,hemorrhag", "Poise Debuffs");
			QueueIniWrite("fScorchedMeleeDamageMultiplier", "1.20", "Poise Debuffs");
			
			FlushIniWrites();
			SendSettingsToHudImpl("resetPoise");
			
			g_poiseResetConfirm = false;
		} else {
			// First click - arm the confirmation
			g_poiseResetConfirm = true;
			g_poiseResetTime = now;
			logger::info("Poise reset armed - click again within 3 seconds to confirm");
		}
	}
}
