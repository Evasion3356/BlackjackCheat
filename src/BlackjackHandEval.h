#pragma once

// Self-contained blackjack hand evaluator + basic-strategy advisor -- no
// dependency on ScriptHookRDR2/game memory, same separation-of-concerns
// rationale as PokerCheat's PokerHandEval.h (one header shared by both the
// mod and tests/BlackjackHandEvalTests.cpp, so a change here is checked by
// the test suite before it ships instead of only ever being eyeballed
// in-game).
//
// Card rank convention matches PokerCheat's/bjack_sp's own encoding (see
// BlackjackCheat.cpp's file header comment): 2-10 numeric, 11=J, 12=Q,
// 13=K, 14=A. Suit is irrelevant to blackjack and not represented here.
//
// Basic strategy below is the standard, widely-published chart for
// 4-8 deck shoes, dealer STANDS on soft 17, double allowed on any 2 cards
// (double-after-split included), no surrender -- this is pure math,
// independently correct regardless of bjack_sp's own memory layout.
// Session 2 static re-trace raised confidence on most of this rule set
// considerably (still NOT live-confirmed, see docs/JOURNAL.md Session 2
// for full citations); Session 3 found bjack_sp actually deals from a
// SINGLE 52-card deck, rebuilt and reshuffled every round -- NOT a 4-8
// deck shoe as assumed here (func_458, bjack_sp.ysc.c line ~18168, is an
// unambiguous 4-suit x 13-rank = 52 card nested loop, `f_106 = num2` = 52
// always; func_718 case 0, line ~25823, calls func_459 -- which rebuilds
// AND reshuffles that same 52-card array via func_458+func_933 -- at the
// start of every single round, confirmed via that call site). Real
// single-deck basic strategy charts differ from multi-deck ones at a
// handful of borderline hands (commonly cited: doubling 8 vs. 5/6,
// doubling soft 18/19 vs. 2, splitting 6,6 vs. 7, splitting 4,4/3,3/2,2
// vs. a few more dealer upcards) -- the chart below is STILL the
// multi-deck chart and has NOT been corrected for single-deck play; this
// is a known, explicitly flagged gap for a future session, not a silent
// assumption. See docs/JOURNAL.md Session 3.
//   - Dealer stands on soft 17: the dealer's draw loop is
//     `while (dealerHand.f_24 < 17) { hit }` (bjack_sp.ysc.c line
//     ~26058), and f_24 is now confirmed (not just assumed) to be an
//     ace-adjusted BEST total, not a raw sum -- bjack_sp's own func_645
//     (line ~23356) is a byte-for-byte structural match of this file's
//     own EvaluateHand() ace-demotion loop, and its func_1010 (line
//     ~33066) is an exact match of CardValue() (2-10 face, J/Q/K=10,
//     A=11). Since a soft 17 (e.g. A,6) therefore computes f_24=17, not
//     27, the "< 17" condition is false and the dealer stops -- this is
//     now a solid logical derivation FROM confirmed source, not an
//     inference from an absent branch, even though it's still not been
//     watched happen live.
//   - Double allowed on any 2 cards, no total restriction: bjack_sp's
//     own action-legality gate (func_1237 case 4, bjack_sp.ysc.c line
//     ~40771) only checks `hand.f_23 <= 2` (card count) and a bankroll
//     check (`f_1 >= f_4[handIndex]`) -- no total-value gate at all.
//   - No surrender: confirmed absent, not just unchecked -- an
//     exhaustive case-insensitive search of bjack_sp.ysc.c for
//     "surrender" returns zero hits (vs. real insurance/double/split
//     mechanics, which all have message-string hits).
//   - Split requires exact RANK match (see GetBasicStrategyAction()'s own
//     header comment) and is capped at ONE split (2 hands total, no
//     re-splitting): func_1237 case 6 (line ~40785) returns false
//     outright when `seat.f_59 > 1`, i.e. once a hand has already been
//     split once.
//   - Split Aces DO get the standard "one card each, then forced stand"
//     restriction -- Session 3 correction, reversing Session 2's "no
//     ace-specific restriction found" conclusion (that conclusion looked
//     only at func_1237's legality gate itself, which is indeed
//     ace-blind, and missed the actual enforcement mechanism, which
//     lives one level up in the per-seat turn state machine). Traced via
//     `f_699`, a "how many hands to force-resolve without further player
//     input" counter: func_1067 (the action executor) sets f_699=1 after
//     Stand (case 7, line ~35474) and after Double (case 4, line
//     ~35437) -- both turn-ending actions for that one hand -- and
//     leaves it at 0 after an ordinary Hit (case 5) or an ordinary Split
//     of a non-Ace pair (case 6, line ~35447-35468, no f_699 write at
//     all), matching "you keep acting on this hand". But case 6 has one
//     more line (line ~35469-35470): `if (hand[num][0] == 14) f_699 =
//     2` -- i.e. specifically when the ORIGINAL pair being split was
//     Aces (rank 14), it sets f_699 to 2 instead of leaving it at 0. The
//     consuming loop (the state machine's own turn-advance step, line
//     ~26027: `for (j = seat.f_3; func_1068(...) && f_699 > 0 || ...; j
//     ++) { resolve hand j; seat.f_3++; f_699--; }`) then force-resolves
//     exactly `f_699` hands in a row with no player input in between --
//     so f_699=2 after an Ace split walks straight through BOTH new
//     hands automatically, giving each exactly the one card func_1067's
//     split branch already dealt them (`func_1072(..., 1)` called once
//     per new hand right before this check) and never re-prompting for
//     hit/stand/double on either. This IS the classic real-casino "split
//     Aces get one card only" rule -- GetBasicStrategyAction() below
//     takes an explicit `isSplitAceHand` flag for it (see its own header
//     comment) rather than silently returning a legal-looking but
//     never-actually-offered Hit/Double. See docs/JOURNAL.md Session 3
//     for the full trace.
//   - Blackjack payout ratio (3:2 vs 6:5) and deck penetration/reshuffle
//     point: NOT traced, don't affect strategy decisions either way.
// If bjack_sp turns out to diverge from any of the above on live testing,
// only the *Action() functions below need updating -- EvaluateHand()
// itself (hand value, soft/bust/blackjack detection) is rules-independent
// and is now a confirmed structural match to the game's own func_645/
// func_1010, not just an assumption.
//
// All functions are `inline` so this header can be included from more
// than one translation unit without violating the one-definition rule.

