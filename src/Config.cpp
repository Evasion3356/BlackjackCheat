#include "Config.h"
#include "Log.h"

#include "..\external\inipp\inipp\inipp.h"

#include <windows.h>
#include <fstream>
#include <cstdio>
#include <cstdlib>
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
			GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(&ResolveIniPath),
				&hModule);

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
		char buf[64];
		sprintf_s(buf, "%g", value);
		sec[key] = buf;
	}

	void SetBool(Section& sec, const char* key, bool value)
	{
		sec[key] = value ? "true" : "false";
	}

	void ReloadImpl()
	{
		inipp::Ini<char> ini;
		{
			std::ifstream is(ResolveIniPath());
			if (is)
				ini.parse(is);
		}

		Config::Values defaults;
		auto& general = ini.sections["General"];

		g_values.ShowDealerHand = GetOr(general, "ShowDealerHand", defaults.ShowDealerHand);
		g_values.ShowPlayerHands = GetOr(general, "ShowPlayerHands", defaults.ShowPlayerHands);
		g_values.ShowAdvice = GetOr(general, "ShowAdvice", defaults.ShowAdvice);
		g_values.ShowInsuranceAdvice = GetOr(general, "ShowInsuranceAdvice", defaults.ShowInsuranceAdvice);
		g_values.ShowDeckPrediction = GetOr(general, "ShowDeckPrediction", defaults.ShowDeckPrediction);

#ifdef _DEBUG
		auto& hud = ini.sections["HUD"];
		g_values.PanelX = GetOr(hud, "PanelX", defaults.PanelX);
		g_values.PanelY = GetOr(hud, "PanelY", defaults.PanelY);
		g_values.TextScale = GetOr(hud, "TextScale", defaults.TextScale);
		g_values.TitleTextScale = GetOr(hud, "TitleTextScale", defaults.TitleTextScale);
		g_values.AdviceX = GetOr(hud, "AdviceX", defaults.AdviceX);
		g_values.AdviceY = GetOr(hud, "AdviceY", defaults.AdviceY);
		SetFloat(hud, "PanelX", g_values.PanelX);
		SetFloat(hud, "PanelY", g_values.PanelY);
		SetFloat(hud, "TextScale", g_values.TextScale);
		SetFloat(hud, "TitleTextScale", g_values.TitleTextScale);
		SetFloat(hud, "AdviceX", g_values.AdviceX);
		SetFloat(hud, "AdviceY", g_values.AdviceY);
#endif

		SetBool(general, "ShowDealerHand", g_values.ShowDealerHand);
		SetBool(general, "ShowPlayerHands", g_values.ShowPlayerHands);
		SetBool(general, "ShowAdvice", g_values.ShowAdvice);
		SetBool(general, "ShowInsuranceAdvice", g_values.ShowInsuranceAdvice);
		SetBool(general, "ShowDeckPrediction", g_values.ShowDeckPrediction);

		{
			std::ofstream os(ResolveIniPath(), std::ios::trunc);
			if (os)
				ini.generate(os);
			else
				Log::Write("Config::Reload -- failed to open %ls for writing", ResolveIniPath().c_str());
		}

		Log::Write("Config::Reload -- loaded from %ls (ShowDealerHand=%d ShowPlayerHands=%d ShowAdvice=%d)",
			ResolveIniPath().c_str(), g_values.ShowDealerHand, g_values.ShowPlayerHands, g_values.ShowAdvice);
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
			Log::Write("Config::Reload -- std::exception: %s -- keeping previous config values", e.what());
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
