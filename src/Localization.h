/*
	Localizes the handful of strings this mod actually draws on screen in
	a Release build -- the HIT/STAND/DOUBLE/SPLIT advice readout
	(ActionName(), see DrawAdviceStatus() in BlackjackCheat.cpp), the
	BET MAX/MIN betting-advice readout (BetSizeLabel(), see
	DrawBettingAdviceStatus()), the Insurance YES/No readout (see
	DrawInsuranceStatus()), and the "Next cards:" label (see
	DrawNextCardStatus()). Everything else this mod draws (the Debug-only
	text panel, DrawLine()/DrawPanel() in BlackjackCheat.cpp) is
	#ifdef _DEBUG-only -- a dev diagnostic surface, never shown to an end
	user -- and stays English-only; not worth translating. Ported from
	PokerCheat's own Localization.h/.cpp (see that project's
	docs/JOURNAL.md Session 19 for the original derivation) -- same
	approach, different string set, since this mod's on-screen text is
	entirely different from poker's verdict/personality tags.

	Language is auto-detected from the game's own current UI language via
	LANGUAGE::_GET_CURRENT_LANGUAGE_ID() (see Localization.cpp), so a
	player sees this mod's HUD in whatever language they already have
	RDR2 itself set to, with no config needed. BlackjackCheat.ini's
	[General] Language key can override that per Config.h's header
	comment on Config::Values::Language, for anyone who wants the HUD in
	a different language than their game UI.

	Translations beyond English are LLM-assisted, not yet reviewed by a
	native speaker per language -- if a wording is wrong for a given
	language, fix the corresponding row in Localization.cpp's
	kActionLabels/kBetSizeLabels/kInsuranceLabels/
	kNextCardsLabel tables directly, no other file needs to change.

	$Font5 (this mod's text pipeline, see WrapBgFormatText()'s header
	comment in BlackjackCheat.cpp) was already confirmed to render all 13
	of these languages correctly, including CJK, by PokerCheat's own
	Session 20/21 font testing on this exact game build (1491.50) -- see
	PokerCheat's docs/PITFALLS.md. Both mods use the identical
	UIDEBUG::_BG_DISPLAY_TEXT/$Font5 pipeline, so that finding carries
	over verbatim; no separate font test tool was built here.
*/

#pragma once

#include "BlackjackDeckSim.h" // BetSize
#include "BlackjackHandEval.h" // Action
#include <cstdint>
#include <string_view>

namespace Localization
{
	// Matches LANGUAGE::_GET_CURRENT_LANGUAGE_ID()'s own return value
	// mapping exactly -- same enum PokerCheat's Localization.h uses
	// (confirmed against rdr3-nativedb-data/natives.json's comment on
	// native hash 0xDB917DA5C6835FCC), these are the 13 languages RDR2
	// itself ships with, not an arbitrary list.
	enum class Language : std::int32_t
	{
		English = 0,             // en-US
		French = 1,               // fr-FR
		German = 2,               // de-DE
		Italian = 3,              // it-IT
		Spanish = 4,              // es-ES
		PortugueseBrazilian = 5,  // pt-BR
		Polish = 6,               // pl-PL
		Russian = 7,              // ru-RU
		Korean = 8,               // ko-KR
		ChineseTraditional = 9,   // zh-TW
		Japanese = 10,            // ja-JP
		SpanishMexican = 11,      // es-MX
		ChineseSimplified = 12,   // zh-CN

		Count = 13
	};

	// Re-resolves the active language from BlackjackCheat.ini's
	// [General] Language override (Config::Get().Language) if set to
	// anything other than "auto", else from the game's own current UI
	// language. MUST be called from within ScriptHookRDR2's script fiber
	// (i.e. from OnTick() or a menu action running inside ScriptMain's
	// loop), never from DllMain -- unlike Config::Reload(), this calls a
	// real game native (LANGUAGE::_GET_CURRENT_LANGUAGE_ID()) and
	// natives aren't safe to invoke outside the registered script
	// thread's own cooperative fiber. Current() below lazily calls this
	// on first use instead, the same "g_loaded" pattern Config::Get()
	// already uses, so nothing needs to call this explicitly except the
	// Debug F11 menu's "Reload Config" item (to re-pick the language
	// immediately after an ini edit, without waiting for the next
	// natural call) -- see script.cpp.
	void Refresh();

	// Returns the cached language, resolving it via Refresh() on first
	// call if nothing has resolved it yet.
	Language Current();

	// HIT/STAND/DOUBLE/SPLIT advice readout. Uses Current() for the
	// language.
	std::string_view ActionName(BlackjackHandEval::Action action);

	// BET MAX/MIN betting-advice label (the amount is appended by the
	// caller). Uses Current() for the language.
	std::string_view BetSizeLabel(BlackjackDeckSim::BetSize size);

	// "Insurance: YES"/"Insurance: No" readout. Uses Current() for the
	// language.
	std::string_view InsuranceLabel(bool takeInsurance);

	// "Next cards:" label, shown above the next-card-if-you-Hit icon
	// strip. Uses Current() for the language.
	std::string_view NextCardsLabel();

	// Short language code ("en-US", "fr-FR", ...) for a given language --
	// purely for logging, not used by anything Release-facing.
	std::string_view LanguageCode(Language lang);
}
