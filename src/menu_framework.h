#pragma once

#include "SKSEMenuFramework.h"
#include <string>
#include <functional>
#include <filesystem>

namespace MenuFramework
{
	/**
	 * @brief Checks if SKSE Menu Framework is installed and available.
	 * @return true if framework is installed, false otherwise.
	 */
	bool IsAvailable();

	/**
	 * @brief Initialize the SKSE Menu Framework integration.
	 * Registers the mod menu section and sets up ImGui rendering.
	 * Safe to call even if framework is not installed.
	 */
	void Initialize();

	/**
	 * @brief Shutdown the SKSE Menu Framework integration.
	 * Should be called during plugin unload.
	 */
	void Shutdown();

	/**
	 * @brief Register a menu item/section with SKSE Menu Framework.
	 * @param path The menu path (e.g., "Layout & Widgets", "Poise & Combat")
	 * @param renderFn The ImGui render callback for this menu section
	 */
	void RegisterMenuSection(const std::string& path, SKSEMenuFramework::Model::RenderFunction renderFn);

	/**
	 * @brief Set a hook to be called when menu opens/closes.
	 * @param callback Function to call with event type (kOpenMenu, kCloseMenu, etc.)
	 * @return Event ID for later removal if needed
	 */
	int64_t RegisterMenuEvent(SKSEMenuFramework::Model::EventCallback callback);

	/**
	 * @brief Remove a previously registered menu event.
	 * @param eventId The event ID returned from RegisterMenuEvent
	 */
	void UnregisterMenuEvent(int64_t eventId);

	// ========== INI Persistence & Debouncing ==========

	/**
	 * @brief Get the path to TruePoise.ini
	 */
	std::filesystem::path GetIniPath();

	/**
	 * @brief Read a single value from the INI file.
	 * @param key The setting key (e.g., "fBasePoise")
	 * @param section The INI section (default: "HUD")
	 * @return The value as a string, empty if not found
	 */
	std::string ReadIniValue(const std::string& key, const std::string& section = "HUD");

	/**
	 * @brief Queue an INI write (debounced).
	 * Writes are batched and flushed every 500ms to prevent disk spam.
	 * @param key The setting key
	 * @param value The value to write
	 * @param section The INI section (default: "HUD")
	 */
	void QueueIniWrite(const std::string& key, const std::string& value, const std::string& section = "HUD");

	/**
	 * @brief Immediately flush all pending INI writes to disk.
	 */
	void FlushIniWrites();

	/**
	 * @brief Check pending writes and flush if debounce time has elapsed.
	 * Call this once per frame (or periodically) from main update loop.
	 */
	void UpdatePendingWrites();
}