#include <cstdint>

namespace BlackjackHandEval
{
	struct HandValue
	{
		std::int32_t total = 0;  // best total <= 21 if not bust, else the (busted) hard total
		bool soft = false;       // true if an Ace is still being counted as 11
		bool bust = false;
		bool blackjack = false;  // exactly 2 cards, natural 21
	};

	enum class Action
	{
		Hit,
		Stand,
		Double,
		Split,
	};

	// Blackjack value of one card: 2-10 face value, J/Q/K=10, A=11 (soft --
	// EvaluateHand() below demotes Aces to 1 as needed).
	inline std::int32_t CardValue(std::int32_t rank)
	{
		if (rank == 14)
			return 11;
		if (rank >= 11 && rank <= 13)
			return 10;
		return rank; // 2-10
	}

	inline HandValue EvaluateHand(const std::int32_t* ranks, std::int32_t count)
	{
		HandValue result;

		std::int32_t total = 0;
		std::int32_t aces = 0;
		for (std::int32_t i = 0; i < count; i++)
		{
			total += CardValue(ranks[i]);
			if (ranks[i] == 14)
				aces++;
		}

		while (total > 21 && aces > 0)
		{
			total -= 10; // demote one Ace from 11 to 1
			aces--;
		}

		result.total = total;
		result.soft = aces > 0; // an Ace is still counted as 11
		result.bust = total > 21;
		result.blackjack = (count == 2 && total == 21);

		return result;
	}

	namespace detail
	{
		inline Action HardAction(std::int32_t total, std::int32_t dealerVal, bool canDouble)
		{
			if (total <= 8)
				return Action::Hit;
			if (total == 9)
				return (canDouble && dealerVal >= 3 && dealerVal <= 6) ? Action::Double : Action::Hit;
			if (total == 10)
				return (canDouble && dealerVal >= 2 && dealerVal <= 9) ? Action::Double : Action::Hit;
			if (total == 11)
				return (canDouble && dealerVal >= 2 && dealerVal <= 10) ? Action::Double : Action::Hit;
			if (total == 12)
				return (dealerVal >= 4 && dealerVal <= 6) ? Action::Stand : Action::Hit;
			if (total >= 13 && total <= 16)
				return (dealerVal >= 2 && dealerVal <= 6) ? Action::Stand : Action::Hit;
			return Action::Stand; // 17+
		}

