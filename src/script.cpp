/*
	BlackjackCheat -- ScriptHookRDR2 ASI mod, blackjack advisor HUD for the
	"bjack_sp" single-player minigame. Sibling of PokerCheat, same toolchain
	and conventions (see CLAUDE.md).

	Press F10 in-game to open the test menu (NUMPAD 8/2 to move, NUMPAD 5 to
	select, NUMPAD 0/Backspace/F10 to back out -- same controls as
	PokerCheat/CollectorOffline and the ScriptHookRDR2 SDK's own
	NativeTrainer sample).
*/

#include "scriptmenu.h" // pulls in script.h (natives/types/enums/main) and keyboard.h
#include "Log.h"
#include "BlackjackCheat.h"
#include "Config.h"

namespace
{
#ifdef _DEBUG
	MenuController g_menuController;
	MenuBase* g_mainMenu = nullptr;

	void BuildMenu()
	{
		g_mainMenu = new MenuBase(new MenuItemTitle("BlackjackCheat"));
		g_mainMenu->AddItem(new MenuItemAction("Toggle Blackjack Cheat (see log)", BlackjackCheat::Toggle));
		g_mainMenu->AddItem(new MenuItemAction("Dump Local Stack Range (see log)", BlackjackCheat::DumpLocalStackRange));
		g_mainMenu->AddItem(new MenuItemAction("Probe Table Struct (see log)", BlackjackCheat::ProbeTableStruct));
		g_mainMenu->AddItem(new MenuItemAction("Probe Seat Hands (see log)", BlackjackCheat::ProbeSeatHands));
		g_mainMenu->AddItem(new MenuItemAction("Probe Deck Prediction (see log)", BlackjackCheat::ProbeDeckPrediction));
		g_mainMenu->AddItem(new MenuItemAction("Dump Full Stack JSONL", BlackjackCheat::DumpFullStackJsonl));
		g_mainMenu->AddItem(new MenuItemAction("Reload Config (see log)", Config::Reload));
		g_menuController.RegisterMenu(g_mainMenu);
	}
#endif
}

void ScriptMain()
{
	Log::Write("BlackjackCheat started");

	// Config is loaded from DllMain, not here -- see main.cpp.

#ifdef _DEBUG
	// Debug-only -- the F10 test menu (toggle, probes) is a dev-tuning
	// surface, not something an end user should ever see. Release has no
	// menu to toggle the cheat from at all, so it enables itself
	// unconditionally below instead. Unlike PokerCheat, Release enabling
	// itself here is arguably MORE justified to stay Debug-gated given how
	// unconfirmed the struct offsets still are (see BlackjackCheat.cpp's
	// file header comment) -- kept symmetric with PokerCheat's convention
	// anyway since a wrong-offset read just shows garbage/nothing on
	// screen, not a crash (ReadScriptLocal bounds-checks against the
	// thread's real stack size).
	BuildMenu();
#else
	BlackjackCheat::Toggle();
#endif

	while (true)
	{
#ifdef _DEBUG
		if (!g_menuController.HasActiveMenu() && MenuInput::MenuSwitchPressed())
			g_menuController.PushMenu(g_mainMenu);

		g_menuController.Update();
#endif
		BlackjackCheat::OnTick();

		WAIT(0);
	}
}
