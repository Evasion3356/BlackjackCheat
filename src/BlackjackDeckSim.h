#pragma once

// Pure, self-contained "pure cheat" decision engine for bjack_sp -- no
// dependency on ScriptHookRDR2/game memory, same separation-of-concerns
// rationale as BlackjackHandEval.h (one header
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
// BlackjackHandEval.h's own game-memory-free
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
// result, in bet-unit terms (Win=+1/Push=0/Loss=-1 per hand, doubled for
// a hand that doubled -- see the code-review addendum at the end), against
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
//
// Addendum -- the fallback's known-card override above only checked for
// an outright bust, which missed a live bug report: soft 18 (A,7), known
// next card 5, another seat still to act (so the fallback triggered) --
// basic strategy correctly says Hit for soft 18 against a strong dealer
// upcard, but the known next card (5) demotes the hand to a hard 13, not
// a bust, yet still strictly worse than the 18 already in hand. Fixed by
// widening the override to ALSO trigger on any known non-bust total
// DECREASE, not just a bust -- a strictly lower non-bust total can never
// compare better against any eventual dealer hand than the higher total
// already held, the same dealer-independent "higher non-bust total is
// never worse" guarantee DetermineCheatAction()'s own trustworthy-path
// tie-break already relies on. See
// tests/BlackjackDeckSimTests.cpp's "fallback known downgrade card
// overrides to Stand" case.
//
// Code-review addendum -- two more fixes, each pinned by a new test in
// tests/BlackjackDeckSimTests.cpp (numbered (1) and (3); (2) was a
// proposed change that was tried and rejected, kept here so it isn't
// retried):
// (1) The fallback's one-card lookahead above was itself too narrow: when
//     the fallback triggers, only the DEALER's cards are uncertain -- this
//     hand's own hits all come straight off the live cursor, so every one
//     of them is known, not just the first. Soft 16 (A,5) with known next
//     cards 6 then 5 was forced to Stand because A,5,6 is a hard 12, even
//     though A,5,6,5 is 21. detail::RefineFallbackWithOwnCards() now walks
//     every known stopping point instead, treating all totals <= 16 as
//     equally weak (the dealer always finishes on 17+ or busts, so 12 and
//     16 lose and win in exactly the same cases).
// (2) REJECTED: keeping the fewest hits among winning candidates instead
//     of the highest total. It looks safer (why hit a hand that already
//     wins?), but a standing "win" can depend entirely on the dealer
//     busting on the next card -- hard 11 vs a dealer 15 with a known 10
//     next "wins" by standing, where hitting to a known 21 wins without
//     depending on the dealer's draw at all. The highest-total tie-break
//     stays; see DetermineCheatAction()'s inline comment.
// (3) EvaluateSplit() scored every hand as +/-1, even one that doubled
//     (PlayHandOut() only doubles into a known Win), so it undervalued
//     exactly the lines it picked Double for. PlayoutResult now records
//     `doubled` and StakedOutcomeValue() weighs it as 2 units.

#include "BlackjackHandEval.h"

