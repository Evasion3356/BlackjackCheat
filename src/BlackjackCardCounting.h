#pragma once

// Self-contained Hi-Lo card counter -- pure math, zero game dependency,
// same separation-of-concerns rationale as BlackjackHandEval.h (one header
// shared by the mod and tests/BlackjackCardCountingTests.cpp).
//
// SECONDARY/legacy as of Session 4: BlackjackCheat.cpp can now read the
// deck array directly (SimulateDealerOutcome()/UpdateDeckPrediction(), the
// blackjack equivalent of PokerCheat's BuildPredictedBoard()) and show the
// dealer's real hole card plus a deterministic simulated draw-out, which is
// strictly better information than a probabilistic Hi-Lo estimate once the
// deck itself is directly readable -- there's no need to infer what's left
// in the deck from a count when the exact cards are sitting in memory. This
// header is kept (correct, tested, still wired into the HUD as a secondary/
// Debug-only line) because it's still a legitimate general-purpose
// technique and costs nothing to leave in, not because it's the
// recommended way to play this specific game anymore.
//
// IMPORTANT single-deck caveat (Session 3, see BlackjackHandEval.h's own
// header comment for the citations): bjack_sp deals from a single 52-card
// deck that is rebuilt AND reshuffled at the start of every round
// (func_458+func_933, called from func_718 case 0) -- not a persistent
// multi-round shoe. A running count therefore MUST reset to 0 at the start
// of every round (BlackjackCheat.cpp's UpdateCardCounting() does this by
// detecting the dealer's hand clearing back to 0 cards). This also means
// the classic multi-deck-shoe counting advantage -- watch the true count
// climb over dozens of rounds as a shoe gets depleted -- mostly doesn't
// apply here: within one ~2-10 card round dealt from a full fresh deck,
// there's limited room for the count to swing far from 0 before the round
// (and the count) resets. The mechanics below are still correctly
// implemented and genuinely informative moment-to-moment, just with a much
// smaller real edge than an actual multi-deck shoe game would give a
// counter -- this file doesn't pretend otherwise, see
// docs/JOURNAL.md Session 3.

#include <cstdint>

namespace BlackjackCardCounting
{
	constexpr std::int32_t kCardsPerDeck = 52;

	// Hi-Lo tag for one card by RANK (2-14, 11=J,12=Q,13=K,14=A -- same
	// convention as BlackjackHandEval.h): 2-6 = +1, 7-9 = 0, 10/J/Q/K/A = -1.
	inline std::int32_t HiLoTag(std::int32_t rank)
	{
		if (rank >= 2 && rank <= 6)
			return 1;
		if (rank >= 7 && rank <= 9)
			return 0;
		return -1; // 10, J, Q, K, A
	}

	struct Count
	{
		std::int32_t running = 0;

		void Reset() { running = 0; }
		void AddCard(std::int32_t rank) { running += HiLoTag(rank); }
	};

	// Decks remaining in the current (single, per-round) deck, as a
	// fraction of one 52-card deck -- e.g. 0.5 once half the deck has been
	// drawn this round. Never negative.
	inline double DecksRemaining(std::int32_t deckTotalCards, std::int32_t deckCursor)
	{
		std::int32_t remaining = deckTotalCards - deckCursor;
		if (remaining < 0)
			remaining = 0;
		return static_cast<double>(remaining) / static_cast<double>(kCardsPerDeck);
	}

	// True count = running count / decks remaining. decksRemaining is
	// floored to a small minimum (4 cards' worth, ~0.077 decks) rather than
	// letting it approach zero near the end of the deck, which would
	// otherwise blow the division up into wild, meaningless swings on the
	// last few cards.
	inline double TrueCount(std::int32_t runningCount, double decksRemaining)
	{
		constexpr double kMinDecksRemaining = 4.0 / kCardsPerDeck;
		if (decksRemaining < kMinDecksRemaining)
			decksRemaining = kMinDecksRemaining;
		return static_cast<double>(runningCount) / decksRemaining;
	}

	// Two of the best-known "Illustrious 18" count deviations -- only the
	// top couple, not the full 18 (see this file's header comment: the
	// single-deck-reshuffled-every-round finding makes most of the
	// classic 18 nearly unreachable in a single round here, so
	// implementing the full set would mostly be effort spent on scenarios
	// that essentially never occur in this specific game).
	inline bool ShouldTakeInsurance(double trueCount)
	{
		return trueCount >= 3.0;
	}

	// Standard strategy for hard 16 vs. dealer 10 is Hit; the
	// Illustrious 18's single most famous deviation is to Stand instead
	// once the true count is non-negative.
	inline bool ShouldStandHard16VsTen(double trueCount)
	{
		return trueCount >= 0.0;
	}
}
