#pragma once

// Blackjack advisor: reads live player/dealer hand data out of bjack_sp's
// own script memory (candidate struct layout, statically traced from the
// decompiled script only -- see BlackjackCheat.cpp's file header comment
// and docs/JOURNAL.md; UNCONFIRMED against live memory, unlike PokerCheat's
// equivalent constants), and draws an on-screen HUD showing the dealer's
// hand, every player hand, and a basic-strategy hit/stand/double/split
// suggestion (see src/BlackjackHandEval.h -- that part is pure math and
// doesn't depend on any of the struct-offset guesses being right).
namespace BlackjackCheat
{
	extern bool Enabled;

	// Flips Enabled and logs the new state. Wired to the F10 menu's
	// "Toggle Blackjack Cheat" item.
	void Toggle();

	// Called every ScriptMain tick regardless of Enabled state (Enabled is
	// checked internally). Draws the HUD when Enabled and bjack_sp is
	// running; no-ops otherwise.
	void OnTick();

#ifdef _DEBUG
	// Everything below is wired to the F10 test menu only (see
	// script.cpp's BuildMenu(), Debug-only) -- dev-tuning/reversing tools,
	// same convention as PokerCheat's equivalents. These exist specifically
	// because the struct offsets below are UNCONFIRMED static-trace
	// candidates -- running these against a real game session (F10 while
	// actually seated at a blackjack table) is the concrete next step
	// needed to confirm or correct them, the same iterative process
	// PokerCheat's docs/JOURNAL.md documents happening over many sessions.

	// Diagnostic: finds bjack_sp's running scrThread and logs every
	// candidate struct field (LaunchArgs slot, Table base, deck
	// cursor/count + next few cards, dealer hand, every seat's hand(s))
	// to BlackjackCheat.log. Wired to the F10 menu's "Probe Table Struct"
	// item.
	void ProbeTableStruct();

	// Diagnostic: logs bjack_sp's script-local stack's start/end absolute
	// addresses (same convention as PokerCheat's DumpLocalStackRange) --
	// meant to be pasted into Cheat Engine for live/visual memory analysis.
	// Wired to the F10 menu's "Dump Local Stack Range" item.
	void DumpLocalStackRange();

	// Diagnostic: dumps, for every candidate seat (0-3), the occupancy
	// marker, hand count, bankroll, and every hand's cards + computed
	// total + bet amount -- same purpose as PokerCheat's
	// ProbeSeatOccupancy, built to sanity-check the per-seat struct
	// candidates against real gameplay (bankroll/bet are Session 2
	// additions -- see BlackjackCheat.cpp's file header comment). Wired
	// to the F10 menu's "Probe Seat Hands" item.
	void ProbeSeatHands();

	// Diagnostic: logs the current deterministic deck-ahead prediction --
	// the dealer's real (already-dealt-but-hidden) hole card plus the
	// simulated stand-on-17 draw-out sequence read straight off the deck
	// array ahead of the current cursor (see BlackjackCheat.cpp's
	// SimulateDealerOutcome()) -- and the next few raw undrawn deck cards.
	// Session 4 addition, the blackjack equivalent of PokerCheat's
	// predicted-board probing. Wired to the F10 menu's "Probe Deck
	// Prediction" item. The same prediction is also self-validated
	// automatically every round (no F10 needed) via a "PredictionCheck"
	// log line written when each round ends -- see
	// BlackjackCheat.cpp's UpdateDeckPrediction().
	void ProbeDeckPrediction();

	// Diagnostic: dumps EVERY script-local slot of bjack_sp's running
	// thread to BlackjackCheat_stackdump.jsonl (see
	// GamePointers::DumpLocalStackJsonl) -- one JSON object per slot, all
	// of i32/u32/i64/f32/hex, no assumption about what any slot means.
	// Built for the specific situation a Probe* function can't help with:
	// a known-wrong field (Session 6 -- the dealer's hole card reads
	// garbage even though its count/first-card neighbors read correctly)
	// where the fix is to search a raw memory window for the ACTUAL
	// value rather than keep guessing candidate offsets one at a time.
	// Wired to the F10 menu's "Dump Full Stack JSONL" item -- run it
	// while a hand you can see the real answer for (e.g. the dealer's
	// up card, or your own hole cards) is on the table, then grep/jq the
	// output for that real rank/suit pair to find where it actually
	// lives.
	void DumpFullStackJsonl();
#endif
}
