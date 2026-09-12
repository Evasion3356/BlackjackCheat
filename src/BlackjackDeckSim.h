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
// Session 11 addendum -- Split is now ALSO deck-derived, via
// EvaluateSplit() below. It had been deliberately left to
// BlackjackHandEval::GetBasicStrategyAction()'s blind textbook chart
// (see the paragraph this replaces, kept here for history: "would need
// to simulate both resulting hands' own draw-outs plus the dealer's,
// which is real additional work not attempted this pass"), which is
// exactly what produced a live bug report: J,J (a hard 20) was advised
// Stand because the textbook chart hard-codes "never split tens" --
// correct advice against an UNKNOWN next card, but the known next three
// cards that round were an Ace then a 2 then a 7, meaning splitting
// actually produced a 21 (J,A) and, after one more known hit, a 19
// (J,2,7) -- worth more in total bet-unit profit than standing pat on a
// single 20, if the dealer's own hand doesn't beat both. EvaluateSplit()
// answers this exactly the same way DetermineCheatAction() answers
// hit/stand/double: simulate every card that's actually going to be
// dealt (both post-split hands' one guaranteed card, then each hand
// played out via DetermineCheatAction()'s own advice to its own
// conclusion, then the dealer's real draw-out) and compare the summed
// result, in bet-unit terms (Win=+1/Push=0/Loss=-1 per hand), against
// just playing the pair as one ordinary hand. Same trustworthiness
// precondition as DetermineCheatAction() (isLastSeatBeforeDealer OR the
// dealer's already-dealt hand is already 17+) -- when that doesn't
// hold, the caller must fall back to the textbook pair chart instead,
// same convention as everywhere else in this file. See
// tests/BlackjackDeckSimTests.cpp's "known cards make splitting tens
// correct" case, built directly from this bug report.
//
// Session 9 addendum -- isLastSeatBeforeDealer added after a second live
// bug report: a hard 9 (bust-proof no matter what) was advised Stand.
// Root cause was the ALREADY-documented "another occupied seat can shift
// the cursor" caveat above actually manifesting: this function's dealer
// simulation always assumed the very next undrawn card goes straight to
// the dealer, and correctly noticed that STANDING would let the dealer
// draw a card that happened to bust them -- a mathematically real
// insight, but only true if nothing else draws in between. It wasn't
// true that round. What was a documented risk is now an explicit,
// checked precondition: DetermineCheatAction() no longer trusts its own
// simulation at all when another occupied seat still has to act first,
// falling back to BlackjackHandEval::GetBasicStrategyAction() instead --
// see that parameter's own inline comment on DetermineCheatAction()
// below for the full mechanism, and
// tests/BlackjackDeckSimTests.cpp's "hard 9 denied dealer bust" case for
// the regression test built from this exact bug shape.
//
// Session 13 addendum -- EvaluateBettingConfidence() added (user request:
// a "Betting Advice" HUD line, Low/Medium/High, shown above the ordinary
// hit/stand/double/split readout). Deliberately reuses PlayHandOut() (the
// same "what actually happens if this hand follows the engine's own
// advice" helper EvaluateSplit() already relies on) rather than
// introducing a second, parallel simulation -- see that function's own
// header comment below for the full weighting rationale.
//
// Session 10 addendum -- the isLastSeatBeforeDealer fallback above was
// itself too blunt and caused two more live bug reports. (1) Hard 12
// (K,2) with a known next card of King (a certain bust) was advised Hit:
// once another seat forced the fallback to plain textbook strategy, the
// known top-of-deck card was thrown away entirely, even though it's
// THIS hand's own turn right now -- nothing else can draw before this
// hand's own hit does, so the very next card is exact regardless of
// what any later seat or the dealer does afterward. (2) A hard 19
// against a dealer already showing 20 with a known next card of 2 (an
// outright double to 21) was advised Stand: the dealer's hand was
// already fully known and already >=17, meaning it was never going to
// draw a card at all -- another seat's interference is irrelevant to a
// dealer who already stands pat, so the fallback triggered for no
// reason. Both fixed below: dealerOutcomeTrustworthy now also holds
// whenever the dealer's already-dealt hand alone is already >=17 (no
// draw needed, so nothing downstream can invalidate the comparison),
// and even inside the fallback, a known immediate next card that would
// bust this hand overrides Hit/Double to Stand -- the one piece of
// certainty the fallback can always still use. See
// tests/BlackjackDeckSimTests.cpp's "known bust card overrides fallback"
// and "dealer already pat trusts sim regardless of other seats" cases.

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
	// Caveat this inherits from SimulateDealerFromRanks() -- SOLVED below
	// via `isLastSeatBeforeDealer`, not just documented: if another
	// occupied seat still has to act before the dealer, their real hits
	// will shift the cursor by an amount this function can't predict
	// (this project deliberately never ported bjack_sp's own ~1860-line
	// AI decision table, func_623 -- see BlackjackCheat.cpp's file
	// header, Session 5 addendum), so the simulated dealer hand this
	// compares against is exact ONLY once this is the last seat left to
	// act before the dealer.
	//
	// A real, live-reported bug caught what happens when that isn't true
	// but this function trusts the simulation anyway: hard 9 (5,4)
	// against a dealer showing a hand that needs to hit, where the very
	// next undrawn card would bust the dealer -- this function correctly
	// noticed that STANDING denies the player nothing (dealer draws that
	// exact card next and busts) while HITTING would consume that exact
	// card for the player instead, letting the dealer draw a SAFE card
	// afterward and beat the player's own low total -- and so recommended
	// Stand on a hand that can mathematically never bust, which is
	// correct ONLY if nothing else can draw between this hand and the
	// dealer. The user was NOT last to act that round -- another occupied
	// seat still had to play first, meaning the "dealer's next card" this
	// function assumed was actually going to be consumed by that OTHER
	// seat's real hits, not handed straight to the dealer -- so the
	// entire "denying the dealer their bust card" premise was built on a
	// future that was never going to happen. `isLastSeatBeforeDealer`
	// makes this an explicit, checked precondition instead of a silent
	// assumption: when false, this function no longer trusts ANY of its
	// own dealer-outcome simulation (not just the specific denial case
	// above -- every extraHits candidate's dealer comparison shares the
	// exact same broken premise) and instead defers entirely to
	// BlackjackHandEval::GetBasicStrategyAction() -- the same textbook
	// fallback this file already used for Split (see this file's own
	// header comment above), now extended to Hit/Stand/Double too. The
	// dealer's up-card rank for that fallback is read as `dealerRanks[1]`
	// -- the real, visible up card, per the [hole, up] ordering
	// BlackjackCheat.cpp's own callers already use (Session 7) -- with a
	// defensive fallback to `dealerRanks[0]` if dealerCount is somehow
	// under 2 (shouldn't happen in practice; this function is never
	// called before the dealer has its own 2 cards).
	inline BlackjackHandEval::Action DetermineCheatAction(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isSplitAceHand, bool isLastSeatBeforeDealer)
	{
		if (isSplitAceHand)
			return BlackjackHandEval::Action::Stand;

		// The dealer's own hand is already fully known (hole card
		// included) -- if it's already 17+ it stands pat and never
		// touches the future deck at all, so the simulation below is
		// exact regardless of any other seat still left to act. Only a
		// dealer who still needs to hit is actually at risk from another
		// seat's real draws shifting the cursor first.
		BlackjackHandEval::HandValue dealerKnown = BlackjackHandEval::EvaluateHand(dealerRanks, dealerCount);
		bool dealerOutcomeTrustworthy = isLastSeatBeforeDealer || dealerKnown.total >= 17;

		if (!dealerOutcomeTrustworthy)
		{
			std::int32_t dealerUpcardRank = dealerCount >= 2 ? dealerRanks[1] : dealerRanks[0];
			BlackjackHandEval::Action fallback = BlackjackHandEval::GetBasicStrategyAction(playerRanks, playerCount, dealerUpcardRank, canDouble, /*canSplit*/ false, isSplitAceHand);

			// One piece of the deck is still exact even here: it's this
			// hand's own turn right now, so the very next undrawn card is
			// guaranteed to be what THIS hand draws if it hits/doubles --
			// nothing else can get to it first. Basic strategy is blind
			// to that card by design; never let it recommend drawing a
			// card already known to bust us.
			if ((fallback == BlackjackHandEval::Action::Hit || fallback == BlackjackHandEval::Action::Double) && futureCount > 0 && playerCount < kHandMaxCards)
			{
				std::int32_t ranks[kHandMaxCards];
				std::int32_t count = playerCount;
				for (std::int32_t i = 0; i < count; i++)
					ranks[i] = playerRanks[i];
				ranks[count] = futureRanks[0];
				count++;

				if (BlackjackHandEval::EvaluateHand(ranks, count).bust)
					return BlackjackHandEval::Action::Stand;
			}

			return fallback;
		}

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

	// +1/0/-1 bet-unit value of an outcome -- lets EvaluateSplit() below
	// compare "one hand" against "two hands, each its own bet" on a
	// common scale instead of just comparing win/loss/push categories,
	// which would treat "stand pat and win 1 unit" and "split into two
	// winning hands, +2 units" as an indistinguishable tie.
	inline std::int32_t OutcomeValue(Outcome outcome)
	{
		return outcome == Outcome::Win ? 1 : (outcome == Outcome::Loss ? -1 : 0);
	}

	struct PlayoutResult
	{
		BlackjackHandEval::HandValue value;
		std::int32_t consumed = 0; // cards actually drawn from futureRanks
	};

	// Plays a hand to its own conclusion by repeatedly asking
	// DetermineCheatAction() what it would do and applying that action,
	// consuming real future cards as it goes -- "what actually happens
	// if this hand follows the engine's own advice." Used by
	// EvaluateSplit() below for each of the two post-split hands (and
	// for the pair played as a single ordinary hand, its "don't split"
	// baseline). isLastSeatBeforeDealer here means "is nothing else
	// still going to draw between the END of this specific hand and the
	// dealer" -- for a post-split hand 1, that's always false (hand 2 is
	// still to come); the caller is responsible for passing the right
	// value per hand, same as DetermineCheatAction() itself never
	// assumes it.
	inline PlayoutResult PlayHandOut(
		const std::int32_t* startRanks, std::int32_t startCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isLastSeatBeforeDealer)
	{
		std::int32_t ranks[kHandMaxCards];
		std::int32_t count = startCount > kHandMaxCards ? kHandMaxCards : startCount;
		for (std::int32_t i = 0; i < count; i++)
			ranks[i] = startRanks[i];

		PlayoutResult result;
		result.value = BlackjackHandEval::EvaluateHand(ranks, count);

		while (!result.value.bust && count < kHandMaxCards && result.consumed < futureCount)
		{
			BlackjackHandEval::Action action = DetermineCheatAction(ranks, count, dealerRanks, dealerCount,
				futureRanks + result.consumed, futureCount - result.consumed, canDouble, /*isSplitAceHand*/ false, isLastSeatBeforeDealer);

			if (action == BlackjackHandEval::Action::Stand)
				break;

			ranks[count] = futureRanks[result.consumed];
			count++;
			result.consumed++;
			result.value = BlackjackHandEval::EvaluateHand(ranks, count);

			if (action == BlackjackHandEval::Action::Double)
				break; // exactly one card, then forced stand
		}

		return result;
	}

	struct SplitDecision
	{
		bool trustworthy = false; // false means the caller must fall back to the textbook pair chart instead of trusting shouldSplit
		bool shouldSplit = false;
	};

	// See this file's own header comment (Session 11 addendum) for the
	// full derivation and the live bug this fixes. playerRanks[0]/[1]
	// are the matching pair being considered (caller has already
	// confirmed the rank match and that splitting is legal here);
	// dealerRanks/dealerCount is the dealer's own already-dealt hand;
	// futureRanks/futureCount is the exact undrawn deck from the LIVE
	// cursor. canDoubleAfterSplit is the same double-legality flag the
	// caller already computes for the pair itself (card-count +
	// bankroll) -- this project doesn't model a separate, larger
	// bankroll requirement for affording a double on EACH of two split
	// hands; a known simplification, not a silent one.
	inline SplitDecision EvaluateSplit(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDoubleAfterSplit, bool isLastSeatBeforeDealer)
	{
		SplitDecision result;

		if (playerCount != 2 || futureCount < 2)
			return result;

		BlackjackHandEval::HandValue dealerKnown = BlackjackHandEval::EvaluateHand(dealerRanks, dealerCount);
		bool dealerOutcomeTrustworthy = isLastSeatBeforeDealer || dealerKnown.total >= 17;
		if (!dealerOutcomeTrustworthy)
			return result;

		std::int32_t pairRank = playerRanks[0];

		// Value of NOT splitting: play the pair as one ordinary hand.
		PlayoutResult noSplit = PlayHandOut(playerRanks, 2, dealerRanks, dealerCount, futureRanks, futureCount, canDoubleAfterSplit, isLastSeatBeforeDealer);
		DealerSimResult dealerForNoSplit = SimulateDealerFromRanks(dealerRanks, dealerCount, futureRanks + noSplit.consumed, futureCount - noSplit.consumed);
		std::int32_t noSplitValue = OutcomeValue(CompareOutcome(noSplit.value, dealerForNoSplit.value));

		// Value of splitting: both new hands get their one guaranteed
		// card immediately (future[0] then future[1], per bjack_sp's own
		// split-dealing order -- see BlackjackHandEval.h's Ace-split
		// header comment), THEN each plays out in turn (hand 1 first,
		// hand 2 second). A split pair of Aces is forced to stand on
		// that one dealt card with no further play at all (the same
		// rule DetermineCheatAction()'s isSplitAceHand handles) rather
		// than being run through PlayHandOut(), which would otherwise
		// describe hits the game never actually offers.
		bool isAcePair = (pairRank == 14);
		std::int32_t hand1Start[2] = { pairRank, futureRanks[0] };
		std::int32_t hand2Start[2] = { pairRank, futureRanks[1] };

		PlayoutResult hand1;
		if (isAcePair)
			hand1.value = BlackjackHandEval::EvaluateHand(hand1Start, 2);
		else
			hand1 = PlayHandOut(hand1Start, 2, dealerRanks, dealerCount, futureRanks + 2, futureCount - 2, canDoubleAfterSplit, /*isLastSeatBeforeDealer*/ false);

		std::int32_t hand2FutureOffset = 2 + hand1.consumed;
		PlayoutResult hand2;
		if (isAcePair)
			hand2.value = BlackjackHandEval::EvaluateHand(hand2Start, 2);
		else
			hand2 = PlayHandOut(hand2Start, 2, dealerRanks, dealerCount, futureRanks + hand2FutureOffset, futureCount - hand2FutureOffset, canDoubleAfterSplit, isLastSeatBeforeDealer);

		std::int32_t dealerFutureOffset = hand2FutureOffset + hand2.consumed;
		DealerSimResult dealerForSplit = SimulateDealerFromRanks(dealerRanks, dealerCount, futureRanks + dealerFutureOffset, futureCount - dealerFutureOffset);

		std::int32_t splitValue = OutcomeValue(CompareOutcome(hand1.value, dealerForSplit.value)) + OutcomeValue(CompareOutcome(hand2.value, dealerForSplit.value));

		result.trustworthy = true;
		result.shouldSplit = splitValue > noSplitValue;
		return result;
	}

	// Session 13 addition -- Betting Advice. Answers "how strongly does
	// the CURRENT hand favor the player, in bet-sizing terms" by playing
	// the hand out with the engine's own best advice (PlayHandOut(), the
	// same helper EvaluateSplit() above already uses for its own
	// "what actually happens" comparison) and comparing the result to the
	// dealer's own simulated final hand. High confidence: an immediate
	// natural blackjack (resolves against the dealer's own already-dealt
	// two cards -- real data, not a guess, see BlackjackCheat.cpp's file
	// header, Session 4 -- with no draw-out needed on either side, so
	// it's exact regardless of isLastSeatBeforeDealer) or a win reached by
	// simply standing on the hand as dealt (PlayHandOut() consumed no
	// extra cards to get there). Medium: a win that only materializes by
	// hitting/doubling into it -- the "could win it if the cards advance"
	// case. Low: anything that ends in a push or a loss. Same
	// dealerOutcomeTrustworthy precondition as DetermineCheatAction()/
	// EvaluateSplit() above -- when it doesn't hold, `trustworthy` comes
	// back false and the caller must fall back to
	// BlackjackHandEval::EstimateBettingConfidence() (a rough,
	// non-deck-derived heuristic) instead of trusting `confidence`.
	struct BettingAdvice
	{
		bool trustworthy = false;
		Outcome outcome = Outcome::Push;
		BlackjackHandEval::BettingConfidence confidence = BlackjackHandEval::BettingConfidence::Low;
	};

	inline BettingAdvice EvaluateBettingConfidence(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isLastSeatBeforeDealer)
	{
		BettingAdvice result;

		BlackjackHandEval::HandValue playerNow = BlackjackHandEval::EvaluateHand(playerRanks, playerCount);
		BlackjackHandEval::HandValue dealerNow = BlackjackHandEval::EvaluateHand(dealerRanks, dealerCount);

		if (playerNow.blackjack)
		{
			result.trustworthy = true;
			result.outcome = dealerNow.blackjack ? Outcome::Push : Outcome::Win;
			result.confidence = (result.outcome == Outcome::Win) ? BlackjackHandEval::BettingConfidence::High : BlackjackHandEval::BettingConfidence::Low;
			return result;
		}

		bool dealerOutcomeTrustworthy = isLastSeatBeforeDealer || dealerNow.total >= 17;
		if (!dealerOutcomeTrustworthy)
			return result;

		PlayoutResult played = PlayHandOut(playerRanks, playerCount, dealerRanks, dealerCount, futureRanks, futureCount, canDouble, isLastSeatBeforeDealer);
		DealerSimResult dealerFinal = SimulateDealerFromRanks(dealerRanks, dealerCount, futureRanks + played.consumed, futureCount - played.consumed);

		result.trustworthy = true;
		result.outcome = CompareOutcome(played.value, dealerFinal.value);

		if (result.outcome == Outcome::Win)
			result.confidence = (played.consumed == 0) ? BlackjackHandEval::BettingConfidence::High : BlackjackHandEval::BettingConfidence::Medium;
		else
			result.confidence = BlackjackHandEval::BettingConfidence::Low; // push or loss

		return result;
	}
}
