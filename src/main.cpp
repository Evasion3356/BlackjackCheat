/*
	Entry point. Registers ScriptMain as a ScriptHookRDR2 script thread and
	wires up the keyboard handler, same pattern as PokerCheat's main.cpp
	(itself matching the ScriptHookRDR2 SDK's NativeTrainer sample).

	Every step below is bracketed by a Log::Trace (Debug-only, see Log.h)
	so a crash/hang during injection shows exactly which step it died in.
*/

#include "..\external\ScriptHookSDK\inc\main.h"
#include "script.h"
#include "keyboard.h"
#include "Log.h"

#ifdef _DEBUG
#include <string>

namespace
{
	std::string GetModulePath(HMODULE module)
	{
		std::string path(MAX_PATH, '\0');
		DWORD len = GetModuleFileNameA(module, path.data(), static_cast<DWORD>(path.size()));
		path.resize(len);
		return path;
	}

	std::string GetWorkingDirectory()
	{
		DWORD len = GetCurrentDirectoryA(0, nullptr);
		if (len == 0)
			return {};
		std::string dir(len, '\0');
		len = GetCurrentDirectoryA(len, dir.data());
		dir.resize(len);
		return dir;
	}

	void TraceProcessContext(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
	{
		Log::Trace("DllMain: reason={} ({}) hInstance=0x{:X} lpReserved=0x{:X} ({}) pid={} tid={}",
			reason == DLL_PROCESS_ATTACH ? "DLL_PROCESS_ATTACH" : "DLL_PROCESS_DETACH", reason,
			reinterpret_cast<std::uintptr_t>(hInstance), reinterpret_cast<std::uintptr_t>(lpReserved),
			reason == DLL_PROCESS_ATTACH
				? (lpReserved ? "static load" : "dynamic LoadLibrary")
				: (lpReserved ? "process terminating" : "FreeLibrary"),
			GetCurrentProcessId(), GetCurrentThreadId());
		Log::Trace("DllMain: asi path={}", GetModulePath(hInstance));
		Log::Trace("DllMain: exe path={} exe base=0x{:X}", GetModulePath(nullptr),
			reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr)));
		Log::Trace("DllMain: working dir={}", GetWorkingDirectory());
		// Null module would make GetModuleFileNameA report the exe's path
		// instead, so only look the path up when it's actually loaded.
		HMODULE scriptHook = GetModuleHandleA("ScriptHookRDR2.dll");
		Log::Trace("DllMain: ScriptHookRDR2.dll base=0x{:X} path={}",
			reinterpret_cast<std::uintptr_t>(scriptHook),
			scriptHook ? GetModulePath(scriptHook) : std::string("<not loaded>"));
	}
}
#endif

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		TraceProcessContext(hInstance, reason, lpReserved);
#endif

		// Nothing but registration here. Config::Reload() (file I/O) and
		// GamePointers::GetScriptThreads() (a scan of RDR2.exe's whole image)
		// used to run here, under the loader lock and -- with an early ASI
		// loader -- possibly before RDR2.exe has finished unpacking (a failed
		// scan was then cached for the session). ScriptMain does both first
		// thing instead (see script.cpp).
		Log::Trace("DllMain: scriptRegister begin");
		scriptRegister(hInstance, ScriptMain);
		Log::Trace("DllMain: scriptRegister done");
#ifdef _DEBUG
		// Release has no menu to drive with keystrokes at all (see
		// script.cpp) -- Debug-only, same as PokerCheat.
		Log::Trace("DllMain: keyboardHandlerRegister begin");
		keyboardHandlerRegister(OnKeyboardMessage);
		Log::Trace("DllMain: keyboardHandlerRegister done");
#endif
		Log::Trace("DllMain: DLL_PROCESS_ATTACH complete");
		break;
	case DLL_PROCESS_DETACH:
#ifdef _DEBUG
		TraceProcessContext(hInstance, reason, lpReserved);
#endif
		Log::Trace("DllMain: scriptUnregister begin");
		scriptUnregister(hInstance);
		Log::Trace("DllMain: scriptUnregister done");
#ifdef _DEBUG
		Log::Trace("DllMain: keyboardHandlerUnregister begin");
		keyboardHandlerUnregister(OnKeyboardMessage);
		Log::Trace("DllMain: keyboardHandlerUnregister done");
#endif
		Log::Trace("DllMain: DLL_PROCESS_DETACH complete");
		break;
	}
	return TRUE;
}
