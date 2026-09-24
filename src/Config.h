/*
	Lightweight INI-backed config, vendored from PokerCheat's Config.h/.cpp
	(same inipp-based approach, same reasons for it -- see that file's
	original header comment for the mINI-vs-inipp backstory, not repeated
	here). Values below are blackjack's own HUD tunables, not poker's.
*/

#pragma once

#include <string>

namespace Config
{
	struct Values
	{
		// Trimmed (Session 8, user request) to only the toggles that do
		// something in Release: ShowPlayerHands only ever gated a
		// Debug-only per-seat text dump nobody but a dev cares about (the
		// user explicitly doesn't want opponents' hands surfaced at all,
		// not even in Debug), and ShowInsuranceAdvice is folded into
		// ShowAdvice below -- insurance IS advice, no reason for its own
		// switch. ShowCardCount, a stray leftover key some old
		// BlackjackCheat.ini files still have on disk from before Session
		// 7 removed card counting, was never a real field here and is
		// actively dropped on the next Reload/save now -- see
		// Config.cpp's ReloadImpl().
		bool ShowDealerHand = true;
		bool ShowBettingAdvice = true;  // Pre-deal BET MAX/MIN readout with the amount, drawn ABOVE ShowAdvice's own line -- see BlackjackCheat.cpp's DrawBettingAdviceStatus()/DetermineBettingAdvice() and BlackjackDeckSim::AdvisePreDealBet(). Own toggle, independent of ShowAdvice -- the user may want the hit/stand/double/split advice without the betting readout or vice versa.
		bool ShowAdvice = true;         // hit/stand/double/split AND insurance -- see BlackjackCheat.cpp's DrawInsuranceStatus() call site
		bool ShowDeckPrediction = true; // Release+Debug -- PRIMARY feature as of Session 4: dealer's real hole card icon + the "Next cards" 3-card-ahead preview, see BlackjackCheat.cpp's SimulateDealerOutcome()
		bool BetHotkeys = true;         // Right/Left arrow = bet 5 steps down/up (hold to repeat, like the game's Up/Down), Tab = bet max, with a "-/+" and a BET MAX prompt beside the game's own; not shown for insurance. Writes the bet amount directly (the mod's only memory write), always within the game's own limits -- see BlackjackCheat.cpp's UpdateBetHotkeys(). false = off entirely: no prompts, the keys do nothing.
		bool ShowCardsBeforeBet = true; // Release+Debug -- Session 9: predicted dealer/your-hand icons shown BEFORE the round is even dealt, see BlackjackCheat.cpp's SimulatePreDeal(). Separate from ShowDeckPrediction (which only ever applies post-deal) since this is a distinctly more provisional guess -- see that function's own header comment -- and the user may want it off independently. Gated on the table's round-phase field reading 2, "waiting for bet" (Table.f_701, see BlackjackCheat.cpp's kRoundPhaseOffset/IsAtBettingPhase()), so it only shows once the table is genuinely free to act on bets -- an earlier version of this gate guessed at a fixed real-time delay instead (PreDealSettleDelaySeconds, since removed) before that lock field was identified.

		// Overrides which language the HUD's advice/betting/insurance/
		// next-cards labels show in (see Localization.h/.cpp) -- "auto"
		// (the default) matches the game's own current UI language
		// automatically via LANGUAGE::_GET_CURRENT_LANGUAGE_ID(), no
		// setup needed. Set to one of en-US/fr-FR/de-DE/it-IT/es-ES/
		// pt-BR/pl-PL/ru-RU/ko-KR/zh-TW/ja-JP/es-MX/zh-CN (the exact
		// codes that native itself maps to) to force a specific
		// language regardless of the game's own UI language; anything
		// else unrecognized (a typo, or this default "auto") falls back
		// to that same auto-detect behavior. Ported from PokerCheat's
		// identical Language key.
		std::string Language = "auto";

#ifdef _DEBUG
		// HUD text panel position/scale -- dev-tuning values, not something
		// an end user should need to calibrate. Release bakes in the
		// user-confirmed values directly instead (see BlackjackCheat.cpp),
		// same convention as PokerCheat's own PanelX/PanelY/TextScale.
		float PanelX = 0.015f;
		float PanelY = 0.30f;
		float TextScale = 0.32f;
		float TitleTextScale = 0.38f;

		// Standalone advice readout (hit/stand/double/split), separate
		// from the text panel -- same convention as PokerCheat's
		// WinPredictionX/Y.
		float AdviceX = 0.4f;
		float AdviceY = 0.5f;

		// Dealer hole-card icon, drawn top-right -- same calibrated spot
		// PokerCheat's own community-card strip uses (Card2DIconBaseX/Y in
		// PokerCheat's Config.h), per user request to show the dealer's
		// real (but on-screen face-down) hole card as an actual card-face
		// sprite there instead of a text line. See BlackjackCheat.cpp's
		// DrawDealerHoleCardIcon(). X user-confirmed via live Reload Config
		// tuning (0.821 initial guess -> 0.957).
		float HoleCardIconX = 0.957f;
		float HoleCardIconY = 0.078f;
		float HoleCardIconWidth = 0.03f;
		float HoleCardIconHeight = 0.075f;

		// "Next cards (if you Hit)" icon strip, drawn to the right of the
		// "Next cards:" label -- see BlackjackCheat.cpp's
		// DrawNextCardIcons(). BaseX user-confirmed via live Reload Config
		float NextCardIconBaseX = 0.48f;
		float NextCardIconY = 0.59f;
		float NextCardIconSpacingX = 0.03f;
		float NextCardIconWidth = 0.025f;
		float NextCardIconHeight = 0.06f;

		// Player's own predicted hand, pre-deal/betting-phase only (see
		// BlackjackCheat.cpp's SimulatePreDeal()) -- drawn near the
		// player's own on-screen avatar rather than reusing the
		// NextCardIcon* slot above, which is now free during betting
		// phase to instead show the post-deal-style "next cards" preview
		// (see DrawOverlay()'s betting-phase block). User-confirmed via
		// live Reload Config tuning (0.2/0.2 initial guess -> these) --
		// same process HoleCardIconX/NextCardIconBaseX already went
		// through.
		float MyHandIconX = 0.14f;
		float MyHandIconY = 0.925f;
		float MyHandIconSpacingX = 0.02f;
		float MyHandIconWidth = 0.02f;
		float MyHandIconHeight = 0.04f;
#endif
	};

	// Returns the current config, triggering the very first load
	// automatically on first call.
	const Values& Get();

	// Re-reads BlackjackCheat.ini from disk, replacing the cached values.
	// Wired to the F11 menu's "Reload Config" item; also called once,
	// eagerly, from DllMain.
	void Reload();
}
