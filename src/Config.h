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
		bool ShowDealerHand = true;
		bool ShowPlayerHands = true;
		bool ShowAdvice = true;
		bool ShowInsuranceAdvice = true; // Release+Debug, same convention as ShowAdvice -- now a deterministic read of the already-known dealer hole card, not a card-count deviation (Session 7 fifth addendum, see BlackjackCheat.cpp)
		bool ShowDeckPrediction = true;  // Release+Debug -- PRIMARY feature as of Session 4: dealer's real hole card + deterministic draw-out prediction, see BlackjackCheat.cpp's SimulateDealerOutcome()

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
