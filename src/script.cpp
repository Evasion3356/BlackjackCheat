/*
	BlackjackCheat -- ScriptHookRDR2 ASI mod, blackjack advisor HUD for the
	"bjack_sp" single-player minigame. Sibling of PokerCheat, same toolchain
	and conventions (see CLAUDE.md).

	Press F11 in-game to open the test menu (NUMPAD 8/2 to move, NUMPAD 5 to
	select, NUMPAD 0/Backspace/F11 to back out -- same NUMPAD controls as
	PokerCheat/CollectorOffline and the ScriptHookRDR2 SDK's own
	NativeTrainer sample, but F11 instead of their F10 so this mod's menu
	doesn't collide with PokerCheat's own F10 menu when both are loaded).
*/

#include "scriptmenu.h" // pulls in script.h (natives/types/enums/main) and keyboard.h
#include "Log.h"
#include "BlackjackCheat.h"
#include "Config.h"
#include "Localization.h"

namespace
{
#ifdef _DEBUG
	MenuController g_menuController;
	MenuBase* g_mainMenu = nullptr;

	// Re-picks the active HUD language immediately after an ini edit,
	// same live-tuning workflow as every other Config-backed value here
	// -- Localization::Refresh() itself can't just be called from
	// Config::Reload() directly (see Localization.h's header comment):
	// it invokes a real game native, so it must run from inside
	// ScriptHookRDR2's script fiber, same as this menu action already
	// does. Ported from PokerCheat's identical script.cpp helper.
	void ReloadConfigAndLocalization()
	{
		Config::Reload();
		Localization::Refresh();
	}

	void BuildMenu()
	{
		g_mainMenu = new MenuBase(new MenuItemTitle("BlackjackCheat"));
		g_mainMenu->AddItem(new MenuItemAction("Toggle Blackjack Cheat (see log)", BlackjackCheat::Toggle));
		g_mainMenu->AddItem(new MenuItemAction("Dump Local Stack Range (see log)", BlackjackCheat::DumpLocalStackRange));
		g_mainMenu->AddItem(new MenuItemAction("Probe Table Struct (see log)", BlackjackCheat::ProbeTableStruct));
		g_mainMenu->AddItem(new MenuItemAction("Probe Seat Hands (see log)", BlackjackCheat::ProbeSeatHands));
		g_mainMenu->AddItem(new MenuItemAction("Probe Deck Prediction (see log)", BlackjackCheat::ProbeDeckPrediction));
		g_mainMenu->AddItem(new MenuItemAction("Dump Full Stack JSONL", BlackjackCheat::DumpFullStackJsonl));
		g_mainMenu->AddItem(new MenuItemAction("Reload Config (see log)", ReloadConfigAndLocalization));
		g_menuController.RegisterMenu(g_mainMenu);
	}
#endif
}

void ScriptMain()
{
	Log::Write("BlackjackCheat started");
	Log::Trace("ScriptMain: entered on script fiber, tid={}", GetCurrentThreadId());

	// Config is loaded from DllMain, not here -- see main.cpp.

#ifdef _DEBUG
	// Debug-only -- the F11 test menu (toggle, probes) is a dev-tuning
	// surface, not something an end user should ever see. Release has no
	// menu to toggle the cheat from at all, so it enables itself
	// unconditionally below instead. Unlike PokerCheat, Release enabling
	// itself here is arguably MORE justified to stay Debug-gated given how
	// unconfirmed the struct offsets still are (see BlackjackCheat.cpp's
	// file header comment) -- kept symmetric with PokerCheat's convention
	// anyway since a wrong-offset read just shows garbage/nothing on
	// screen, not a crash (ReadScriptLocal bounds-checks against the
	// thread's real stack size).
	Log::Trace("ScriptMain: BuildMenu begin");
	BuildMenu();
	Log::Trace("ScriptMain: BuildMenu done");
#else
	// SetEnabled(true), not Toggle() -- idempotent against ScriptMain
	// ever being re-entered (see BlackjackCheat.h's SetEnabled comment).
	BlackjackCheat::SetEnabled(true);
#endif

	Log::Trace("ScriptMain: entering tick loop");
#ifdef _DEBUG
	bool firstTickTraced = false;
#endif

	while (true)
	{
#ifdef _DEBUG
		if (!firstTickTraced)
			Log::Trace("ScriptMain: first tick begin");

		if (!g_menuController.HasActiveMenu() && MenuInput::MenuSwitchPressed())
			g_menuController.PushMenu(g_mainMenu);

		g_menuController.Update();
#endif
		BlackjackCheat::OnTick();

#ifdef _DEBUG
		if (!firstTickTraced)
		{
			Log::Trace("ScriptMain: first tick OnTick done, yielding");
			firstTickTraced = true;
		}
#endif

		WAIT(0);
	}
}
