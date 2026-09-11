/*
	Lightweight INI-backed config, vendored from PokerCheat's Config.h/.cpp
	(same inipp-based approach, same reasons for it -- see that file's
	original header comment for the mINI-vs-inipp backstory, not repeated
	here). Values below are blackjack's own HUD tunables, not poker's.
*/

#pragma once

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
		bool ShowAdvice = true;         // hit/stand/double/split AND insurance -- see BlackjackCheat.cpp's DrawInsuranceStatus() call site
		bool ShowDeckPrediction = true; // Release+Debug -- PRIMARY feature as of Session 4: dealer's real hole card icon + the "Next cards" 3-card-ahead preview, see BlackjackCheat.cpp's SimulateDealerOutcome()

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
		float AdviceX = 0.48f;
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
		// tuning (0.62 initial guess -> 0.55).
		float NextCardIconBaseX = 0.55f;
		float NextCardIconY = 0.59f;
		float NextCardIconSpacingX = 0.03f;
		float NextCardIconWidth = 0.025f;
		float NextCardIconHeight = 0.06f;
#endif
	};

	// Returns the current config, triggering the very first load
	// automatically on first call.
	const Values& Get();

	// Re-reads BlackjackCheat.ini from disk, replacing the cached values.
	// Wired to the F10 menu's "Reload Config" item; also called once,
	// eagerly, from DllMain.
	void Reload();
}
