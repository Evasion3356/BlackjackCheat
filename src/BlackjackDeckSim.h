#pragma once

// Pure, self-contained "pure cheat" decision engine for bjack_sp -- no
// dependency on ScriptHookRDR2/game memory, same separation-of-concerns
// rationale as BlackjackHandEval.h/BlackjackCardCounting.h (one header
// shared by both the mod and tests/BlackjackDeckSimTests.cpp, so a change
// here is checked by the test suite before it ships instead of only ever
// being eyeballed in-game -- see docs/JOURNAL.md's own note about
// PokerCheat's hand-eval going unverified for 9 sessions specifically
// because it had no automated check on it).
//
// Session 7 addendum -- replaces BlackjackHandEval::GetBasicStrategyAction()
// as the source of hit/stand/double advice (see BlackjackCheat.cpp's own
// file header comment for the full history) with a genuinely deck-derived
// simulation: since bjack_sp deals from one fixed, already-shuffled 52-card
// deck (Session 3), and it's this hand's own turn (nothing else can have
// drawn from the shared cursor since -- the same invariant the mod's
// "Next card" HUD line already relies on), every future card THIS hand's
// own hits would draw is exact, known data, not a probability. This lets
// the engine try every legal "stand after N more hits" stopping point and
// pick whichever one actually wins against the simulated dealer hand,
// instead of following a textbook chart optimized for an unknown next
// card.
//
// A live bug report caught the first version of this algorithm (originally
// written directly inside BlackjackCheat.cpp, not unit-tested) ALWAYS
// recommending Stand, even on a hard 11 with a known next card that
// couldn't possibly bust (3D 8S = 11, next card 2S). Root cause: the
// original selection only updated its "best" candidate when a STRICTLY
// higher-ranked outcome (Win > Push > Loss) was found, so once the very
// FIRST candidate checked (extraHits=0, i.e. stand immediately) was
// recorded as a Loss, every later candidate that was ALSO a Loss (just at
// a higher, still-losing total) never displaced it -- even though a
// strictly higher non-bust total can only match or beat a lower one
// against the same comparison hand (see CompareOutcome: the outcome only
// depends on total/bust, and total only ever increases as more cards are
// added). Fixed here by tie-breaking within the same outcome rank toward
// the HIGHEST non-bust total instead of keeping whichever candidate
// happened to be evaluated first -- see DetermineCheatAction()'s own
// inline comments and tests/BlackjackDeckSimTests.cpp's "hard 11 always
// hits" case, added specifically to pin this down and prevent a
// regression. This bug had gone live because the original version lived
// directly in BlackjackCheat.cpp, reading straight from game memory, with
// no automated test possible -- moving it to this pure header (mirroring
// BlackjackHandEval.h/BlackjackCardCounting.h's own game-memory-free
// design) is what makes it testable at all.
//
// Split is deliberately NOT handled by this header -- BlackjackCheat.cpp
// still asks BlackjackHandEval::GetBasicStrategyAction() for the
// split/no-split call (the textbook pair chart) before ever calling
// DetermineCheatAction() below. A genuinely deck-derived split decision
// would need to simulate both resulting hands' own draw-outs plus the
// dealer's, which is real additional work not attempted this pass --
// flagged as a known scope limit, not a silent gap.

#include "BlackjackHandEval.h"

#include <cstdint>

namespace BlackjackDeckSim
{
	// Same cap as BlackjackCheat.cpp's kHandMaxCards (the struct's own
	// per-hand card array size) -- kept in sync manually since this header
	// has no game-struct dependency to derive it from.
	constexpr std::int32_t kHandMaxCards = 11;

	enum class Outcome { Win, Push, Loss };

	inline int OutcomeRank(Outcome outcome)
	{
		return outcome == Outcome::Win ? 2 : (outcome == Outcome::Loss ? 0 : 1);
	}

	inline Outcome CompareOutcome(const BlackjackHandEval::HandValue& player, const BlackjackHandEval::HandValue& dealer)
	{
		if (player.bust)
			return Outcome::Loss;
		if (dealer.bust)
			return Outcome::Win;
		if (player.total > dealer.total)
			return Outcome::Win;
		if (player.total < dealer.total)
			return Outcome::Loss;
		return Outcome::Push;
	}

	struct DealerSimResult
	{
		BlackjackHandEval::HandValue value;
		std::int32_t drawn = 0; // how many of futureRanks were actually consumed
	};