		inline Action SoftAction(std::int32_t total, std::int32_t dealerVal, bool canDouble)
		{
			if (total <= 14) // soft 13 (A,2) / soft 14 (A,3)
				return (canDouble && dealerVal >= 5 && dealerVal <= 6) ? Action::Double : Action::Hit;
			if (total <= 16) // soft 15 (A,4) / soft 16 (A,5)
				return (canDouble && dealerVal >= 4 && dealerVal <= 6) ? Action::Double : Action::Hit;
			if (total == 17) // soft 17 (A,6)
				return (canDouble && dealerVal >= 3 && dealerVal <= 6) ? Action::Double : Action::Hit;
			if (total == 18) // soft 18 (A,7)
			{
				if (canDouble && dealerVal >= 3 && dealerVal <= 6)
					return Action::Double;
				if (dealerVal == 2 || dealerVal == 7 || dealerVal == 8)
					return Action::Stand;
				return Action::Hit; // 9, 10, Ace
			}
			return Action::Stand; // soft 19+
		}

		// Returns true if the pair chart says split, given the PER-CARD
		// blackjack value (2-11, 11=Ace pair) and the dealer's upcard
		// value.
		inline bool ShouldSplitPair(std::int32_t pairValue, std::int32_t dealerVal)
		{
			switch (pairValue)
			{
				case 11: return true; // A,A
				case 10: return false; // never split tens
				case 9: return dealerVal != 7 && dealerVal != 10 && dealerVal != 11;
				case 8: return true;
				case 7: return dealerVal >= 2 && dealerVal <= 7;
				case 6: return dealerVal >= 2 && dealerVal <= 6;
				case 5: return false; // play as hard 10 instead
				case 4: return dealerVal == 5 || dealerVal == 6; // assumes double-after-split allowed
				case 2:
				case 3: return dealerVal >= 2 && dealerVal <= 7;
				default: return false;
			}
		}
	}

	// playerRanks/playerCount describe the player's current hand;
	// dealerUpcardRank is the dealer's single visible card. canDouble/
	// canSplit gate whether those actions are actually legal right now
	// (e.g. false once the hand already has 3+ cards) -- when gated off,
	// this falls back to the equivalent Hit/Stand recommendation rather
	// than ever returning an illegal action.
	//
	// isSplitAceHand: true when THIS hand is one of the two results of
	// splitting a pair of Aces -- bjack_sp force-resolves both such hands
	// immediately after dealing each one card, never re-offering
	// hit/stand/double (see the header comment above, "Split Aces DO get
	// ..."). When true, this always returns Stand regardless of total,
	// since that's the only outcome the game will actually let happen --
	// recommending Hit/Double here would describe a choice the player
	// was never really offered. The caller (BlackjackCheat.cpp) detects
	// this by the same signal the struct itself gives no direct flag
	// for: a seat with 2 hands in play (only possible after exactly one
	// split, see kMaxHandsPerSeat) whose hand's first (non-drawn) card is
	// an Ace.
	//
	// Pair detection requires the two cards to share the exact RANK, not
	// merely the same blackjack value -- confirmed via bjack_sp's own
	// split-legality gate (func_1237 case 6, bjack_sp.ysc.c line ~40791:
	// `if (hand[0] != hand[1]) return false`, comparing raw card values
	// 2-14, not CardValue()'s 2-11 blackjack values). This game does NOT
	// let you split a King+Queen (both worth 10 but rank 13 vs 12) even
	// though some real casinos do -- using CardValue() equality here
	// would have recommended an illegal split. See docs/JOURNAL.md
	// Session 2.
	inline Action GetBasicStrategyAction(const std::int32_t* playerRanks, std::int32_t playerCount, std::int32_t dealerUpcardRank, bool canDouble, bool canSplit, bool isSplitAceHand = false)
	{
		if (isSplitAceHand)
			return Action::Stand;

		std::int32_t dealerVal = CardValue(dealerUpcardRank);

		if (canSplit && playerCount == 2 && playerRanks[0] == playerRanks[1])
		{
			std::int32_t pairValue = CardValue(playerRanks[0]);
			if (detail::ShouldSplitPair(pairValue, dealerVal))
				return Action::Split;
		}

		HandValue hand = EvaluateHand(playerRanks, playerCount);
		bool doubleLegal = canDouble && playerCount == 2;

		if (hand.soft)
			return detail::SoftAction(hand.total, dealerVal, doubleLegal);

		return detail::HardAction(hand.total, dealerVal, doubleLegal);
	}
}
