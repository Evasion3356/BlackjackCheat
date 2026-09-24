#include "Config.h"
#include "Log.h"
#include "LogFallback.h"

#include "..\external\inipp\inipp\inipp.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <string>
#include <exception>

namespace
{
	using Section = inipp::Ini<char>::Section;

	Config::Values g_values;
	bool g_loaded = false;

	// Where BlackjackCheat.ini is loaded from and saved to: next to the .asi, or
	// %LOCALAPPDATA%\RDR2ASIMods\BlackjackCheat.ini when the game folder isn't
	// writable -- starting from the game folder's copy if there is one (see
	// LogFallback::ResolveSettings). Resolved once per session.
	const LogFallback::SettingsPaths& IniPaths()
	{
		static const LogFallback::SettingsPaths paths = LogFallback::ResolveSettings(
			LogFallback::ModuleDirectory(), L"BlackjackCheat.ini", LogFallback::FallbackDirectory());
		return paths;
	}

	template <typename T>
	T GetOr(const Section& sec, const char* key, T def)
	{
		inipp::get_value(sec, key, def);
		return def;
	}

	void SetFloat(Section& sec, const char* key, float value)
	{
		// std::ostringstream's default floatfield (unset -- neither fixed
		// nor scientific) picks whichever representation is shorter at the
		// default 6-significant-digit precision, same behavior "%g" gave --
		// see CLAUDE.md's "No C-style strings/buffers" convention for why
		// this replaced a fixed char[64] + sprintf_s.
		std::ostringstream oss;
		oss << value;
		sec[key] = oss.str();
	}

	void SetBool(Section& sec, const char* key, bool value)
	{
		sec[key] = value ? "true" : "false";
	}