	// Simulates the dealer's forced stand-on-17 draw-out purely from rank
	// data: dealerRanks/dealerCount is the dealer's own already-dealt hand
	// (both cards, hole card included -- see BlackjackCheat.cpp for why
	// that's real, already-known data, not a guess), futureRanks/
	// futureCount is the exact sequence of undrawn deck cards starting at
	// whatever cursor position the dealer's turn would actually begin
	// from. Suit is irrelevant to blackjack scoring (BlackjackHandEval.h
	// never looks at it) so this deliberately only takes ranks, keeping
	// the signature -- and the tests against it -- simple.
	inline DealerSimResult SimulateDealerFromRanks(const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount)
	{
		std::int32_t ranks[kHandMaxCards];
		std::int32_t count = dealerCount;
		if (count > kHandMaxCards)
			count = kHandMaxCards;
		for (std::int32_t i = 0; i < count; i++)
			ranks[i] = dealerRanks[i];

		DealerSimResult result;
		result.value = BlackjackHandEval::EvaluateHand(ranks, count);

		while (result.value.total < 17 && count < kHandMaxCards && result.drawn < futureCount)
		{
			ranks[count] = futureRanks[result.drawn];
			count++;
			result.drawn++;
			result.value = BlackjackHandEval::EvaluateHand(ranks, count);
		}

		return result;
	}

	// The actual "pure cheat" hit/stand/double decision -- see this file's
	// own header comment above for the full derivation and the bug it was
	// written to fix. playerRanks/playerCount is the current hand;
	// dealerRanks/dealerCount is the dealer's own already-dealt hand;
	// futureRanks/futureCount is the exact undrawn deck starting at the
	// LIVE cursor (shared by this hand's own hits, any seats after it, and
	// the dealer alike -- see the caveat below). isSplitAceHand forces
	// Stand regardless of everything else, the one case this function
	// still needs to know about directly (the game itself never re-offers
	// an action after a split-Ace hand's single forced card -- see
	// BlackjackHandEval.h's own header comment).
	//
	// Caveat this inherits from SimulateDealerFromRanks() and does NOT
	// solve: if another occupied seat still has to act before the dealer,
	// their real hits will shift the cursor by an amount this function
	// can't predict (this project deliberately never ported bjack_sp's own
	// ~1860-line AI decision table, func_623 -- see BlackjackCheat.cpp's
	// file header, Session 5 addendum) -- so the simulated dealer hand
	// this compares against is exact once this is the last seat left to
	// act before the dealer, and a "what if nobody else draws" provisional
	// guess otherwise, exactly like the mod's own "Predicted dealer draws"
	// HUD line already is.
	inline BlackjackHandEval::Action DetermineCheatAction(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isSplitAceHand)
	{
		if (isSplitAceHand)
			return BlackjackHandEval::Action::Stand;

		std::int32_t ranks[kHandMaxCards];
		std::int32_t count = playerCount;
		if (count > kHandMaxCards)
			count = kHandMaxCards;
		for (std::int32_t i = 0; i < count; i++)
			ranks[i] = playerRanks[i];

		std::int32_t futureUsed = 0;
		std::int32_t bestExtraHits = 0;
		std::int32_t bestRank = -1;
		std::int32_t bestTotal = -1;
		bool haveBest = false;

		for (std::int32_t extraHits = 0; ; extraHits++)
		{
			BlackjackHandEval::HandValue value = BlackjackHandEval::EvaluateHand(ranks, count);
			if (value.bust)
				break; // strictly worse than any already-recorded non-bust candidate; further hits only add more bust cards

			DealerSimResult dealerSim = SimulateDealerFromRanks(dealerRanks, dealerCount, futureRanks + futureUsed, futureCount - futureUsed);
			Outcome outcome = CompareOutcome(value, dealerSim.value);
			std::int32_t rank = OutcomeRank(outcome);

			// Tie-break within the same outcome rank toward the HIGHER
			// total instead of keeping whichever candidate was evaluated
			// first -- see this file's own header comment for the bug
			// this specifically fixes.
			if (!haveBest || rank > bestRank || (rank == bestRank && value.total > bestTotal))
			{
				bestRank = rank;
				bestTotal = value.total;
				bestExtraHits = extraHits;
				haveBest = true;
			}

			if (count >= kHandMaxCards || futureUsed >= futureCount)
				break;

			ranks[count] = futureRanks[futureUsed];
			count++;
			futureUsed++;
		}

		if (bestExtraHits == 0)
			return BlackjackHandEval::Action::Stand;

		if (canDouble && playerCount == 2 && bestExtraHits == 1 && bestRank == OutcomeRank(Outcome::Win))
			return BlackjackHandEval::Action::Double;

		return BlackjackHandEval::Action::Hit;
	}
}
