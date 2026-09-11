/*
	Entry point. Registers ScriptMain as a ScriptHookRDR2 script thread and
	wires up the keyboard handler, same pattern as PokerCheat's main.cpp
	(itself matching the ScriptHookRDR2 SDK's NativeTrainer sample).
*/

#include "..\..\ScriptHookSDK\inc\main.h"
#include "script.h"
#include "keyboard.h"
#include "Config.h"
#include "GamePointers.h"

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		// Plain synchronous call, no worker thread -- see PokerCheat's
		// Config.cpp/Config.h header comment for why an earlier mINI-based
		// version of that file needed one and inipp doesn't.
		Config::Reload();

		// Eagerly resolve the live scrThread pool here too (see
		// PokerCheat's main.cpp for why this belongs in DllMain rather
		// than lazily on the first toggle -- avoids landing the AOB scan's
		// one-time cost on the first "Toggle Blackjack Cheat" press).
		GamePointers::GetScriptThreads();

		scriptRegister(hInstance, ScriptMain);
#ifdef _DEBUG
		// Release has no menu to drive with keystrokes at all (see
		// script.cpp) -- Debug-only, same as PokerCheat.
		keyboardHandlerRegister(OnKeyboardMessage);
#endif
		break;
	case DLL_PROCESS_DETACH:
		scriptUnregister(hInstance);
#ifdef _DEBUG
		keyboardHandlerUnregister(OnKeyboardMessage);
#endif
		break;
	}
	return TRUE;
}