	void ReloadImpl()
	{
		Log::Trace(L"Config: ini path={}", IniPaths().read);

		inipp::Ini<char> ini;
		bool fileRead = false;
		{
			Log::Trace("Config: opening ini for read");
			std::ifstream is(IniPaths().read);
			Log::Trace("Config: ini open for read {}", is ? "succeeded" : "failed (using defaults)");
			if (is)
			{
				fileRead = true;
				ini.parse(is);
				Log::Trace("Config: ini parsed, {} section(s), {} parse error(s)", ini.sections.size(), ini.errors.size());
			}
		}

		// Parsed into a local copy and only committed once every value
		// has been read, so an exception partway through (caught by
		// Reload()) really does keep the previous values intact, as its
		// log line says, instead of leaving them half-updated.
		Config::Values defaults;
		Config::Values loaded;
		{
			auto& general = ini.sections["General"];
			loaded.ShowDealerHand = GetOr(general, "ShowDealerHand", defaults.ShowDealerHand);
			loaded.ShowBettingAdvice = GetOr(general, "ShowBettingAdvice", defaults.ShowBettingAdvice);
			loaded.ShowAdvice = GetOr(general, "ShowAdvice", defaults.ShowAdvice);
			loaded.ShowDeckPrediction = GetOr(general, "ShowDeckPrediction", defaults.ShowDeckPrediction);
			loaded.ShowCardsBeforeBet = GetOr(general, "ShowCardsBeforeBet", defaults.ShowCardsBeforeBet);
			loaded.Language = GetOr(general, "Language", defaults.Language);

#ifdef _DEBUG
			auto& hud = ini.sections["HUD"];
			loaded.PanelX = GetOr(hud, "PanelX", defaults.PanelX);
			loaded.PanelY = GetOr(hud, "PanelY", defaults.PanelY);
			loaded.TextScale = GetOr(hud, "TextScale", defaults.TextScale);
			loaded.TitleTextScale = GetOr(hud, "TitleTextScale", defaults.TitleTextScale);
			loaded.AdviceX = GetOr(hud, "AdviceX", defaults.AdviceX);
			loaded.AdviceY = GetOr(hud, "AdviceY", defaults.AdviceY);
			loaded.HoleCardIconX = GetOr(hud, "HoleCardIconX", defaults.HoleCardIconX);
			loaded.HoleCardIconY = GetOr(hud, "HoleCardIconY", defaults.HoleCardIconY);
			loaded.HoleCardIconWidth = GetOr(hud, "HoleCardIconWidth", defaults.HoleCardIconWidth);
			loaded.HoleCardIconHeight = GetOr(hud, "HoleCardIconHeight", defaults.HoleCardIconHeight);
			loaded.NextCardIconBaseX = GetOr(hud, "NextCardIconBaseX", defaults.NextCardIconBaseX);
			loaded.NextCardIconY = GetOr(hud, "NextCardIconY", defaults.NextCardIconY);
			loaded.NextCardIconSpacingX = GetOr(hud, "NextCardIconSpacingX", defaults.NextCardIconSpacingX);
			loaded.NextCardIconWidth = GetOr(hud, "NextCardIconWidth", defaults.NextCardIconWidth);
			loaded.NextCardIconHeight = GetOr(hud, "NextCardIconHeight", defaults.NextCardIconHeight);
			loaded.MyHandIconX = GetOr(hud, "MyHandIconX", defaults.MyHandIconX);
			loaded.MyHandIconY = GetOr(hud, "MyHandIconY", defaults.MyHandIconY);
			loaded.MyHandIconSpacingX = GetOr(hud, "MyHandIconSpacingX", defaults.MyHandIconSpacingX);
			loaded.MyHandIconWidth = GetOr(hud, "MyHandIconWidth", defaults.MyHandIconWidth);
			loaded.MyHandIconHeight = GetOr(hud, "MyHandIconHeight", defaults.MyHandIconHeight);
#endif
		}

		g_values = loaded;

		// The sections this build owns are rebuilt from scratch rather
		// than reusing the ones just parsed above -- any key not
		// explicitly written back below (a stale leftover from a removed
		// feature, e.g. ShowPlayerHands/ShowInsuranceAdvice/ShowCardCount
		// from before Session 8) is dropped instead of round-tripping
		// forever. Only the toggles that actually do something in
		// Release belong in [General] (Session 8, user request).
		//
		// Code-review fix: every OTHER section is carried over untouched.
		// This used to clear the whole file, so a Release build wiped the
		// Debug-only [HUD] section (a tuned layout, when both builds
		// share one INI) along with anything else it didn't recognize.
		inipp::Ini<char> out;
		out.sections = ini.sections;
		out.sections.erase("General");
#ifdef _DEBUG
		out.sections.erase("HUD");
#endif

		auto& general = out.sections["General"];
		SetBool(general, "ShowDealerHand", g_values.ShowDealerHand);
		SetBool(general, "ShowBettingAdvice", g_values.ShowBettingAdvice);
		SetBool(general, "ShowAdvice", g_values.ShowAdvice);
		SetBool(general, "ShowDeckPrediction", g_values.ShowDeckPrediction);
		SetBool(general, "ShowCardsBeforeBet", g_values.ShowCardsBeforeBet);
		general["Language"] = g_values.Language;

#ifdef _DEBUG
		auto& hud = out.sections["HUD"];
		SetFloat(hud, "PanelX", g_values.PanelX);
		SetFloat(hud, "PanelY", g_values.PanelY);
		SetFloat(hud, "TextScale", g_values.TextScale);
		SetFloat(hud, "TitleTextScale", g_values.TitleTextScale);
		SetFloat(hud, "AdviceX", g_values.AdviceX);
		SetFloat(hud, "AdviceY", g_values.AdviceY);
		SetFloat(hud, "HoleCardIconX", g_values.HoleCardIconX);
		SetFloat(hud, "HoleCardIconY", g_values.HoleCardIconY);
		SetFloat(hud, "HoleCardIconWidth", g_values.HoleCardIconWidth);
		SetFloat(hud, "HoleCardIconHeight", g_values.HoleCardIconHeight);
		SetFloat(hud, "NextCardIconBaseX", g_values.NextCardIconBaseX);
		SetFloat(hud, "NextCardIconY", g_values.NextCardIconY);
		SetFloat(hud, "NextCardIconSpacingX", g_values.NextCardIconSpacingX);
		SetFloat(hud, "NextCardIconWidth", g_values.NextCardIconWidth);
		SetFloat(hud, "NextCardIconHeight", g_values.NextCardIconHeight);
		SetFloat(hud, "MyHandIconX", g_values.MyHandIconX);
		SetFloat(hud, "MyHandIconY", g_values.MyHandIconY);
		SetFloat(hud, "MyHandIconSpacingX", g_values.MyHandIconSpacingX);
		SetFloat(hud, "MyHandIconWidth", g_values.MyHandIconWidth);
		SetFloat(hud, "MyHandIconHeight", g_values.MyHandIconHeight);
#endif

		// Code-review fix: only rewrite the file when it's missing or its
		// owned sections actually differ from what would be written
		// (a missing key, a stale one to prune, a value to normalize).
		// inipp's generate() can't round-trip comments, so rewriting an
		// already-complete file on every load/Reload Config silently
		// deleted any notes the user had added to it.
		if (fileRead && out.sections == ini.sections)
		{
			Log::Write(L"Config::Reload -- loaded from {}, already up to date (ShowDealerHand={} ShowBettingAdvice={} ShowAdvice={} ShowDeckPrediction={} ShowCardsBeforeBet={})",
				IniPaths().read, g_values.ShowDealerHand, g_values.ShowBettingAdvice, g_values.ShowAdvice, g_values.ShowDeckPrediction, g_values.ShowCardsBeforeBet);
			return;
		}

		if (IniPaths().usedFallback)
			Log::Write("Config::Reload -- the game folder isn't writable, so settings are saved to {}",
				LogFallback::ToUtf8(IniPaths().write));

		{
			Log::Trace("Config: opening ini for write");
			std::ofstream os(IniPaths().write, std::ios::trunc);
			Log::Trace("Config: ini open for write {}", os ? "succeeded" : "failed");
			if (os)
				out.generate(os);
			else
				Log::Write(L"Config::Reload -- failed to open {} for writing", IniPaths().write);
		}

		Log::Write(L"Config::Reload -- loaded from {} (ShowDealerHand={} ShowBettingAdvice={} ShowAdvice={} ShowDeckPrediction={} ShowCardsBeforeBet={})",
			IniPaths().read, g_values.ShowDealerHand, g_values.ShowBettingAdvice, g_values.ShowAdvice, g_values.ShowDeckPrediction, g_values.ShowCardsBeforeBet);
	}
}

namespace Config
{
	void Reload()
	{
		try
		{
			ReloadImpl();
		}
		catch (const std::exception& e)
		{
			Log::Write("Config::Reload -- std::exception: {} -- keeping previous config values", e.what());
		}
		catch (...)
		{
			Log::Write("Config::Reload -- unknown non-std exception -- keeping previous config values");
		}

		g_loaded = true;
	}

	const Values& Get()
	{
		if (!g_loaded)
			Reload();

		return g_values;
	}
}
