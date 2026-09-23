#include "Config.h"
#include "Log.h"

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

	// Same technique as PokerCheat's Config.cpp: resolves BlackjackCheat.ini
	// next to this DLL's own .asi via the DLL's own module handle, wide
	// path throughout so there's no narrow/wide conversion anywhere in this
	// file (see PokerCheat's Config.h header comment for why that mattered
	// there -- a real crash source with the mINI library it replaced).
	const std::wstring& ResolveIniPath()
	{
		static const std::wstring path = []() -> std::wstring
		{
			HMODULE hModule = nullptr;
			[[maybe_unused]] BOOL gotModule = GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(&ResolveIniPath),
				&hModule);
			Log::Trace("Config::ResolveIniPath: GetModuleHandleExA ok={} hModule=0x{:X}",
				gotModule != FALSE, reinterpret_cast<std::uintptr_t>(hModule));

			wchar_t modulePath[MAX_PATH] = {};
			GetModuleFileNameW(hModule, modulePath, MAX_PATH);

			wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR];
			_wsplitpath_s(modulePath, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);

			return std::wstring(drive) + dir + L"BlackjackCheat.ini";
		}();

		return path;
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
		Log::Trace(L"Config: ini path={}", ResolveIniPath());

		inipp::Ini<char> ini;
		{
			Log::Trace("Config: opening ini for read");
			std::ifstream is(ResolveIniPath());
			Log::Trace("Config: ini open for read {}", is ? "succeeded" : "failed (using defaults)");
			if (is)
			{
				ini.parse(is);
				Log::Trace("Config: ini parsed, {} section(s), {} parse error(s)", ini.sections.size(), ini.errors.size());
			}
		}

		Config::Values defaults;
		{
			auto& general = ini.sections["General"];
			g_values.ShowDealerHand = GetOr(general, "ShowDealerHand", defaults.ShowDealerHand);
			g_values.ShowBettingAdvice = GetOr(general, "ShowBettingAdvice", defaults.ShowBettingAdvice);
			g_values.ShowAdvice = GetOr(general, "ShowAdvice", defaults.ShowAdvice);
			g_values.ShowDeckPrediction = GetOr(general, "ShowDeckPrediction", defaults.ShowDeckPrediction);
			g_values.ShowCardsBeforeBet = GetOr(general, "ShowCardsBeforeBet", defaults.ShowCardsBeforeBet);
			g_values.Language = GetOr(general, "Language", defaults.Language);

#ifdef _DEBUG
			auto& hud = ini.sections["HUD"];
			g_values.PanelX = GetOr(hud, "PanelX", defaults.PanelX);
			g_values.PanelY = GetOr(hud, "PanelY", defaults.PanelY);
			g_values.TextScale = GetOr(hud, "TextScale", defaults.TextScale);
			g_values.TitleTextScale = GetOr(hud, "TitleTextScale", defaults.TitleTextScale);
			g_values.AdviceX = GetOr(hud, "AdviceX", defaults.AdviceX);
			g_values.AdviceY = GetOr(hud, "AdviceY", defaults.AdviceY);
			g_values.HoleCardIconX = GetOr(hud, "HoleCardIconX", defaults.HoleCardIconX);
			g_values.HoleCardIconY = GetOr(hud, "HoleCardIconY", defaults.HoleCardIconY);
			g_values.HoleCardIconWidth = GetOr(hud, "HoleCardIconWidth", defaults.HoleCardIconWidth);
			g_values.HoleCardIconHeight = GetOr(hud, "HoleCardIconHeight", defaults.HoleCardIconHeight);
			g_values.NextCardIconBaseX = GetOr(hud, "NextCardIconBaseX", defaults.NextCardIconBaseX);
			g_values.NextCardIconY = GetOr(hud, "NextCardIconY", defaults.NextCardIconY);
			g_values.NextCardIconSpacingX = GetOr(hud, "NextCardIconSpacingX", defaults.NextCardIconSpacingX);
			g_values.NextCardIconWidth = GetOr(hud, "NextCardIconWidth", defaults.NextCardIconWidth);
			g_values.NextCardIconHeight = GetOr(hud, "NextCardIconHeight", defaults.NextCardIconHeight);
			g_values.MyHandIconX = GetOr(hud, "MyHandIconX", defaults.MyHandIconX);
			g_values.MyHandIconY = GetOr(hud, "MyHandIconY", defaults.MyHandIconY);
			g_values.MyHandIconSpacingX = GetOr(hud, "MyHandIconSpacingX", defaults.MyHandIconSpacingX);
			g_values.MyHandIconWidth = GetOr(hud, "MyHandIconWidth", defaults.MyHandIconWidth);
			g_values.MyHandIconHeight = GetOr(hud, "MyHandIconHeight", defaults.MyHandIconHeight);
#endif
		}

		// Rebuilt from scratch rather than reusing the sections just
		// parsed above -- any key not explicitly written back below (a
		// stale leftover from a removed feature, e.g. ShowPlayerHands/
		// ShowInsuranceAdvice/ShowCardCount from before Session 8) is
		// dropped instead of round-tripping forever. Only the toggles
		// that actually do something in Release belong in [General]
		// (Session 8, user request).
		ini.sections.clear();

		auto& general = ini.sections["General"];
		SetBool(general, "ShowDealerHand", g_values.ShowDealerHand);
		SetBool(general, "ShowBettingAdvice", g_values.ShowBettingAdvice);
		SetBool(general, "ShowAdvice", g_values.ShowAdvice);
		SetBool(general, "ShowDeckPrediction", g_values.ShowDeckPrediction);
		SetBool(general, "ShowCardsBeforeBet", g_values.ShowCardsBeforeBet);
		general["Language"] = g_values.Language;

#ifdef _DEBUG
		auto& hud = ini.sections["HUD"];
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

		{
			Log::Trace("Config: opening ini for write");
			std::ofstream os(ResolveIniPath(), std::ios::trunc);
			Log::Trace("Config: ini open for write {}", os ? "succeeded" : "failed");
			if (os)
				ini.generate(os);
			else
				Log::Write(L"Config::Reload -- failed to open {} for writing", ResolveIniPath());
		}

		Log::Write(L"Config::Reload -- loaded from {} (ShowDealerHand={} ShowBettingAdvice={} ShowAdvice={} ShowDeckPrediction={} ShowCardsBeforeBet={})",
			ResolveIniPath(), g_values.ShowDealerHand, g_values.ShowBettingAdvice, g_values.ShowAdvice, g_values.ShowDeckPrediction, g_values.ShowCardsBeforeBet);
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