#include <cstdint>
#include <string_view>

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

	// Naturals: a dealer blackjack beats any player hand except a player
	// natural (push), and a player natural beats any dealer hand except a
	// dealer natural. A two-card 21 on a SPLIT hand isn't a natural -- pass
	// playerNaturalCounts=false for those. Without this, 21 vs a dealer
	// blackjack and a natural vs a three-card 21 both read as a push.
	inline Outcome CompareOutcome(const BlackjackHandEval::HandValue& player, const BlackjackHandEval::HandValue& dealer,
		bool playerNaturalCounts = true)
	{
		if (player.bust)
			return Outcome::Loss;
		const bool playerNatural = playerNaturalCounts && player.blackjack;
		if (dealer.blackjack)
			return playerNatural ? Outcome::Push : Outcome::Loss;
		if (playerNatural)
			return Outcome::Win;
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

	// ---- The game's own action legality for MY seat ----
	//
	// The Double and Split prompts (func_600) are only offered when
	// func_998/func_997 pass, and both require f_59 == 1 -- the seat hasn't
	// split. func_1237 on its own would accept a double after a split, but
	// the button never appears, so a split hand can't double. Live bug:
	// the mod advised Double on a split hand because it only checked the
	// card count and bankroll.
	inline bool CanDouble(std::int32_t cardCount, std::int32_t handsInPlay, std::int32_t bankroll, std::int32_t bet)
	{
		return cardCount == 2 && handsInPlay == 1 && bankroll >= bet;
	}

	inline bool CanSplit(const std::int32_t* ranks, std::int32_t cardCount, std::int32_t handsInPlay, std::int32_t bankroll, std::int32_t bet)
	{
		return cardCount == 2 && handsInPlay == 1 && ranks[0] == ranks[1] && bankroll >= bet;
	}

	// ---- The other seats' AI (func_1002 -> func_623) ----
	//
	// Every seat that isn't mine plays by func_1002: a pure function of
	// the hand's total (f_24, best total with Aces counted high when that
	// doesn't bust -- func_645), the dealer's up card rank, the card count
	// and whether the seat has split. Its 1-6s "thinking" timer only
	// delays WHEN it acts. Modeling it makes every seat's draws known, so
	// the dealer simulation no longer has to give up whenever a seat
	// after mine is still to act -- which was every round of the first
	// round log (seat 0, three AI seats).
	//
	// func_623's table, transcribed with a script (rows = total, columns =
	// dealer up card 2..14, J/Q/K/A as their own columns). H=Hit, S=Stand,
	// D=Double, P=Split; an empty row is func_623's default `return 0`,
	// which no live hand reaches. The pair rows are used only for an
	// unsplit 2-card pair the seat can afford to split (func_997), keyed
	// by the pair's total; a pair of Aces always splits (func_1002 checks
	// it before the table). The up card key is the dealer's VISIBLE card,
	// dealerRanks[1]: the round log's rounds 4 and 5 only replay with that
	// card (e.g. 16 vs a hole 8 would have hit; the seat stood on 16 vs
	// the visible 6).
	namespace detail
	{
		constexpr std::string_view kAiHardTable[22] = {
			"", "", "", "",
			"HHHHHHHHHHHHH", // 4
			"HHHHHHHHHHHHH", // 5
			"HHHHHHHHHHHHH", // 6
			"HHHHHHHHHHHHH", // 7
			"HHHHHHHHHHHHH", // 8
			"HDDDDHHHHHHHH", // 9
			"DDDDDDDDHHHHH", // 10
			"DDDDDDDDDDDDH", // 11
			"HHSSSHHHHHHHH", // 12
			"SSSSSHHHHHHHH", // 13
			"SSSSSHHHHHHHH", // 14
			"SSSSSHHHHHHHH", // 15
			"SSSSSHHHHHHHH", // 16
			"SSSSSSSSSSSSS", // 17
			"SSSSSSSSSSSSS", // 18
			"SSSSSSSSSSSSS", // 19
			"SSSSSSSSSSSSS", // 20
			"SSSSSSSSSSSSS", // 21
		};

		constexpr std::string_view kAiPairTable[22] = {
			"", "", "", "",
			"PPPPPPHHHHHHH", // 4: 2,2
			"",
			"HHHHHHHHHHHHH", // 6: 3,3
			"",
			"HHHPPHHHHHHHH", // 8: 4,4
			"",
			"DDDDDDDDHHHHH", // 10: 5,5
			"",
			"PPPPPHHHHHHHH", // 12: 6,6
			"",
			"PPPPPPHHHHHHH", // 14: 7,7
			"",
			"PPPPPPPPPPPPP", // 16: 8,8
			"",
			"PPPPPPPPSSSSS", // 18: 9,9
			"",
			"SSSSSSSSSSSSS", // 20: 10,10 / J,J / Q,Q / K,K
			"",
		};
	}

	inline BlackjackHandEval::Action AiTableAction(std::int32_t total, std::int32_t upcardRank, bool pairRow)
	{
		if (total < 0 || total > 21 || upcardRank < 2 || upcardRank > 14)
			return BlackjackHandEval::Action::Stand;
		const std::string_view row = (pairRow ? detail::kAiPairTable : detail::kAiHardTable)[total];
		if (row.empty())
			return BlackjackHandEval::Action::Stand;
		switch (row[upcardRank - 2])
		{
			case 'H': return BlackjackHandEval::Action::Hit;
			case 'D': return BlackjackHandEval::Action::Double;
			case 'P': return BlackjackHandEval::Action::Split;
			default: return BlackjackHandEval::Action::Stand;
		}
	}

	// func_1002 itself: the table plus its overrides. Double drops to Hit
	// once the hand has more than 2 cards, the seat can't cover a second
	// bet, or the seat has split.
	inline BlackjackHandEval::Action AiDecide(const std::int32_t* ranks, std::int32_t count, std::int32_t upcardRank,
		std::int32_t handsInPlay, bool canAffordSecondBet)
	{
		using BlackjackHandEval::Action;

		const std::int32_t total = BlackjackHandEval::EvaluateHand(ranks, count).total;
		const bool pairRow = count == 2 && handsInPlay == 1 && ranks[0] == ranks[1] && canAffordSecondBet;

		Action action = (pairRow && ranks[0] == 14) ? Action::Split : AiTableAction(total, upcardRank, pairRow);
		if (action == Action::Double && (count > 2 || !canAffordSecondBet || handsInPlay > 1))
			action = Action::Hit;
		return action;
	}

	constexpr std::int32_t kSeatCount = 4; // func_280's `iParam1 < 4`

	// A seat that still has to act, as the AI will play it: its hand so
	// far (2 cards if it hasn't started) and whether it can put up a
	// second bet.
	struct AiSeat
	{
		std::int32_t ranks[kHandMaxCards] = {};
		std::int32_t count = 0;
		bool canAffordSecondBet = true;
	};

	// Everything that draws between the hand being decided and the dealer.
	// `known` false means something in there can't be modeled (my own
	// later split hand, or a seat that has already split) -- callers then
	// fall back as they did before the AI model existed. The default is
	// "nothing else draws".
	struct SeatsAfter
	{
		bool known = true;
		std::int32_t count = 0;
		AiSeat seats[kSeatCount];

		static SeatsAfter Unknown()
		{
			SeatsAfter after;
			after.known = false;
			return after;
		}

		void Add(const std::int32_t* ranks, std::int32_t cardCount, bool canAffordSecondBet)
		{
			if (count >= kSeatCount)
			{
				known = false;
				return;
			}
			AiSeat& seat = seats[count++];
			seat.count = cardCount > kHandMaxCards ? kHandMaxCards : cardCount;
			for (std::int32_t i = 0; i < seat.count; i++)
				seat.ranks[i] = ranks[i];
			seat.canAffordSecondBet = canAffordSecondBet;
		}
	};

	inline SeatsAfter SeatsAfterFromFlag(bool isLastSeatBeforeDealer)
	{
		return isLastSeatBeforeDealer ? SeatsAfter{} : SeatsAfter::Unknown();
	}

	namespace detail
	{
		// One AI hand from `ranks`, drawing off future. Returns cards drawn.
		inline std::int32_t PlayAiHand(std::int32_t* ranks, std::int32_t count, std::int32_t upcardRank, std::int32_t handsInPlay,
			bool canAffordSecondBet, const std::int32_t* futureRanks, std::int32_t futureCount)
		{
			std::int32_t drawn = 0;
			while (count < kHandMaxCards && drawn < futureCount && !BlackjackHandEval::EvaluateHand(ranks, count).bust)
			{
				BlackjackHandEval::Action action = AiDecide(ranks, count, upcardRank, handsInPlay, canAffordSecondBet);
				if (action != BlackjackHandEval::Action::Hit && action != BlackjackHandEval::Action::Double)
					break;
				ranks[count++] = futureRanks[drawn++];
				if (action == BlackjackHandEval::Action::Double)
					break;
			}
			return drawn;
		}
	}

	// Plays one AI seat to the end of its turn. A split deals each new
	// hand its second card straight away (the same order EvaluateSplit()
	// models for my seat), then plays hand 1, then hand 2; split Aces get
	// that one card and nothing more. Returns how many of futureRanks the
	// seat drew.
	inline std::int32_t PlayAiSeat(const AiSeat& seat, std::int32_t upcardRank, const std::int32_t* futureRanks, std::int32_t futureCount)
	{
		std::int32_t ranks[kHandMaxCards];
		for (std::int32_t i = 0; i < seat.count; i++)
			ranks[i] = seat.ranks[i];

		if (seat.count == 2 && futureCount >= 2
			&& AiDecide(ranks, 2, upcardRank, 1, seat.canAffordSecondBet) == BlackjackHandEval::Action::Split)
		{
			std::int32_t hand1[kHandMaxCards] = { ranks[0], futureRanks[0] };
			std::int32_t hand2[kHandMaxCards] = { ranks[1], futureRanks[1] };
			std::int32_t drawn = 2;
			if (ranks[0] == 14)
				return drawn;
			drawn += detail::PlayAiHand(hand1, 2, upcardRank, 2, seat.canAffordSecondBet, futureRanks + drawn, futureCount - drawn);
			drawn += detail::PlayAiHand(hand2, 2, upcardRank, 2, seat.canAffordSecondBet, futureRanks + drawn, futureCount - drawn);
			return drawn;
		}

		return detail::PlayAiHand(ranks, seat.count, upcardRank, 1, seat.canAffordSecondBet, futureRanks, futureCount);
	}

	// The rest of the round after the hand being decided: every seat in
	// `after` plays off the deck in turn, then the dealer draws out. A
	// dealer already on 17+ never draws, so the seats can't change it and
	// aren't simulated. `drawn` counts the dealer's own cards only.
	inline DealerSimResult SimulateSeatsThenDealer(const std::int32_t* dealerRanks, std::int32_t dealerCount, const SeatsAfter& after,
		const std::int32_t* futureRanks, std::int32_t futureCount)
	{
		std::int32_t used = 0;
		if (BlackjackHandEval::EvaluateHand(dealerRanks, dealerCount).total < 17)
		{
			const std::int32_t upcardRank = dealerCount >= 2 ? dealerRanks[1] : dealerRanks[0];
			for (std::int32_t i = 0; i < after.count; i++)
				used += PlayAiSeat(after.seats[i], upcardRank, futureRanks + used, futureCount - used);
		}
		return SimulateDealerFromRanks(dealerRanks, dealerCount, futureRanks + used, futureCount - used);
	}

	namespace detail
	{
		// A non-bust total's strength against a dealer that's unknown but
		// guaranteed to finish on 17+ or bust (it stands on 17, see
		// SimulateDealerFromRanks): every total <= 16 loses to every
		// non-bust dealer hand and wins only when the dealer busts, so
		// they're all equivalent -- 12 is no worse than 16. Bust is worst.
		inline std::int32_t EffectiveStrength(const BlackjackHandEval::HandValue& value)
		{
			if (value.bust)
				return -1;
			return value.total < 16 ? 16 : value.total;
		}

		// DetermineCheatAction()'s fallback path (the dealer's draw-out
		// can't be trusted because another seat still acts first) --
		// basic strategy is blind to the deck, but THIS hand's own future
		// cards are still exact: it's this hand's turn, so every card it
		// hits draws the next card off the live cursor with nothing in
		// between, however many hits that is. So instead of peeking at
		// only the very next card, this walks every known "stand after k
		// more hits" stopping point and keeps the strongest one by
		// EffectiveStrength() (ties toward fewer hits). A strictly
		// stronger total is never worse against ANY eventual dealer hand
		// (CompareOutcome only looks at total/bust), so this needs no
		// dealer simulation at all.
		//
		// Live bug this replaces: the old one-card lookahead forced Stand
		// on soft 16 (A,5) with known next cards 6 then 5 -- A,5,6 is a
		// hard 12 (a "downgrade"), but A,5,6,5 is 21.
		//
		// - Strongest stopping point is k=0 and a known card busts or
		//   the known cards never beat the current hand -> Stand.
		// - Strongest stopping point is k>=1 -> Hit (even if basic
		//   strategy said Stand: a known improvement dominates), or
		//   Double when basic strategy already favors doubling here AND
		//   the single next card is itself the strongest stopping point.
		// - Ran out of known cards without busting (only happens with a
		//   near-empty deck) and nothing known beats the current hand:
		//   the cards beyond are unknown, so keep basic strategy's call
		//   unless the very next card is a known strict downgrade.
		inline BlackjackHandEval::Action RefineFallbackWithOwnCards(BlackjackHandEval::Action fallback,
			const std::int32_t* playerRanks, std::int32_t playerCount,
			const std::int32_t* futureRanks, std::int32_t futureCount)
		{
			std::int32_t ranks[kHandMaxCards];
			std::int32_t count = playerCount > kHandMaxCards ? kHandMaxCards : playerCount;
			for (std::int32_t i = 0; i < count; i++)
				ranks[i] = playerRanks[i];

			BlackjackHandEval::HandValue current = BlackjackHandEval::EvaluateHand(ranks, count);
			const std::int32_t currentStrength = EffectiveStrength(current);
			std::int32_t bestStrength = currentStrength;
			std::int32_t bestExtraHits = 0;
			std::int32_t oneHitStrength = -2; // -2 = no known next card
			bool exhausted = false;

			for (std::int32_t extraHits = 1; ; extraHits++)
			{
				std::int32_t futureIndex = extraHits - 1;
				if (futureIndex >= futureCount || count >= kHandMaxCards)
				{
					exhausted = true;
					break;
				}

				ranks[count] = futureRanks[futureIndex];
				count++;
				std::int32_t strength = EffectiveStrength(BlackjackHandEval::EvaluateHand(ranks, count));
				if (extraHits == 1)
					oneHitStrength = strength;
				if (strength < 0)
					break; // bust -- every later stopping point busts too
				if (strength > bestStrength)
				{
					bestStrength = strength;
					bestExtraHits = extraHits;
				}
			}

			if (bestExtraHits >= 1)
			{
				if (fallback == BlackjackHandEval::Action::Double && bestExtraHits == 1)
					return BlackjackHandEval::Action::Double;
				return BlackjackHandEval::Action::Hit;
			}

			if (!exhausted)
				return BlackjackHandEval::Action::Stand;

			if (oneHitStrength != -2 && oneHitStrength < currentStrength)
				return BlackjackHandEval::Action::Stand;
			return fallback;
		}
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
	//
	// AI-model addendum: `after` replaces the bool. The seats still to act
	// are played by the AI model (AiDecide()) before the dealer draws, so
	// the simulation is exact whenever every one of them is an AI seat --
	// the fallback below is now only for `after.known == false`. The bool
	// overload maps true to "nothing after me" and false to unknown.
	inline BlackjackHandEval::Action DetermineCheatAction(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isSplitAceHand, const SeatsAfter& after)
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
		bool dealerOutcomeTrustworthy = after.known || dealerKnown.total >= 17;

		if (!dealerOutcomeTrustworthy)
		{
			std::int32_t dealerUpcardRank = dealerCount >= 2 ? dealerRanks[1] : dealerRanks[0];
			BlackjackHandEval::Action fallback = BlackjackHandEval::GetBasicStrategyAction(playerRanks, playerCount, dealerUpcardRank, canDouble, /*canSplit*/ false, isSplitAceHand);
			return detail::RefineFallbackWithOwnCards(fallback, playerRanks, playerCount, futureRanks, futureCount);
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

			DealerSimResult dealerSim = SimulateSeatsThenDealer(dealerRanks, dealerCount, after, futureRanks + futureUsed, futureCount - futureUsed);
			Outcome outcome = CompareOutcome(value, dealerSim.value);
			std::int32_t rank = OutcomeRank(outcome);

			// Tie-break within the same outcome rank toward the HIGHER
			// total instead of keeping whichever candidate was evaluated
			// first -- see this file's own header comment for the bug
			// this specifically fixes. This deliberately applies to
			// winning candidates too: "fewest hits among wins" was tried
			// (code-review addendum) and rejected, because it would stand
			// a hard 11 whose standing "win" relies entirely on the dealer
			// busting on the next card, where hitting to a known 21 wins
			// without depending on the dealer's draw at all -- see
			// TestKnownWinningCardIsDouble.
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

	inline BlackjackHandEval::Action DetermineCheatAction(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isSplitAceHand, bool isLastSeatBeforeDealer)
	{
		return DetermineCheatAction(playerRanks, playerCount, dealerRanks, dealerCount, futureRanks, futureCount,
			canDouble, isSplitAceHand, SeatsAfterFromFlag(isLastSeatBeforeDealer));
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
		bool doubled = false;      // the hand doubled, so its outcome is worth 2 bet units, not 1
	};

	// Bet-unit value of a played-out hand's outcome: OutcomeValue()
	// scaled by the hand's stake (2 units once doubled). EvaluateSplit()
	// needs this -- scoring a doubled hand as +/-1 undervalues exactly
	// the lines the engine picks Double for.
	inline std::int32_t StakedOutcomeValue(Outcome outcome, const PlayoutResult& hand)
	{
		return OutcomeValue(outcome) * (hand.doubled ? 2 : 1);
	}

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
		bool canDouble, const SeatsAfter& after)
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
				futureRanks + result.consumed, futureCount - result.consumed, canDouble, /*isSplitAceHand*/ false, after);

			if (action == BlackjackHandEval::Action::Stand)
				break;

			ranks[count] = futureRanks[result.consumed];
			count++;
			result.consumed++;
			result.value = BlackjackHandEval::EvaluateHand(ranks, count);

			if (action == BlackjackHandEval::Action::Double)
			{
				result.doubled = true;
				break; // exactly one card, then forced stand
			}
		}

		return result;
	}

	inline PlayoutResult PlayHandOut(
		const std::int32_t* startRanks, std::int32_t startCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isLastSeatBeforeDealer)
	{
		return PlayHandOut(startRanks, startCount, dealerRanks, dealerCount, futureRanks, futureCount,
			canDouble, SeatsAfterFromFlag(isLastSeatBeforeDealer));
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
	// cursor. canDouble is the pair's own double-legality flag, used only
	// for the "don't split" line: a split hand can never double
	// (CanDouble() -- the game hides the Double button once f_59 > 1).
	// `after` is everything that draws after my seat (see SeatsAfter).
	inline SplitDecision EvaluateSplit(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, const SeatsAfter& after)
	{
		SplitDecision result;

		if (playerCount != 2 || futureCount < 2)
			return result;

		BlackjackHandEval::HandValue dealerKnown = BlackjackHandEval::EvaluateHand(dealerRanks, dealerCount);
		bool dealerOutcomeTrustworthy = after.known || dealerKnown.total >= 17;
		if (!dealerOutcomeTrustworthy)
			return result;

		std::int32_t pairRank = playerRanks[0];

		// Value of NOT splitting: play the pair as one ordinary hand.
		PlayoutResult noSplit = PlayHandOut(playerRanks, 2, dealerRanks, dealerCount, futureRanks, futureCount, canDouble, after);
		DealerSimResult dealerForNoSplit = SimulateSeatsThenDealer(dealerRanks, dealerCount, after, futureRanks + noSplit.consumed, futureCount - noSplit.consumed);
		std::int32_t noSplitValue = StakedOutcomeValue(CompareOutcome(noSplit.value, dealerForNoSplit.value), noSplit);

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
			hand1 = PlayHandOut(hand1Start, 2, dealerRanks, dealerCount, futureRanks + 2, futureCount - 2, /*canDouble*/ false, SeatsAfter::Unknown());

		std::int32_t hand2FutureOffset = 2 + hand1.consumed;
		PlayoutResult hand2;
		if (isAcePair)
			hand2.value = BlackjackHandEval::EvaluateHand(hand2Start, 2);
		else
			hand2 = PlayHandOut(hand2Start, 2, dealerRanks, dealerCount, futureRanks + hand2FutureOffset, futureCount - hand2FutureOffset, /*canDouble*/ false, after);

		std::int32_t dealerFutureOffset = hand2FutureOffset + hand2.consumed;
		DealerSimResult dealerForSplit = SimulateSeatsThenDealer(dealerRanks, dealerCount, after, futureRanks + dealerFutureOffset, futureCount - dealerFutureOffset);

		std::int32_t splitValue = StakedOutcomeValue(CompareOutcome(hand1.value, dealerForSplit.value, /*playerNaturalCounts*/ false), hand1)
			+ StakedOutcomeValue(CompareOutcome(hand2.value, dealerForSplit.value, /*playerNaturalCounts*/ false), hand2);

		result.trustworthy = true;
		result.shouldSplit = splitValue > noSplitValue;
		return result;
	}

	inline SplitDecision EvaluateSplit(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isLastSeatBeforeDealer)
	{
		return EvaluateSplit(playerRanks, playerCount, dealerRanks, dealerCount, futureRanks, futureCount,
			canDouble, SeatsAfterFromFlag(isLastSeatBeforeDealer));
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
		bool canDouble, const SeatsAfter& after)
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

		bool dealerOutcomeTrustworthy = after.known || dealerNow.total >= 17;
		if (!dealerOutcomeTrustworthy)
			return result;

		PlayoutResult played = PlayHandOut(playerRanks, playerCount, dealerRanks, dealerCount, futureRanks, futureCount, canDouble, after);
		DealerSimResult dealerFinal = SimulateSeatsThenDealer(dealerRanks, dealerCount, after, futureRanks + played.consumed, futureCount - played.consumed);

		result.trustworthy = true;
		result.outcome = CompareOutcome(played.value, dealerFinal.value);

		if (result.outcome == Outcome::Win)
			result.confidence = (played.consumed == 0) ? BlackjackHandEval::BettingConfidence::High : BlackjackHandEval::BettingConfidence::Medium;
		else
			result.confidence = BlackjackHandEval::BettingConfidence::Low; // push or loss

		return result;
	}

	inline BettingAdvice EvaluateBettingConfidence(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool isLastSeatBeforeDealer)
	{
		return EvaluateBettingConfidence(playerRanks, playerCount, dealerRanks, dealerCount, futureRanks, futureCount,
			canDouble, SeatsAfterFromFlag(isLastSeatBeforeDealer));
	}

	// The full per-hand advice decision BlackjackCheat.cpp's
	// DetermineAdvice() makes once it has read the live deck: forced
	// split-Ace Stand, then the split comparison (deck-derived when
	// trustworthy, the textbook pair chart otherwise), then
	// DetermineCheatAction(). Lives here, with no game dependency, so a
	// recorded "decision" line (RoundRecord.h) replays through exactly
	// the code the mod ran -- see tests/fixtures/rounds.jsonl.
	inline BlackjackHandEval::Action DetermineFullAdvice(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool canSplit, bool isSplitAceHand, const SeatsAfter& after)
	{
		if (isSplitAceHand)
			return BlackjackHandEval::Action::Stand; // forced by the game itself, see BlackjackHandEval.h's own header comment

		if (canSplit && playerCount == 2 && playerRanks[0] == playerRanks[1])
		{
			SplitDecision splitDecision = EvaluateSplit(playerRanks, playerCount, dealerRanks, dealerCount,
				futureRanks, futureCount, canDouble, after);

			if (splitDecision.trustworthy)
			{
				if (splitDecision.shouldSplit)
					return BlackjackHandEval::Action::Split;
				// else: deck-derived already answered "don't split" exactly -- fall through to the normal Hit/Stand/Double evaluation below, not the blind pair chart
			}
			else
			{
				std::int32_t dealerUpcardRank = dealerCount >= 2 ? dealerRanks[1] : dealerRanks[0];
				if (BlackjackHandEval::GetBasicStrategyAction(playerRanks, playerCount, dealerUpcardRank, canDouble, canSplit, false) == BlackjackHandEval::Action::Split)
					return BlackjackHandEval::Action::Split;
			}
		}

		return DetermineCheatAction(playerRanks, playerCount, dealerRanks, dealerCount,
			futureRanks, futureCount, canDouble, isSplitAceHand, after);
	}

	inline BlackjackHandEval::Action DetermineFullAdvice(
		const std::int32_t* playerRanks, std::int32_t playerCount,
		const std::int32_t* dealerRanks, std::int32_t dealerCount,
		const std::int32_t* futureRanks, std::int32_t futureCount,
		bool canDouble, bool canSplit, bool isSplitAceHand, bool isLastSeatBeforeDealer)
	{
		return DetermineFullAdvice(playerRanks, playerCount, dealerRanks, dealerCount, futureRanks, futureCount,
			canDouble, canSplit, isSplitAceHand, SeatsAfterFromFlag(isLastSeatBeforeDealer));
	}

	// The initial deal as func_1057 does it: 2 cards to each dealt seat in
	// ascending order, then 2 to the dealer ([0] is the hole card).
	struct InitialDeal
	{
		bool valid = false;
		std::int32_t seatCards[kSeatCount][2] = {};
		std::int32_t dealerRanks[2] = {};
		std::int32_t dealerDeckIndex = 0; // deck index of the dealer's first card
		std::int32_t cursor = 0;          // first undealt card
		bool dealerNatural = false;
	};

	inline InitialDeal DealRound(const std::int32_t* deckRanks, std::int32_t deckCount, const bool* seatDealt)
	{
		InitialDeal deal;
		for (std::int32_t seat = 0; seat < kSeatCount; seat++)
		{
			if (!seatDealt[seat])
				continue;
			if (deal.cursor + 2 > deckCount)
				return deal;
			deal.seatCards[seat][0] = deckRanks[deal.cursor];
			deal.seatCards[seat][1] = deckRanks[deal.cursor + 1];
			deal.cursor += 2;
		}
		if (deal.cursor + 2 > deckCount)
			return deal;
		deal.dealerRanks[0] = deckRanks[deal.cursor];
		deal.dealerRanks[1] = deckRanks[deal.cursor + 1];
		deal.dealerDeckIndex = deal.cursor;
		deal.cursor += 2;
		deal.dealerNatural = BlackjackHandEval::EvaluateHand(deal.dealerRanks, 2).blackjack;
		deal.valid = true;
		return deal;
	}

	// Does this seat take a turn at all? A natural never draws: func_718's
	// case 3 marks it done (f_3 = 1 == f_59) before case 4 starts anyone's
	// turn, and a dealer natural sends case 3 straight to case 8, so
	// nobody draws.
	inline bool SeatTakesTurn(const InitialDeal& deal, const bool* seatDealt, std::int32_t seat)
	{
		return seatDealt[seat] && !deal.dealerNatural && !BlackjackHandEval::EvaluateHand(deal.seatCards[seat], 2).blackjack;
	}

	inline AiSeat DealtAiSeat(const InitialDeal& deal, std::int32_t seat)
	{
		AiSeat ai;
		ai.count = 2;
		ai.ranks[0] = deal.seatCards[seat][0];
		ai.ranks[1] = deal.seatCards[seat][1];
		return ai;
	}

	// The pre-deal betting decision, made from nothing but the freshly
	// shuffled deck (deckRanks[0] is the first card dealt), which seats
	// will be dealt in, and my seat. Every other seat is an AI seat, so
	// the whole round plays out off the deck: the seats before mine by the
	// AI model, then my hand by the engine's own advice, then the seats
	// after mine and the dealer (EvaluateBettingConfidence()). AI seats are
	// assumed able to afford a second bet -- their bets aren't down yet.
	// Before the AI model this fell back to a textbook estimate whenever
	// another seat drew around mine -- every round of the first round log.
	// Its round 5 (18 vs 14, a sure loss) showed Medium and cost $402.
	// Returns Low if deckRanks doesn't even cover the initial deal.
	inline BlackjackHandEval::BettingConfidence EvaluatePreDealBetting(
		const std::int32_t* deckRanks, std::int32_t deckCount,
		const bool* seatDealt, std::int32_t mySeat)
	{
		using BlackjackHandEval::BettingConfidence;

		if (mySeat < 0 || mySeat >= kSeatCount || !seatDealt[mySeat])
			return BettingConfidence::Low;

		const InitialDeal deal = DealRound(deckRanks, deckCount, seatDealt);
		if (!deal.valid)
			return BettingConfidence::Low;

		const std::int32_t upcardRank = deal.dealerRanks[1];
		std::int32_t cursor = deal.cursor;
		for (std::int32_t seat = 0; seat < mySeat; seat++)
		{
			if (SeatTakesTurn(deal, seatDealt, seat))
				cursor += PlayAiSeat(DealtAiSeat(deal, seat), upcardRank, deckRanks + cursor, deckCount - cursor);
		}

		SeatsAfter after;
		for (std::int32_t seat = mySeat + 1; seat < kSeatCount; seat++)
		{
			if (SeatTakesTurn(deal, seatDealt, seat))
				after.Add(deal.seatCards[seat], 2, /*canAffordSecondBet*/ true);
		}

		const std::int32_t* myRanks = deal.seatCards[mySeat];
		BettingAdvice advice = EvaluateBettingConfidence(myRanks, 2, deal.dealerRanks, 2,
			deckRanks + cursor, deckCount - cursor, /*canDouble*/ true, after);
		if (advice.trustworthy)
			return advice.confidence;
		return BlackjackHandEval::EstimateBettingConfidence(myRanks, 2, upcardRank);
	}

	// Replays a finished round's dealer hand off its deck: the deal, every
	// other seat by the AI model, my seat as the myCardsDrawn cards it
	// actually took (total cards across my final hands minus the 2 dealt,
	// so a split's extra cards count too), then the dealer's draw-out. The
	// round log compares this with the dealer's real final hand every
	// round, so a mismatch means the deck read, the turn order or the AI
	// model is wrong. deckIndex[i] is where the dealer's i-th card sits in
	// the deck (for printing suits).
	struct ReplayedDealer
	{
		bool valid = false;
		std::int32_t ranks[kHandMaxCards] = {};
		std::int32_t deckIndex[kHandMaxCards] = {};
		std::int32_t count = 0;
	};

	inline ReplayedDealer ReplayDealer(const std::int32_t* deckRanks, std::int32_t deckCount,
		const bool* seatDealt, std::int32_t mySeat, std::int32_t myCardsDrawn)
	{
		ReplayedDealer result;
		const InitialDeal deal = DealRound(deckRanks, deckCount, seatDealt);
		if (!deal.valid)
			return result;

		const std::int32_t upcardRank = deal.dealerRanks[1];
		std::int32_t cursor = deal.cursor;
		for (std::int32_t seat = 0; seat < kSeatCount && cursor <= deckCount; seat++)
		{
			if (!SeatTakesTurn(deal, seatDealt, seat))
				continue;
			if (seat == mySeat)
				cursor += myCardsDrawn;
			else
				cursor += PlayAiSeat(DealtAiSeat(deal, seat), upcardRank, deckRanks + cursor, deckCount - cursor);
		}
		if (cursor > deckCount)
			return result;

		result.ranks[0] = deal.dealerRanks[0];
		result.ranks[1] = deal.dealerRanks[1];
		result.deckIndex[0] = deal.dealerDeckIndex;
		result.deckIndex[1] = deal.dealerDeckIndex + 1;
		result.count = 2;
		while (BlackjackHandEval::EvaluateHand(result.ranks, result.count).total < 17 && result.count < kHandMaxCards && cursor < deckCount)
		{
			result.deckIndex[result.count] = cursor;
			result.ranks[result.count++] = deckRanks[cursor++];
		}
		result.valid = true;
		return result;
	}
}
