// Unit tests for src/BlackjackDeckSim.h -- links against the SAME header
// the mod itself uses (not a hand-copied duplicate), same rationale as
// tests/BlackjackHandEvalTests.cpp.
//
// Build & run (from the BlackjackCheat directory):
//   MSBuild.exe tests\BlackjackDeckSimTests.vcxproj /p:Configuration=Debug /p:Platform=x64
//   bin\Debug\BlackjackDeckSimTests.exe
// Exits 0 and prints "ALL PASS" if every case passes, exits 1 and lists
// failures otherwise.

#include "../src/BlackjackDeckSim.h"
#include "../src/RoundRecord.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
	using BlackjackHandEval::HandValue;
	using BlackjackHandEval::EvaluateHand;
	using Action = BlackjackHandEval::Action;
	using BlackjackDeckSim::DetermineCheatAction;
	using BlackjackDeckSim::SimulateDealerFromRanks;
	using BlackjackDeckSim::Outcome;
	using BlackjackDeckSim::CompareOutcome;
	using BlackjackDeckSim::OutcomeRank;
	using BlackjackDeckSim::EvaluateSplit;
	using BlackjackDeckSim::PlayHandOut;
	using BlackjackDeckSim::SplitDecision;

	int g_failures = 0;

	void Check(bool condition, const char* testName, const char* detail)
	{
		if (condition)
		{
			std::printf("  [PASS] %s\n", testName);
		}
		else
		{
			std::printf("  [FAIL] %s -- %s\n", testName, detail);
			g_failures++;
		}
	}

	// The exact live bug report this header was written to fix: 3D 8S
	// (hard 11) with a known next card of 2S. The dealer already stands
	// pat at 19 (no further draws needed either way), so standing at 11
	// is a Loss and hitting to 13 is STILL a Loss -- but 13 is strictly
	// safer/better than 11 (a known, bust-proof improvement), so the
	// engine must recommend Hit, not Stand. The original (buggy) version
	// of this algorithm only ever updated its "best" candidate on a
	// STRICTLY higher outcome rank, so the first candidate checked
	// (stand now, extraHits=0) always won ties against any later
	// same-rank candidate -- this test fails against that version and
	// passes against the fixed tie-break-toward-higher-total version.
	void TestHardElevenAlwaysHits()
	{
		std::printf("TestHardElevenAlwaysHits:\n");

		std::int32_t player[2] = { 3, 8 }; // 3D 8S = hard 11
		std::int32_t dealer[2] = { 10, 9 }; // already 19, stands pat regardless of what the player does
		std::int32_t future[1] = { 2 }; // 2S -- the known next card from the bug report

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, /*canDouble*/ true, /*isSplitAceHand*/ false, /*isLastSeatBeforeDealer*/ true);
		Check(action == Action::Hit, "hard 11 with a known safe next card is Hit, never Stand",
			"11+2=13 can never bust and is strictly better than standing at 11, even though both still lose to a dealer 19");
	}

	// Sanity control for the above: with NO further cards available at
	// all, the engine has nothing to compare against extraHits=0 and must
	// still return Stand rather than looping forever or crashing.
	void TestNoFutureCardsStands()
	{
		std::printf("TestNoFutureCardsStands:\n");

		std::int32_t player[3] = { 7, 7, 7 }; // already 21
		std::int32_t dealer[2] = { 10, 9 }; // 19, already loses to 21
		std::int32_t future[1] = { 0 }; // unused -- futureCount is 0, so this must never actually be read

		Action action = DetermineCheatAction(player, 3, dealer, 2, future, 0, true, false, /*isLastSeatBeforeDealer*/ true);
		Check(action == Action::Stand, "already-winning hand with no future cards left is Stand",
			"no cards to peek at means extraHits can only ever be 0");
	}

	// A known next card that turns a mediocre 2-card 11 into a winning 21
	// (11+10=21) should recommend Double, not just Hit -- taking exactly
	// one more card is legal (2-card hand) and is verifiably the single
	// best stopping point (a 3rd hit would bust: 21+3=24).
	void TestKnownWinningCardIsDouble()
	{
		std::printf("TestKnownWinningCardIsDouble:\n");

		std::int32_t player[2] = { 5, 6 }; // hard 11
		std::int32_t dealer[2] = { 10, 5 }; // 15, must hit at least once
		std::int32_t future[2] = { 10, 3 }; // player hit -> 21; dealer's own subsequent draw -> 18

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 2, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ true);
		Check(action == Action::Double, "a known card that makes 21 on a 2-card 11 is Double",
			"exactly one more card wins outright (21 vs dealer's resulting 18) and no further hit could do better");

		Action actionNoDouble = DetermineCheatAction(player, 2, dealer, 2, future, 2, /*canDouble*/ false, false, /*isLastSeatBeforeDealer*/ true);
		Check(actionNoDouble == Action::Hit, "same scenario with canDouble=false falls back to Hit",
			"Double must never be recommended when it isn't actually legal");
	}

	// isSplitAceHand must force Stand unconditionally -- the game itself
	// never re-offers hit/stand/double after a split Ace's single forced
	// card, regardless of how good a known next card would otherwise look
	// (same rule BlackjackHandEval::GetBasicStrategyAction() already
	// enforces for the textbook path -- this engine needs its own check
	// since it doesn't call that function at all once past the split
	// decision).
	void TestSplitAceHandForcesStand()
	{
		std::printf("TestSplitAceHandForcesStand:\n");

		std::int32_t player[2] = { 14, 2 }; // A,2 = soft 13
		std::int32_t dealer[2] = { 10, 9 }; // 19
		std::int32_t future[1] = { 9 }; // would otherwise be a fine hit (13+9 -> 21 with ace demotion)

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, /*isSplitAceHand*/ true, /*isLastSeatBeforeDealer*/ true);
		Check(action == Action::Stand, "split-Ace hand is always Stand, even with a known winning next card",
			"the game never actually offers this hand another action, so recommending Hit would describe a choice that can't happen");
	}

	// Standing pat on an already-winning hand when the only available
	// next card would bust must stay Stand -- the bust candidate is
	// discarded entirely (never displaces the extraHits=0 baseline).
	void TestBustingCardIsRejected()
	{
		std::printf("TestBustingCardIsRejected:\n");

		std::int32_t player[2] = { 10, 10 }; // hard 20, already beats the dealer below
		std::int32_t dealer[2] = { 10, 9 }; // 19, already stands pat -- doesn't need to draw either way
		std::int32_t future[1] = { 5 }; // hitting busts (20+5=25)

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, false, /*isLastSeatBeforeDealer*/ true);
		Check(action == Action::Stand, "a hand that would bust on the only known next card stays Stand",
			"extraHits=1 busts and must never be recorded as the new best, even though extraHits=0 already wins outright");
	}

	// The exact live bug report isLastSeatBeforeDealer was added to fix:
	// a hard 9 (5,4 -- mathematically cannot bust on any single next
	// card) was advised Stand. Reproduced here: dealer shows a hand that
	// must hit (16), and the very next undrawn card (6) would bust it
	// (16+6=22) -- so standing "steals" nothing (the dealer draws that
	// exact card next and busts, an automatic win regardless of the
	// player's own low total) while hitting would consume that exact
	// card FOR the player instead, letting the dealer draw a safe card
	// (4) afterward and beat the player's now-higher-but-still-losing
	// total every time (15 vs 20, then 19 vs 20, then busting on a third
	// hit) -- so the simulation-trusting version of this function
	// legitimately (and, if it really were the last seat before the
	// dealer, CORRECTLY) recommends Stand.
	void TestHardNineDeniedDealerBust()
	{
		std::printf("TestHardNineDeniedDealerBust:\n");

		std::int32_t player[2] = { 5, 4 }; // hard 9 -- cannot bust on any single card
		std::int32_t dealer[2] = { 10, 6 }; // 16, must hit
		std::int32_t future[9] = { 6, 4, 4, 4, 4, 4, 4, 4, 4 }; // 6 would bust the dealer; every card after that is a harmless 4

		Action lastSeat = DetermineCheatAction(player, 2, dealer, 2, future, 9, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ true);
		Check(lastSeat == Action::Stand, "denying the dealer's own bust card is correct Stand -- but ONLY when genuinely last to act",
			"if nothing else can draw before the dealer, standing hands the dealer their exact bust card (16+6=22) -- a real, verifiable win no amount of hitting recovers (15/19 vs the dealer's resulting 20, then a bust on the third hit)");

		// The actual live bug: the user was NOT last to act that round --
		// another occupied seat still had to play before the dealer, so
		// the "dealer's next card" assumption above never held. This
		// must no longer trust the dealer simulation, and must never
		// Stand. Basic strategy alone says Double (hard 9 vs up-card 6),
		// but this hand's own next cards are still exact: doubling takes
		// only the 6 (a weak 15), while two hits reach 9+6+4=19 -- so the
		// fallback's own-card refinement demotes Double to Hit.
		Action notLastSeat = DetermineCheatAction(player, 2, dealer, 2, future, 9, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ false);
		Check(notLastSeat == Action::Hit, "the same hand with another seat still to act never Stands, and doesn't Double into a known weak 15",
			"the dealer simulation isn't trusted here, but this hand's own known cards are: Double stops at 15, two hits reach 19");

		Action notLastSeatNoDouble = DetermineCheatAction(player, 2, dealer, 2, future, 9, /*canDouble*/ false, false, /*isLastSeatBeforeDealer*/ false);
		Check(notLastSeatNoDouble == Action::Hit, "same fallback with canDouble=false is Hit, still never Stand",
			"basic strategy demotes Double to Hit when doubling isn't legal, but a bust-proof 9 must always at least Hit");
	}

	// Session 10 live bug report #1: hard 12 (K,2) with a known next card
	// of King (a certain bust: 12+10=22) was advised Hit. The dealer here
	// still needs to hit (14, below 17) and another seat is still to
	// act, so this correctly falls into the basic-strategy fallback --
	// but the fallback must still never recommend drawing a card already
	// known to bust this hand, regardless of what basic strategy itself
	// would have said blind to that card.
	void TestKnownBustCardOverridesFallback()
	{
		std::printf("TestKnownBustCardOverridesFallback:\n");

		std::int32_t player[2] = { 13, 2 }; // K,2 = hard 12
		std::int32_t dealer[2] = { 2, 3 }; // 5, must hit -- upcard 3 is outside basic strategy's 4-6 "Stand on 12" window, so textbook alone says Hit
		std::int32_t future[1] = { 13 }; // King -- the known next card from the bug report, 12+10=22 bust

		BlackjackHandEval::Action textbook = BlackjackHandEval::GetBasicStrategyAction(player, 2, dealer[1], true, false, false);
		Check(textbook == Action::Hit, "sanity check: plain textbook strategy (blind to the known card) says Hit here",
			"hard 12 vs dealer upcard 3 is a textbook Hit, needed as the baseline this test overrides");

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ false);
		Check(action == Action::Stand, "a known bust card overrides the fallback's own Hit suggestion to Stand",
			"12+King=22 is a certain bust regardless of what basic strategy says blind to that card, and regardless of other seats -- it's this hand's own turn, so the next card is exact either way");
	}

	// Session 10 live bug report #2: hard 19 against a dealer already
	// showing 20 (already >=17, stands pat) with a known next card of 2
	// (an outright double to 21) was advised Stand. Another seat still
	// had to act, which used to force the fallback unconditionally -- but
	// the dealer here was never going to draw a card at all, so no other
	// seat's interference could possibly change its outcome. The engine
	// must trust its own simulation here even with isLastSeatBeforeDealer
	// false.
	void TestDealerAlreadyPatTrustsSimRegardlessOfSeats()
	{
		std::printf("TestDealerAlreadyPatTrustsSimRegardlessOfSeats:\n");

		std::int32_t player[2] = { 9, 10 }; // hard 19
		std::int32_t dealer[2] = { 10, 10 }; // already 20, stands pat -- never draws regardless of any other seat
		std::int32_t future[1] = { 2 }; // known next card -- 19+2=21, beats the dealer's 20

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ false);
		Check(action == Action::Double, "a dealer already standing pat at 20 is trusted even when another seat is still to act",
			"the dealer's own hand is already >=17 and will never touch the future deck, so no other seat's draws can change this comparison -- 19 vs 20 is a Loss but 21 vs 20 is a Win, an outright Double");
	}

	// The exact live bug report EvaluateSplit() was written to fix: J,J
	// (hard 20 -- basic strategy's chart never splits tens) with known
	// upcoming cards of Ace, then 2, then 7. Splitting deals the Ace to
	// hand 1 (J,A=21) and the 2 to hand 2 (J,2=12, needing the known 7 to
	// reach 19) -- both beat a dealer already standing pat at 17, so
	// splitting into two winning hands is worth strictly more (+2 bet
	// units) than the single hand alone (a known Ace lets the engine
	// double the un-split pair to a winning 21 too, worth only +1).
	void TestSplitDeckDerivedFindsProfitableSplit()
	{
		std::printf("TestSplitDeckDerivedFindsProfitableSplit:\n");

		std::int32_t player[2] = { 11, 11 }; // J,J = hard 20
		std::int32_t dealer[2] = { 10, 7 }; // already 17, stands pat regardless of any other seat
		std::int32_t future[3] = { 14, 2, 7 }; // Ace -> hand 1's card, 2 -> hand 2's card, 7 -> hand 2's known hit

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 3, /*canDoubleAfterSplit*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(decision.trustworthy, "dealer already pat at 17 makes the split comparison trustworthy even with another seat still to act",
			"the dealer never draws from here regardless of other seats, same rule DetermineCheatAction() already uses");
		// Split hands can't double (TestSplitHandsNeverDouble), so J,2
		// hits the 7 for a single-unit 19: 21 + 19 is +2, the same as
		// doubling the un-split pair into the Ace -- a tie, no split. (This
		// used to expect +3 from doubling J,2, which the game never offers.)
		Check(!decision.shouldSplit, "with doubling allowed, splitting these tens only ties doubling the pair into the Ace",
			"21 + a hit-to-19 (+2) vs a doubled 21 (+2)");

		// With doubling off, the un-split pair can only Hit its way to
		// that 21 (+1), so splitting (+2) still wins -- the original live
		// bug report's comparison.
		SplitDecision noDouble = EvaluateSplit(player, 2, dealer, 2, future, 3, /*canDouble*/ false, /*isLastSeatBeforeDealer*/ false);
		Check(noDouble.trustworthy && noDouble.shouldSplit, "the same split without doubling is still +2 versus +1",
			"21 + 19 beats a single hit-to-21");
	}

	// Control for the above: same pair, but the known future cards don't
	// make splitting worth anything -- against a dealer natural 21,
	// nothing the player does can do better than lose, and splitting
	// merely risks losing TWICE (two bets) instead of once.
	void TestSplitDeckDerivedRejectsWhenNotProfitable()
	{
		std::printf("TestSplitDeckDerivedRejectsWhenNotProfitable:\n");

		std::int32_t player[2] = { 11, 11 }; // J,J = hard 20
		std::int32_t dealer[2] = { 14, 10 }; // dealer natural 21, unbeatable by anything below 21
		std::int32_t future[3] = { 5, 5, 5 }; // no card here turns either hand into 21

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 3, /*canDoubleAfterSplit*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(decision.trustworthy, "dealer already pat at a natural 21 is trustworthy regardless of other seats",
			"same short-circuit as the profitable-split case above");
		Check(!decision.shouldSplit, "splitting into two guaranteed losers instead of one is strictly worse, must not be recommended",
			"-1 for standing pat beats -2 for losing both split hands");
	}

	// When the dealer still needs to hit AND another seat is still to
	// act, EvaluateSplit() can't trust its own comparison at all (same
	// precondition as DetermineCheatAction()) -- trustworthy must come
	// back false so the caller knows to fall back to the textbook pair
	// chart instead of reading shouldSplit as a real answer.
	void TestSplitDeckDerivedNotTrustworthyDefersToCaller()
	{
		std::printf("TestSplitDeckDerivedNotTrustworthyDefersToCaller:\n");

		std::int32_t player[2] = { 11, 11 };
		std::int32_t dealer[2] = { 2, 3 }; // 5, must hit -- not yet 17
		std::int32_t future[3] = { 14, 2, 7 };

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 3, /*canDoubleAfterSplit*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(!decision.trustworthy, "a dealer that still needs to hit, with another seat still to act, can't be trusted",
			"nothing downstream of the dealer's own draw is exact here -- the caller must use the textbook pair chart instead");
	}

	// ------------------------------------------------------------------
	// Session 12 addendum -- a broad, hand-derived scenario sweep, added
	// after a third live bug report in a row (J,J advised Stand instead
	// of Split -- see the Session 11 tests above) prompted a request to
	// stop reacting to individual bug reports one at a time and instead
	// verify the WHOLE decision engine against a wide spread of concrete
	// hand/next-card combinations. This is not a rewrite of
	// DetermineCheatAction()/EvaluateSplit() -- both were already fully
	// deterministic (every decision here comes from real, known future
	// cards, never a probability or a running count; card counting
	// was removed from the advice path entirely in Session
	// 7) -- it's proof of that claim across enough distinct situations to
	// trust it, rather than just asserting it. Every expected outcome
	// below was hand-derived from the exact same card arithmetic
	// BlackjackHandEval::EvaluateHand() uses (CardValue, ace demotion)
	// and then confirmed by actually running these tests, not assumed.
	// ------------------------------------------------------------------

	// User-reported baseline #1: hard 18, next card 3 -> Hit (18+3=21, a
	// safe improvement); next card 4 -> Stand (18+4=22 would bust). Using
	// a 3-card 18 (10,5,3) rather than a 2-card one so canDouble is moot
	// here -- this test is purely about the hit/stand line, not double.
	void TestHardEighteenHitsOnThreeStandsOnFour()
	{
		std::printf("TestHardEighteenHitsOnThreeStandsOnFour:\n");

		std::int32_t player[3] = { 10, 5, 3 }; // hard 18
		std::int32_t dealer[2] = { 10, 10 }; // already 20, stands pat
		std::int32_t futureHit[1] = { 3 };
		std::int32_t futureStand[1] = { 4 };

		Action hitAction = DetermineCheatAction(player, 3, dealer, 2, futureHit, 1, false, false, true);
		Check(hitAction == Action::Hit, "hard 18 with a known 3 next is Hit", "18+3=21 beats the dealer's 20, a safe known improvement");

		Action standAction = DetermineCheatAction(player, 3, dealer, 2, futureStand, 1, false, false, true);
		Check(standAction == Action::Stand, "hard 18 with a known 4 next is Stand", "18+4=22 is a certain bust, must never be recommended");
	}

	// A safe improvement is still worth taking even when it doesn't flip
	// the outcome category -- hard 15 -> 17 against a dealer already
	// pat at 19 loses either way, but 17 is strictly safer/better than
	// 15 (same reasoning as the existing "hard 11 always hits" case,
	// different numbers).
	void TestHardFifteenHitsToSeventeenEvenThoughStillLosing()
	{
		std::printf("TestHardFifteenHitsToSeventeenEvenThoughStillLosing:\n");

		std::int32_t player[3] = { 5, 5, 5 }; // hard 15
		std::int32_t dealer[2] = { 10, 9 }; // 19, stands pat
		std::int32_t future[1] = { 2 };

		Action action = DetermineCheatAction(player, 3, dealer, 2, future, 1, false, false, true);
		Check(action == Action::Hit, "hard 15 with a known safe 2 next is Hit even though 17 still loses to 19",
			"15+2=17 can never bust and is strictly better than 15, even though both lose to a dealer 19");
	}

	// User-reported baseline #2: hard 20 against a dealer already at 19
	// -- any non-Ace next card busts (20+anything>=2 is >=22), so it must
	// Stand. But the ONE card that doesn't bust a 20 is an Ace (counted
	// as 1), which turns 20 into 21 -- still the same Win category, but
	// worth strictly more in bet-unit profit if doubled, so a 2-card
	// hand with canDouble legal should switch to Double specifically on
	// a known Ace, and a hand that CAN'T double (3+ cards) should still
	// Hit rather than Stand.
	void TestHardTwentyStandsExceptDoublesOnKnownAce()
	{
		std::printf("TestHardTwentyStandsExceptDoublesOnKnownAce:\n");

		std::int32_t player2[2] = { 10, 10 }; // hard 20, 2 cards
		std::int32_t dealer[2] = { 10, 9 }; // 19, stands pat
		std::int32_t futureBust[1] = { 5 };
		std::int32_t futureAce[1] = { 14 };

		Action standAction = DetermineCheatAction(player2, 2, dealer, 2, futureBust, 1, true, false, true);
		Check(standAction == Action::Stand, "hard 20 with any known non-Ace next card is Stand", "20+5=25 is a certain bust");

		Action doubleAction = DetermineCheatAction(player2, 2, dealer, 2, futureAce, 1, true, false, true);
		Check(doubleAction == Action::Double, "hard 20 with a known next Ace and canDouble legal is Double",
			"20+Ace(as 1)=21 never busts and is still a Win, so doubling banks strictly more profit on the same guaranteed win");

		std::int32_t player3[3] = { 5, 5, 10 }; // hard 20, 3 cards -- double never legal here
		Action hitInsteadOfDouble = DetermineCheatAction(player3, 3, dealer, 2, futureAce, 1, true, false, true);
		Check(hitInsteadOfDouble == Action::Hit, "the same known Ace with Double illegal (3+ cards) falls back to Hit, never Stand",
			"the underlying improvement (20 -> 21) is still real even when the bet can't be doubled on it");
	}

	// Soft totals: a soft 18 (A,3,4) with a known 3 next reaches a hard
	// 21 -- still worth hitting into even against a dealer that already
	// beats 18, since 21 flips Loss to Win. This uses a 3-card soft 18
	// specifically so canDouble is moot (isolating the hit/stand call).
	void TestSoftEighteenHitsIntoAWinningTwentyOne()
	{
		std::printf("TestSoftEighteenHitsIntoAWinningTwentyOne:\n");

		std::int32_t player[3] = { 14, 3, 4 }; // A,3,4 = soft 18
		std::int32_t dealer[2] = { 10, 10 }; // 20, stands pat
		std::int32_t future[1] = { 3 };

		Action action = DetermineCheatAction(player, 3, dealer, 2, future, 1, false, false, true);
		Check(action == Action::Hit, "soft 18 with a known 3 next is Hit", "A,3,4,3 = hard 21 beats the dealer's 20; standing at 18 loses");
	}

	// The trickier soft-total case: a 2-card soft 18 (A,7) with a known
	// 6 next DEMOTES both the existing ace-as-11 AND doesn't actually
	// help -- A,7,6 forces the ace down to 1, landing on a hard 14,
	// which is WORSE than the original 18 even though it isn't a bust.
	// The engine must recognize this isn't an improvement (same Loss
	// category, lower total) and Stand, never Hit or Double, on a card
	// that looks safe (no bust) but is actually a downgrade.
	void TestSoftEighteenStandsWhenNextCardOnlyLowersTotal()
	{
		std::printf("TestSoftEighteenStandsWhenNextCardOnlyLowersTotal:\n");

		std::int32_t player[2] = { 14, 7 }; // A,7 = soft 18
		std::int32_t dealer[2] = { 10, 10 }; // 20, stands pat
		std::int32_t future[1] = { 6 };

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, false, true);
		Check(action == Action::Stand, "soft 18 with a known 6 next is Stand, not Hit or Double",
			"A,7,6 demotes to a hard 14 -- not a bust, but strictly worse than the 18 already in hand, so it must never be taken");
	}

	// A push is still worth trying to upgrade to a win when a known safe
	// card would do it, and still worth protecting from a known busting
	// card -- two sides of the same "don't just look at Win/Loss/Push
	// categories in isolation, use the actual total" principle.
	void TestPushCanBeUpgradedOrMustBeProtected()
	{
		std::printf("TestPushCanBeUpgradedOrMustBeProtected:\n");

		std::int32_t player[3] = { 6, 6, 6 }; // hard 18
		std::int32_t dealer[2] = { 9, 9 }; // 18, stands pat -- a push if the player also stands at 18
		std::int32_t futureWin[1] = { 3 };
		std::int32_t futureBust[1] = { 9 };

		Action upgrade = DetermineCheatAction(player, 3, dealer, 2, futureWin, 1, false, false, true);
		Check(upgrade == Action::Hit, "a push at 18 with a known 3 next is Hit", "18+3=21 turns a push into an outright win");

		Action protect = DetermineCheatAction(player, 3, dealer, 2, futureBust, 1, false, false, true);
		Check(protect == Action::Stand, "the same push with a known busting 9 next is Stand", "18+9=27 would turn a push into a loss -- never worth the risk");
	}

	// Fallback path (another seat still to act, dealer below 17): basic
	// strategy alone would already say Hit here (hard 12 vs. dealer
	// upcard 3 is outside the "stand on 12 vs 4-6" window) -- confirms
	// the fallback doesn't ALSO suppress a perfectly safe Hit just
	// because it's operating half-blind. Paired with the Session 11
	// "known bust card overrides fallback" test, this covers both
	// directions of the fallback's own correctness.
	void TestFallbackStillHitsWhenTheKnownCardIsSafe()
	{
		std::printf("TestFallbackStillHitsWhenTheKnownCardIsSafe:\n");

		std::int32_t player[2] = { 10, 2 }; // hard 12
		std::int32_t dealer[2] = { 3, 7 }; // 10, must hit -- upcard 7 is a textbook Hit for hard 12
		std::int32_t future[1] = { 5 }; // 12+5=17, safe

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, false, false);
		Check(action == Action::Hit, "the fallback still recommends Hit when the known next card is safe",
			"basic strategy already says Hit here (dealer upcard 7), and 12+5=17 doesn't bust, so nothing should suppress it");
	}

	// Same fallback path, a different pair-of-cards flavor of the
	// Session 11 bust-override bug (Queen instead of King, to confirm
	// the fix isn't coincidentally tied to one specific rank).
	void TestFallbackBustOverrideWithAQueen()
	{
		std::printf("TestFallbackBustOverrideWithAQueen:\n");

		std::int32_t player[2] = { 12, 2 }; // Q,2 = hard 12
		std::int32_t dealer[2] = { 2, 3 }; // 5, must hit -- upcard 3 is a textbook Hit for hard 12
		std::int32_t future[1] = { 12 }; // Queen -- 12+10=22, a certain bust

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, false, false);
		Check(action == Action::Stand, "a known bust card (Queen) overrides the fallback's Hit suggestion, same as the King case",
			"12+Queen=22 is a certain bust regardless of rank -- this isn't a King-specific fix");
	}

	// Live bug report: soft 18 (A,7) with another seat still to act (so
	// the fallback path triggers) and a known next card of 5 was advised
	// Hit, which turned the hand into a hard 13 -- not a bust, but
	// strictly worse than the 18 already in hand, and the fallback's
	// bust-only override let it through untouched. A,7,5 demotes the ace
	// (11+7+5=23 -> 1+7+5=13), so this must now override to Stand, the
	// same way a known bust card already did.
	void TestFallbackKnownDowngradeCardOverridesToStand()
	{
		std::printf("TestFallbackKnownDowngradeCardOverridesToStand:\n");

		std::int32_t player[2] = { 14, 7 }; // A,7 = soft 18
		std::int32_t dealer[2] = { 2, 10 }; // 12, must hit -- upcard 10 is a textbook Hit for soft 18
		std::int32_t future[1] = { 5 }; // A,7,5 -> hard 13, not a bust but a certain downgrade from 18

		BlackjackHandEval::Action textbook = BlackjackHandEval::GetBasicStrategyAction(player, 2, dealer[1], true, false, false);
		Check(textbook == Action::Hit, "sanity check: plain textbook strategy (blind to the known card) says Hit here",
			"soft 18 vs dealer upcard 10 is a textbook Hit, needed as the baseline this test overrides");

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ false);
		Check(action == Action::Stand, "a known non-bust downgrade card overrides the fallback's Hit suggestion to Stand",
			"A,7,5=13 never busts but is strictly worse than the 18 already in hand, regardless of what basic strategy says blind to that card, and regardless of other seats -- it's this hand's own turn, so the next card is exact either way");
	}

	// A dealer already standing pat is trusted even with another seat
	// still to act -- same rule as the Session 11 test, but against a
	// dealer 18 instead of 20 (a Double, not just a Hit), to confirm the
	// rule isn't coincidentally tied to one specific dealer total.
	void TestDealerPatAtEighteenTrustsDoubleRegardlessOfOtherSeats()
	{
		std::printf("TestDealerPatAtEighteenTrustsDoubleRegardlessOfOtherSeats:\n");

		std::int32_t player[2] = { 9, 10 }; // hard 19
		std::int32_t dealer[2] = { 9, 9 }; // already 18, stands pat
		std::int32_t future[1] = { 2 }; // 19+2=21

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, false, false);
		Check(action == Action::Double, "a dealer already pat at 18 is trusted for Double even with another seat still to act",
			"19 already beats 18, but 21 beats it more profitably if doubled -- the dealer's own hand can't change regardless of other seats");
	}

	// The dealer's own draw shares the SAME future cursor as the
	// player's potential hit -- if the player stands, the dealer gets
	// the very next card; if the player hits, the PLAYER gets it
	// instead and the dealer draws from further down. Here hitting would
	// bust (15+10=25), so the only option is Stand, even though that
	// hands the dealer a card (10) that brings it to a made 17 the
	// player then loses to -- a real, unavoidable loss, not a bug.
	void TestDealerDrawSharesTheSameCursorAsAPlayerHit()
	{
		std::printf("TestDealerDrawSharesTheSameCursorAsAPlayerHit:\n");

		std::int32_t player[2] = { 5, 10 }; // hard 15
		std::int32_t dealer[2] = { 2, 5 }; // 7, must hit
		std::int32_t future[2] = { 10, 4 }; // if the player stands, the dealer draws this 10 next (7+10=17, stop)

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 2, true, false, true);
		Check(action == Action::Stand, "hitting into a certain bust is still wrong even though standing hands the dealer a card that makes 17",
			"15+10=25 busts outright -- there's no safe alternative to standing here, regardless of what the dealer ends up drawing");
	}

	// Defensive/edge case: the fallback path with ZERO known future
	// cards must return the fallback suggestion as-is rather than
	// reading out of bounds or misbehaving.
	void TestFallbackWithNoFutureCardsIsSafe()
	{
		std::printf("TestFallbackWithNoFutureCardsIsSafe:\n");

		std::int32_t player[2] = { 10, 2 }; // hard 12
		std::int32_t dealer[2] = { 2, 3 }; // 5, must hit
		std::int32_t future[1] = { 0 }; // unused -- futureCount is 0

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 0, true, false, false);
		Check(action == Action::Hit, "the fallback with no known future cards at all returns its own suggestion untouched",
			"basic strategy says Hit here (dealer upcard 3) and there's no known card to check for a bust override");
	}

	// Deck-derived Split: a pair that basic strategy ALWAYS splits (8s,
	// per the standard chart) should be REJECTED when the known upcoming
	// cards make it strictly worse than just standing pat -- proof the
	// engine isn't just re-deriving the textbook chart's own answers.
	void TestSplitEightsRejectedWhenKnownCardsMakeItWorse()
	{
		std::printf("TestSplitEightsRejectedWhenKnownCardsMakeItWorse:\n");

		std::int32_t player[2] = { 8, 8 };
		std::int32_t dealer[2] = { 10, 10 }; // already 20, stands pat
		std::int32_t future[3] = { 10, 10, 10 }; // both new hands land on 18 (still a loss to 20), and the un-split pair can only bust past 16

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 3, /*canDoubleAfterSplit*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(decision.trustworthy, "dealer already pat at 20 is trustworthy for the split comparison regardless of other seats", "same short-circuit as the hit/stand/double path");
		Check(!decision.shouldSplit, "splitting 8s into two known-losing 18s is worse than standing pat on the un-split 16's own best line",
			"basic strategy always splits 8s against an unknown card -- known cards here say don't");
	}

	// Deck-derived Split: a pair of Aces is forced to Stand on its one
	// dealt card per hand (no further play, see BlackjackHandEval.h's
	// own header comment) -- EvaluateSplit() must model that forced stand
	// rather than letting PlayHandOut() describe hits the game would
	// never actually offer, while still correctly recognizing the split
	// as far better than the un-split pair's own best (deterministic)
	// line with the same cards.
	void TestSplitAcesForcedStandStillBeatsTheUnsplitAlternative()
	{
		std::printf("TestSplitAcesForcedStandStillBeatsTheUnsplitAlternative:\n");

		std::int32_t player[2] = { 14, 14 }; // A,A
		std::int32_t dealer[2] = { 10, 10 }; // already 20, stands pat
		std::int32_t future[2] = { 10, 10 }; // each split hand becomes A,10=21; the un-split pair can only reach a hard 12 with these same two cards, or bust past it

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 2, /*canDoubleAfterSplit*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(decision.trustworthy, "dealer already pat at 20 is trustworthy regardless of other seats", "same rule as every other split/hit/stand/double case");
		Check(decision.shouldSplit, "splitting Aces into two forced-stand 21s beats the un-split pair's own best line with the same two cards",
			"A,10 + A,10 = two wins (+2) versus A,A,10,10's own best achievable hard 12, still a loss to 20 (-1)");
	}

	// A split hand can't double: the Double button (func_998) needs
	// f_59 == 1, and func_1237 alone isn't what the player sees. Live
	// bug: the mod advised Double on a split hand. 5,5 split vs a pat 19,
	// each new hand gets a 6 (11) and then a 10 (21): still worth
	// splitting, but each split hand hits into its 21, one unit each.
	void TestSplitHandsNeverDouble()
	{
		std::printf("TestSplitHandsNeverDouble:\n");

		std::int32_t player[2] = { 5, 5 };
		std::int32_t dealer[2] = { 10, 9 }; // already 19, stands pat
		std::int32_t future[4] = { 6, 6, 10, 10 };

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 4, /*canDouble*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(decision.trustworthy, "dealer already pat at 19 is trustworthy regardless of other seats", "same rule as every other case");
		Check(decision.shouldSplit, "splitting 5s into two hit-to-21 hands (+2) beats the un-split pair's losing 16 (-1)",
			"5,5,6 = 16 loses; the split hands hit (not double) into their 10s");

		Check(!BlackjackDeckSim::CanDouble(2, 2, 1000, 100), "CanDouble is false on a split hand even with 2 cards and the bankroll",
			"func_998 requires f_59 == 1");
		Check(BlackjackDeckSim::CanDouble(2, 1, 100, 100), "CanDouble on an unsplit 2-card hand with bankroll == bet", "func_998: f_1 >= f_4[0]");
		Check(!BlackjackDeckSim::CanDouble(3, 1, 1000, 100), "CanDouble is false with 3 cards", "func_998: f_23 == 2");
		Check(!BlackjackDeckSim::CanDouble(2, 1, 99, 100), "CanDouble is false when the bankroll can't cover the bet", "func_998: f_1 >= f_4[0]");

		const std::int32_t pair[2] = { 8, 8 };
		const std::int32_t nonPair[2] = { 13, 11 };
		Check(BlackjackDeckSim::CanSplit(pair, 2, 1, 100, 100), "CanSplit on an unsplit pair", "func_997");
		Check(!BlackjackDeckSim::CanSplit(nonPair, 2, 1, 100, 100), "CanSplit is false for K,J", "func_997 compares the ranks");
		Check(!BlackjackDeckSim::CanSplit(pair, 2, 2, 100, 100), "CanSplit is false once split", "func_997: f_59 == 1");

		// The live bug end to end: a split 5,6 (11), a known winning 10
		// next, dealer pat at 19, flags from CanDouble()/CanSplit() the
		// way the mod now builds them.
		const std::int32_t splitHand[2] = { 5, 6 };
		const std::int32_t next[1] = { 10 };
		Action action = BlackjackDeckSim::DetermineFullAdvice(splitHand, 2, dealer, 2, next, 1,
			BlackjackDeckSim::CanDouble(2, 2, 1000, 100), BlackjackDeckSim::CanSplit(splitHand, 2, 2, 1000, 100), false, true);
		Check(action == Action::Hit, "a split 11 with a known winning 10 is Hit, not Double", "Double isn't offered after a split");
	}

	// Deck-derived Split isn't trustworthy with fewer than 2 known future
	// cards -- both new hands need their own immediate card before
	// anything else can be evaluated at all.
	void TestSplitNotTrustworthyWithTooFewFutureCards()
	{
		std::printf("TestSplitNotTrustworthyWithTooFewFutureCards:\n");

		std::int32_t player[2] = { 10, 10 };
		std::int32_t dealer[2] = { 10, 9 }; // 19, stands pat
		std::int32_t future[1] = { 5 }; // only 1 known card -- not enough for 2 new hands' own immediate deals

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 1, /*canDoubleAfterSplit*/ true, /*isLastSeatBeforeDealer*/ true);
		Check(!decision.trustworthy, "fewer than 2 known future cards is never trustworthy for a split decision",
			"both post-split hands need their own immediate card before any comparison is meaningful");
	}

	// Code-review fix: the fallback's known-card check used to look only
	// one card ahead. Soft 16 (A,5) with known next cards 6 then 5: the 6
	// alone makes a hard 12 (a "downgrade"), which forced Stand -- but the
	// 5 after it makes 21. This hand's own hits are all exact even when
	// the dealer simulation isn't trusted, so the fallback must look at
	// every known stopping point, not just the first.
	void TestFallbackLooksPastAKnownDowngradeToALaterImprovement()
	{
		std::printf("TestFallbackLooksPastAKnownDowngradeToALaterImprovement:\n");

		std::int32_t player[2] = { 14, 5 }; // A,5 = soft 16
		std::int32_t dealer[2] = { 2, 10 }; // 12, must hit -- dealer sim not trusted with another seat to act
		std::int32_t future[3] = { 6, 5, 10 }; // A,5,6 = hard 12, then A,5,6,5 = 21, then a bust

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 3, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ false);
		Check(action == Action::Hit, "soft 16 with known 6 then 5 is Hit, not Stand",
			"A,5,6 is only a hard 12, but A,5,6,5 is 21 -- a later known card can redeem an early downgrade");

		// Same hand, but the card after the downgrade busts: nothing known
		// ever beats the current soft 16 (12 is exactly as weak as 16
		// against a dealer that always finishes on 17+), so Stand.
		std::int32_t futureNoHelp[2] = { 6, 10 }; // A,5,6 = 12, then 22
		Action standAction = DetermineCheatAction(player, 2, dealer, 2, futureNoHelp, 2, true, false, false);
		Check(standAction == Action::Stand, "soft 16 with known 6 then a bust card is Stand",
			"no known stopping point beats the current hand, and the one after the downgrade busts");
	}

	// Code-review fix: a known improvement overrides a textbook Stand in
	// the fallback too, not only a textbook Hit. Hard 13 vs dealer 6 is a
	// textbook Stand, but a known 4 next makes 17, which is never worse
	// than 13 against any dealer result and strictly better against 17.
	void TestFallbackKnownImprovementOverridesTextbookStand()
	{
		std::printf("TestFallbackKnownImprovementOverridesTextbookStand:\n");

		std::int32_t player[2] = { 10, 3 }; // hard 13
		std::int32_t dealer[2] = { 10, 6 }; // 16, must hit -- up card 6
		std::int32_t future[2] = { 4, 10 }; // 13+4=17, then a bust

		BlackjackHandEval::Action textbook = BlackjackHandEval::GetBasicStrategyAction(player, 2, dealer[1], true, false, false);
		Check(textbook == Action::Stand, "sanity check: textbook says Stand on hard 13 vs 6", "baseline this test overrides");

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 2, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ false);
		Check(action == Action::Hit, "hard 13 vs 6 with a known 4 next is Hit in the fallback",
			"17 is never worse than 13 against any dealer hand -- a known improvement dominates the textbook Stand");
	}

	// Code-review fix: EvaluateSplit() used to score every hand +/-1 even
	// when it doubled. Split hands can't double any more (see
	// TestSplitHandsNeverDouble), but the un-split line still can: 5,5 vs
	// a dealer pat at 17, known cards 10, 10, 6, 5. Un-split, 5,5 doubles
	// into the 10 for a winning 20 (+2). Split, 5,10 hits the 6 to 21 and
	// 5,10 hits the 5 to 20: two single wins (+2), a tie, so no split.
	// Scoring the double as +1 would split.
	void TestSplitCountsADoubledHandAsTwoUnits()
	{
		std::printf("TestSplitCountsADoubledHandAsTwoUnits:\n");

		std::int32_t player[2] = { 5, 5 };
		std::int32_t dealer[2] = { 10, 7 }; // 17, stands pat
		std::int32_t future[4] = { 10, 10, 6, 5 };

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 4, /*canDouble*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(decision.trustworthy, "dealer pat at 17 is trustworthy", "same short-circuit as every other split case");
		Check(!decision.shouldSplit, "a doubled un-split win counts 2 units, so two single split wins only tie it",
			"+2 (doubled 20) vs +1 +1; scoring the double as +1 would have split");
	}

	// SimulateDealerFromRanks() in isolation: dealer must keep drawing
	// while under 17 and stop the instant it reaches 17+, consuming
	// exactly as many future cards as needed and no more.
	void TestDealerSimulationStopsAtSeventeen()
	{
		std::printf("TestDealerSimulationStopsAtSeventeen:\n");

		std::int32_t dealer[2] = { 10, 5 }; // 15
		std::int32_t future[3] = { 2, 9, 9 }; // 15+2=17 -> stop; the trailing 9s must be untouched

		auto result = SimulateDealerFromRanks(dealer, 2, future, 3);
		Check(result.value.total == 17 && result.drawn == 1, "dealer stops the instant it reaches 17",
			"15+2=17 should stop after exactly one draw, not keep drawing into the trailing 9s");
	}

	// Betting advice. The round's result is known before the bet, so
	// PlayMyRound() returns it as a payout in half bets (a natural's 3:2
	// is +3) plus how many bets the line needs on the table, and
	// AdvisePreDealBet() turns that into a bet: the most that still pays
	// on a win, the least otherwise. Replaced Low/Medium/High, which
	// ranked a won Double -- the best-paying line -- below a plain win:
	// three Medium rounds of the second round log were double wins bet
	// small, about $19 left on the table (see tests/fixtures/rounds.jsonl).
	using BlackjackDeckSim::RoundPlan;
	using BlackjackDeckSim::PlayMyRound;
	using BlackjackDeckSim::TableLimits;
	using BlackjackDeckSim::BetSize;
	using BlackjackDeckSim::PreDealBet;
	using BlackjackDeckSim::AdvisePreDealBet;

	const BlackjackDeckSim::SeatsAfter kNoSeatsAfter{};

	void TestRoundPlanNaturals()
	{
		std::printf("TestRoundPlanNaturals:\n");

		const std::int32_t natural[2] = { 14, 10 };
		const std::int32_t dealerFive[2] = { 2, 3 };
		RoundPlan plan = PlayMyRound(natural, dealerFive, nullptr, 0, true, BlackjackDeckSim::SeatsAfter::Unknown());
		Check(plan.exact && plan.natural && plan.netHalfUnits == 3 && plan.stakeUnits == 1, "a natural pays 3:2 (+3 half bets), exact even with seats unknown",
			"nobody draws once a natural is dealt; func_1062 pays floor(2.5 * bet)");

		const std::int32_t dealerNatural[2] = { 14, 12 };
		plan = PlayMyRound(natural, dealerNatural, nullptr, 0, true, kNoSeatsAfter);
		Check(plan.exact && !plan.natural && plan.netHalfUnits == 0, "natural vs natural pushes", "0, not a 3:2 win");

		const std::int32_t hardEleven[2] = { 5, 6 };
		const std::int32_t ten[1] = { 10 };
		plan = PlayMyRound(hardEleven, dealerNatural, ten, 1, true, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == -2 && plan.stakeUnits == 1, "a dealer natural beats 11 even with a 10 next",
			"the round ends at the deal -- I never get to draw the 10");
	}

	void TestRoundPlanStandPatResults()
	{
		std::printf("TestRoundPlanStandPatResults:\n");

		const std::int32_t nineteen[2] = { 10, 9 };
		const std::int32_t eighteen[2] = { 10, 8 };
		const std::int32_t dealerSeventeen[2] = { 10, 7 };
		const std::int32_t dealerNineteen[2] = { 10, 9 };

		RoundPlan plan = PlayMyRound(nineteen, dealerSeventeen, nullptr, 0, true, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == 2 && plan.stakeUnits == 1, "19 vs a pat 17 wins one bet", "+2 half bets");
		plan = PlayMyRound(nineteen, dealerNineteen, nullptr, 0, true, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == 0, "19 vs 19 pushes", "0");
		plan = PlayMyRound(eighteen, dealerNineteen, nullptr, 0, true, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == -2, "18 vs a pat 19 loses one bet", "-2 half bets");

		const std::int32_t dealerFive[2] = { 2, 3 };
		const std::int32_t five[1] = { 5 };
		plan = PlayMyRound(nineteen, dealerFive, five, 1, true, BlackjackDeckSim::SeatsAfter::Unknown());
		Check(!plan.exact, "a drawing dealer with unknown seats after mine isn't exact", "the caller shows no betting advice");
	}

	// Live round 6 of the second round log: Q,5 vs the dealer's 4,10
	// (14), 6 then 10 next. Doubling makes 21 and the dealer busts on
	// the 10 -- two bets won, and it was shown as Medium.
	void TestRoundPlanDoubleWinIsTwoBets()
	{
		std::printf("TestRoundPlanDoubleWinIsTwoBets:\n");

		const std::int32_t player[2] = { 12, 5 };
		const std::int32_t dealer[2] = { 4, 10 };
		const std::int32_t future[2] = { 6, 10 };

		RoundPlan plan = PlayMyRound(player, dealer, future, 2, true, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == 4 && plan.stakeUnits == 2, "a won double is +4 half bets on two bets",
			"double pays twice a plain win");

		plan = PlayMyRound(player, dealer, future, 2, /*canAffordSecondBet*/ false, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == 2 && plan.stakeUnits == 1, "without a second bet the same card is a plain hit win",
			"hitting once draws the same 6 a double would");
	}

	// 8,8 vs a pat 17 with 10, 10 next: unsplit it's 16, and hitting
	// busts, so it loses; split, both hands make 18 and win.
	void TestRoundPlanSplitWinIsTwoBets()
	{
		std::printf("TestRoundPlanSplitWinIsTwoBets:\n");

		const std::int32_t eights[2] = { 8, 8 };
		const std::int32_t dealer[2] = { 10, 7 };
		const std::int32_t future[2] = { 10, 10 };

		RoundPlan plan = PlayMyRound(eights, dealer, future, 2, true, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == 4 && plan.stakeUnits == 2, "splitting 8,8 into two 18s wins two bets",
			"the old betting advice never split, so it showed this round as a loss");

		plan = PlayMyRound(eights, dealer, future, 2, /*canAffordSecondBet*/ false, kNoSeatsAfter);
		Check(plan.exact && plan.netHalfUnits == -2 && plan.stakeUnits == 1, "without a second bet 8,8 can't split and loses",
			"16 vs 17, and the 10 busts it");
	}

	// Only my seat (0) dealt in: my 5,6, the dealer's 10,5, then 10, 3.
	// I double into 21, the dealer draws the 3 to 18.
	const bool kOnlySeatZero[4] = { true, false, false, false };
	const std::int32_t kDoubleWinDeck[6] = { 5, 6, 10, 5, 10, 3 };

	void TestBetAmountForADoubleWin()
	{
		std::printf("TestBetAmountForADoubleWin:\n");

		const TableLimits limits{ 2, 500 };
		PreDealBet bet = AdvisePreDealBet(kDoubleWinDeck, 6, kOnlySeatZero, 0, 2000, limits);
		Check(bet.size == BetSize::Max && bet.amount == 500 && bet.predictedNet == 1000, "a double win with a big bankroll bets the table max",
			"$20.00 covers two $5.00 bets; the double wins $10.00");

		bet = AdvisePreDealBet(kDoubleWinDeck, 6, kOnlySeatZero, 0, 590, limits);
		Check(bet.size == BetSize::Max && bet.amount == 294 && bet.predictedNet == 588, "a double win bets half the bankroll when that's under the max",
			"$5.90 bankroll: $2.94 (rounded down to the 2-cent step) still leaves the double; betting it all would forfeit it");

		bet = AdvisePreDealBet(kDoubleWinDeck, 6, kOnlySeatZero, 0, 3, limits);
		Check(bet.size == BetSize::Max && bet.plan.stakeUnits == 1 && bet.amount == 2 && bet.predictedNet == 2,
			"when two minimum bets aren't affordable the round is replayed without the double",
			"3 cents can't cover 2 x 2 cents, so it's a plain hit win on one 2-cent bet");

		bet = AdvisePreDealBet(kDoubleWinDeck, 6, kOnlySeatZero, 0, 2000, TableLimits{});
		Check(bet.size == BetSize::Max && bet.amount == -1, "unknown limits still say Max, with no amount", "the HUD shows just the label");
	}

	void TestBetAmountForLossesAndNaturals()
	{
		std::printf("TestBetAmountForLossesAndNaturals:\n");

		const TableLimits limits{ 2, 500 };
		const std::int32_t lossDeck[4] = { 10, 8, 10, 9 }; // 18 vs a pat 19
		PreDealBet bet = AdvisePreDealBet(lossDeck, 4, kOnlySeatZero, 0, 2000, limits);
		Check(bet.size == BetSize::Min && bet.amount == 2 && bet.predictedNet == -2, "a loss bets the minimum", "18 vs 19");

		const std::int32_t pushDeck[4] = { 10, 9, 10, 9 };
		bet = AdvisePreDealBet(pushDeck, 4, kOnlySeatZero, 0, 2000, limits);
		Check(bet.size == BetSize::Min && bet.amount == 2 && bet.predictedNet == 0, "a push bets the minimum", "nothing to gain either way");

		const std::int32_t naturalDeck[4] = { 14, 13, 10, 7 };
		bet = AdvisePreDealBet(naturalDeck, 4, kOnlySeatZero, 0, 301, limits);
		Check(bet.size == BetSize::Max && bet.amount == 300 && bet.predictedNet == 450, "a natural bets all it can, one bet",
			"no double needed, so the whole (step-rounded) bankroll; pays floor(2.5 * 300) - 300 = 450");
	}

	void TestOutcomeRanking()
	{
		std::printf("TestOutcomeRanking:\n");

		Check(OutcomeRank(Outcome::Win) > OutcomeRank(Outcome::Push), "Win outranks Push", "ranking must be strictly ordered");
		Check(OutcomeRank(Outcome::Push) > OutcomeRank(Outcome::Loss), "Push outranks Loss", "ranking must be strictly ordered");

		HandValue playerBust; playerBust.bust = true; playerBust.total = 24;
		HandValue dealerLow; dealerLow.total = 12;
		Check(CompareOutcome(playerBust, dealerLow) == Outcome::Loss, "a busted player always loses, even to a low dealer total", "bust overrides everything else");

		HandValue playerGood; playerGood.total = 18;
		HandValue dealerBust; dealerBust.bust = true; dealerBust.total = 26;
		Check(CompareOutcome(playerGood, dealerBust) == Outcome::Win, "a non-bust player always beats a busted dealer", "dealer bust overrides the totals comparison");
	}

	void TestNaturalsBeatThreeCardTwentyOne()
	{
		std::printf("TestNaturalsBeatThreeCardTwentyOne:\n");

		const std::int32_t natural[2] = { 14, 13 };       // A,K
		const std::int32_t threeCard21[3] = { 7, 7, 7 };  // 7,7,7
		const HandValue naturalValue = EvaluateHand(natural, 2);
		const HandValue threeCardValue = EvaluateHand(threeCard21, 3);

		Check(CompareOutcome(threeCardValue, naturalValue) == Outcome::Loss,
			"a three-card 21 loses to a dealer blackjack", "was a push when only totals were compared");
		Check(CompareOutcome(naturalValue, threeCardValue) == Outcome::Win,
			"a player natural beats a dealer three-card 21", "was a push when only totals were compared");
		Check(CompareOutcome(naturalValue, naturalValue) == Outcome::Push,
			"natural vs natural is a push", "both blackjacks");
		Check(CompareOutcome(naturalValue, threeCardValue, /*playerNaturalCounts*/ false) == Outcome::Push,
			"a two-card 21 on a split hand is just 21, not a natural", "split A,10 pushes a dealer 21");
		Check(CompareOutcome(naturalValue, naturalValue, /*playerNaturalCounts*/ false) == Outcome::Loss,
			"a split two-card 21 loses to a dealer blackjack", "split hands can't have a natural");
	}

	// EvaluatePreDealBetting(): a lower seat dealt a natural never draws,
	// so it doesn't make my own cards unknown. Live report: seat 0 A,K,
	// me (seat 1) 9,Q = 19, seat 3 2,3, dealer K,10 = 20, Q next -- the
	// exact answer is a loss (standing loses, hitting busts), but counting
	// seat 0 as "draws first" fell back to the textbook estimate, Medium.
	// Also recorded in tests/fixtures/rounds.jsonl; this pins the rule
	// itself, with a control where seat 0 has no natural.
	void TestPreDealBettingIgnoresALowerSeatNatural()
	{
		std::printf("TestPreDealBettingIgnoresALowerSeatNatural:\n");

		const bool seatsDealt[4] = { true, true, false, true };
		const std::int32_t deck[14] = { 14, 13, 9, 12, 2, 3, 13, 10, 12, 2, 3, 11, 6, 8 };
		Check(BlackjackDeckSim::EvaluatePreDealBetting(deck, 14, seatsDealt, 1).netHalfUnits == -2,
			"19 vs a dealer 20 with a Q next is a loss when the seat before mine has a natural",
			"a natural never draws, so my own cards are exact -- stand loses, hit busts");

		const std::int32_t deckNoNatural[14] = { 10, 6, 9, 12, 2, 3, 13, 10, 12, 2, 3, 11, 6, 8 };
		Check(BlackjackDeckSim::EvaluatePreDealBetting(deckNoNatural, 14, seatsDealt, 1).netHalfUnits == 4,
			"with a drawing seat before mine, the AI model plays it first: seat 0's 16 vs a 10 hits the Q and busts",
			"then my 19 doubles on the known 2 to 21 and beats the dealer's 20 -- two bets won");
	}

	// func_623 transcription sanity: every row is empty (func_623's
	// default) or exactly 13 columns (up cards 2..14) of H/S/D/P, and a
	// few entries read straight off the decompile.
	void TestAiTableMatchesFunc623()
	{
		std::printf("TestAiTableMatchesFunc623:\n");

		bool shapeOk = true;
		for (std::int32_t total = 0; total < 22; total++)
		{
			for (std::string_view row : { BlackjackDeckSim::detail::kAiHardTable[total], BlackjackDeckSim::detail::kAiPairTable[total] })
			{
				if (!row.empty() && (row.size() != 13 || row.find_first_not_of("HSDP") != std::string_view::npos))
					shapeOk = false;
			}
		}
		Check(shapeOk, "every AI table row is empty or 13 H/S/D/P columns", "a typo in the transcription");

		using BlackjackDeckSim::AiTableAction;
		Check(AiTableAction(16, 6, false) == Action::Stand, "AI hard 16 vs 6 stands", "func_623 case 6 / 16 -> 7");
		Check(AiTableAction(16, 7, false) == Action::Hit, "AI hard 16 vs 7 hits", "func_623 case 7 / 16 -> 5");
		Check(AiTableAction(12, 2, false) == Action::Hit && AiTableAction(12, 4, false) == Action::Stand, "AI hard 12 hits vs 2, stands vs 4", "func_623");
		Check(AiTableAction(11, 13, false) == Action::Double && AiTableAction(11, 14, false) == Action::Hit, "AI 11 doubles vs K, hits vs A", "func_623");
		Check(AiTableAction(9, 2, false) == Action::Hit && AiTableAction(9, 3, false) == Action::Double, "AI 9 hits vs 2, doubles vs 3", "func_623");
		Check(AiTableAction(18, 9, true) == Action::Split && AiTableAction(18, 10, true) == Action::Stand, "AI 9,9 splits vs 9, stands vs 10", "func_623 pair branch");
		Check(AiTableAction(8, 5, true) == Action::Split && AiTableAction(8, 4, true) == Action::Hit, "AI 4,4 splits vs 5 only (and 6)", "func_623 pair branch");
		Check(AiTableAction(10, 9, true) == Action::Double, "AI 5,5 doubles vs 9", "func_623 pair branch, total 10");
	}

	// func_1002's overrides on top of the table.
	void TestAiDecideOverrides()
	{
		std::printf("TestAiDecideOverrides:\n");

		using BlackjackDeckSim::AiDecide;
		const std::int32_t aces[2] = { 14, 14 };
		Check(AiDecide(aces, 2, 10, 1, true) == Action::Split, "AI always splits Aces", "func_1002 checks 14,14 before the table");
		Check(AiDecide(aces, 2, 10, 1, false) == Action::Hit, "AI Aces it can't afford to split play as a hard-table 12", "func_997(.., true) needs f_1 >= f_4[0]");

		const std::int32_t eleven[2] = { 5, 6 };
		const std::int32_t threeCardEleven[3] = { 2, 3, 6 };
		Check(AiDecide(eleven, 2, 6, 1, true) == Action::Double, "AI 11 vs 6 doubles", "table D");
		Check(AiDecide(eleven, 2, 6, 1, false) == Action::Hit, "AI Double drops to Hit when it can't cover the bet", "func_113 < func_492");
		Check(AiDecide(eleven, 2, 6, 2, true) == Action::Hit, "AI never doubles after a split", "f_59 > 1");
		Check(AiDecide(threeCardEleven, 3, 6, 1, true) == Action::Hit, "AI never doubles on 3 cards", "f_23 > 2");

		const std::int32_t soft18[2] = { 14, 7 };
		Check(AiDecide(soft18, 2, 10, 1, true) == Action::Stand, "AI soft 18 stands even vs 10", "func_623 only sees the total (func_645)");
	}

	// The live round 5 shape: an AI seat after mine hits first, so the
	// dealer draws a different card. SimulateSeatsThenDealer() must play
	// the seats before the dealer, in seat order.
	void TestSeatsAfterDrawBeforeTheDealer()
	{
		std::printf("TestSeatsAfterDrawBeforeTheDealer:\n");

		const std::int32_t dealer[2] = { 8, 6 }; // hole 8, up 6: 14
		BlackjackDeckSim::SeatsAfter after;
		const std::int32_t seat1[2] = { 4, 2 };
		const std::int32_t seat3[2] = { 3, 10 };
		after.Add(seat1, 2, true);
		after.Add(seat3, 2, true);
		const std::int32_t future[4] = { 13, 6, 4, 5 };

		BlackjackDeckSim::DealerSimResult result = BlackjackDeckSim::SimulateSeatsThenDealer(dealer, 2, after, future, 4);
		Check(result.value.total == 20, "seat 1's 6 vs 6 hits the K and stands on 16, seat 3's 13 stands, dealer takes the 6 for 20",
			"the recorded dealer hand was 8D 6H 6D");

		// Session 9's hard-9 bug, now answered exactly: me 5,4 vs a dealer
		// 16 (10,6); one AI seat after me with 2,3. Standing: the AI takes
		// the 10 (15, stands vs 6), dealer takes the 5 for 21 -- loss.
		// Doubling: I take the 10 (19), the AI takes 5 then 9 (19), dealer
		// takes the 2 for 18 -- win. Treating the seat as absent (the old
		// "last seat" answer) would stand, betting the dealer busts on the 10.
		const std::int32_t me[2] = { 5, 4 };
		const std::int32_t dealer16[2] = { 10, 6 };
		const std::int32_t lowSeat[2] = { 2, 3 };
		BlackjackDeckSim::SeatsAfter one;
		one.Add(lowSeat, 2, true);
		const std::int32_t cards[5] = { 10, 5, 9, 2, 10 };
		Check(DetermineCheatAction(me, 2, dealer16, 2, cards, 5, true, false, one) == Action::Double,
			"9 vs 16 doubles once the AI seat after me is modeled", "stand loses to 21; double wins 19 vs 18");
		Check(DetermineCheatAction(me, 2, dealer16, 2, cards, 5, true, false, /*isLastSeatBeforeDealer*/ true) == Action::Stand,
			"control: ignoring the AI seat stands on 9", "the dealer would bust on the 10 only if nobody else drew it");
	}

	std::string_view ActionName(Action action)
	{
		switch (action)
		{
			case Action::Hit: return "Hit";
			case Action::Double: return "Double";
			case Action::Split: return "Split";
			default: return "Stand";
		}
	}

	// Replays tests/fixtures/rounds.jsonl -- lines copied out of the mod's
	// BlackjackCheat_rounds.jsonl round log (Debug build) with an
	// "expectBetting" (round lines) or "expectAction" (decision lines) key
	// added by hand. Each line goes through the exact pure function the
	// mod ran: AdvisePreDealBet() / DetermineFullAdvice(). See
	// src/RoundRecord.h for the format. Lines without an expect* key, and
	// blank or '#' lines, are skipped.
	void TestRecordedRounds()
	{
		std::printf("TestRecordedRounds:\n");

		const std::filesystem::path candidates[] = {
			std::filesystem::path(__FILE__).parent_path() / "fixtures" / "rounds.jsonl",
			std::filesystem::path("tests") / "fixtures" / "rounds.jsonl",
		};
		std::ifstream file;
		for (const std::filesystem::path& candidate : candidates)
		{
			file.open(candidate);
			if (file)
				break;
			file.clear();
		}
		if (!file)
		{
			Check(false, "tests/fixtures/rounds.jsonl opens", "run from the BlackjackCheat directory");
			return;
		}

		std::int32_t checked = 0;
		std::int32_t lineNumber = 0;
		std::string line;
		while (std::getline(file, line))
		{
			lineNumber++;
			if (line.empty() || line[0] == '#')
				continue;

			std::string type;
			std::string id;
			std::string note;
			RoundRecord::GetString(line, "type", type);
			if (!RoundRecord::GetString(line, "id", id))
				RoundRecord::GetString(line, "round", id);
			RoundRecord::GetString(line, "note", note);
			const std::string name = "rounds.jsonl:" + std::to_string(lineNumber) + " " + type + " " + id + (note.empty() ? "" : " -- " + note);

			if (type == "round")
			{
				std::string expect;
				std::vector<std::int32_t> expectDealer;
				const bool hasBetting = RoundRecord::GetString(line, "expectBetting", expect);
				const bool hasDealer = RoundRecord::GetIntArray(line, "expectDealer", expectDealer);
				if (!hasBetting && !hasDealer)
					continue;

				std::int32_t mySeat = -1;
				std::vector<std::int32_t> seats;
				std::vector<std::int32_t> deck;
				if (!RoundRecord::GetInt(line, "mySeat", mySeat) || !RoundRecord::GetIntArray(line, "seatsDealt", seats)
					|| !RoundRecord::GetIntArray(line, "deckRanks", deck) || seats.size() != 4)
				{
					Check(false, name.c_str(), "needs mySeat, seatsDealt (4 entries) and deckRanks");
					continue;
				}

				bool seatsDealt[4] = {};
				for (std::size_t i = 0; i < 4; i++)
					seatsDealt[i] = seats[i] != 0;

				// expectBetting: Max/Min. Optional expectBettingNet (the plan's
				// half bets) and expectBetAmount (cents, needs the line's
				// bankrollBeforeRound, tableMinBet and tableMaxBet).
				if (hasBetting)
				{
					std::int32_t bankroll = -1;
					BlackjackDeckSim::TableLimits limits;
					RoundRecord::GetInt(line, "bankrollBeforeRound", bankroll);
					RoundRecord::GetInt(line, "tableMinBet", limits.minBet);
					RoundRecord::GetInt(line, "tableMaxBet", limits.maxBet);
					const BlackjackDeckSim::PreDealBet bet = BlackjackDeckSim::AdvisePreDealBet(deck.data(), static_cast<std::int32_t>(deck.size()), seatsDealt, mySeat, bankroll, limits);

					std::string_view got = bet.plan.exact ? BlackjackDeckSim::BetSizeName(bet.size) : "none";
					std::string detail = "expected betting " + expect + ", got " + std::string(got);
					Check(got == expect, (name + " [betting]").c_str(), detail.c_str());
					checked++;

					std::int32_t expectNet = 0;
					if (RoundRecord::GetInt(line, "expectBettingNet", expectNet))
					{
						detail = "expected " + std::to_string(expectNet) + " half bets, got " + std::to_string(bet.plan.netHalfUnits);
						Check(bet.plan.netHalfUnits == expectNet, (name + " [betting net]").c_str(), detail.c_str());
						checked++;
					}

					std::int32_t expectAmount = 0;
					if (RoundRecord::GetInt(line, "expectBetAmount", expectAmount))
					{
						detail = "expected a bet of " + std::to_string(expectAmount) + " cents, got " + std::to_string(bet.amount);
						Check(bet.amount == expectAmount, (name + " [bet amount]").c_str(), detail.c_str());
						checked++;
					}
				}

				// expectDealer: the dealer's real final ranks. ReplayDealer()
				// plays the AI seats off the deck with my seat taking
				// myCardsDrawn cards, the same replay the mod logs.
				if (hasDealer)
				{
					std::int32_t myCardsDrawn = 0;
					if (!RoundRecord::GetInt(line, "myCardsDrawn", myCardsDrawn))
					{
						Check(false, name.c_str(), "expectDealer needs myCardsDrawn");
						continue;
					}
					BlackjackDeckSim::ReplayedDealer replayed = BlackjackDeckSim::ReplayDealer(deck.data(), static_cast<std::int32_t>(deck.size()), seatsDealt, mySeat, myCardsDrawn);
					std::string got;
					for (std::int32_t i = 0; i < replayed.count; i++)
						got += (i ? "," : "") + std::to_string(replayed.ranks[i]);
					std::string want;
					for (std::size_t i = 0; i < expectDealer.size(); i++)
						want += (i ? "," : "") + std::to_string(expectDealer[i]);
					const std::string detail = "expected dealer [" + want + "], replayed [" + got + "]";
					Check(replayed.valid && got == want, (name + " [dealer replay]").c_str(), detail.c_str());
					checked++;
				}
			}
			else if (type == "decision")
			{
				std::string expect;
				if (!RoundRecord::GetString(line, "expectAction", expect))
					continue;

				std::vector<std::int32_t> player;
				std::vector<std::int32_t> dealer;
				std::vector<std::int32_t> future;
				bool canDouble = false;
				bool canSplit = false;
				bool isSplitAceHand = false;
				if (!RoundRecord::GetIntArray(line, "playerRanks", player) || !RoundRecord::GetIntArray(line, "dealerRanks", dealer)
					|| !RoundRecord::GetIntArray(line, "futureRanks", future) || !RoundRecord::GetBool(line, "canDouble", canDouble)
					|| !RoundRecord::GetBool(line, "canSplit", canSplit) || !RoundRecord::GetBool(line, "isSplitAceHand", isSplitAceHand)
					|| player.empty() || dealer.empty())
				{
					Check(false, name.c_str(), "needs playerRanks, dealerRanks, futureRanks and the three flags");
					continue;
				}

				// The seats after mine: seatsAfter* (current format) or the
				// older isLastBeforeDealer flag.
				BlackjackDeckSim::SeatsAfter after;
				bool isLast = false;
				bool afterKnown = true;
				std::vector<std::int32_t> afterRanks;
				std::vector<std::int32_t> afterCounts;
				std::vector<std::int32_t> afterCanAfford;
				if (RoundRecord::GetBool(line, "seatsAfterKnown", afterKnown))
				{
					RoundRecord::GetIntArray(line, "seatsAfterRanks", afterRanks);
					RoundRecord::GetIntArray(line, "seatsAfterCounts", afterCounts);
					RoundRecord::GetIntArray(line, "seatsAfterCanAfford", afterCanAfford);
					std::size_t offset = 0;
					bool shapeOk = afterCanAfford.size() == afterCounts.size();
					for (std::size_t i = 0; shapeOk && i < afterCounts.size(); i++)
					{
						shapeOk = afterCounts[i] > 0 && offset + afterCounts[i] <= afterRanks.size();
						if (shapeOk)
							after.Add(afterRanks.data() + offset, afterCounts[i], afterCanAfford[i] != 0);
						offset += afterCounts[i];
					}
					if (!shapeOk)
					{
						Check(false, name.c_str(), "seatsAfterCounts/seatsAfterCanAfford don't match seatsAfterRanks");
						continue;
					}
					after.known = after.known && afterKnown;
				}
				else if (RoundRecord::GetBool(line, "isLastBeforeDealer", isLast))
				{
					after = BlackjackDeckSim::SeatsAfterFromFlag(isLast);
				}
				else
				{
					Check(false, name.c_str(), "needs seatsAfterKnown (+ seatsAfterRanks/Counts/CanAfford) or isLastBeforeDealer");
					continue;
				}

				std::string_view got = ActionName(BlackjackDeckSim::DetermineFullAdvice(
					player.data(), static_cast<std::int32_t>(player.size()), dealer.data(), static_cast<std::int32_t>(dealer.size()),
					future.data(), static_cast<std::int32_t>(future.size()), canDouble, canSplit, isSplitAceHand, after));
				const std::string detail = "expected action " + expect + ", got " + std::string(got);
				Check(got == expect, name.c_str(), detail.c_str());
				checked++;
			}
			else
			{
				Check(false, name.c_str(), "unknown \"type\" -- expected round or decision");
			}
		}

		Check(checked > 0, "rounds.jsonl has at least one expect* line", "an empty fixture file would silently test nothing");
	}
}

int main()
{
	TestHardElevenAlwaysHits();
	TestNoFutureCardsStands();
	TestKnownWinningCardIsDouble();
	TestSplitAceHandForcesStand();
	TestBustingCardIsRejected();
	TestHardNineDeniedDealerBust();
	TestKnownBustCardOverridesFallback();
	TestDealerAlreadyPatTrustsSimRegardlessOfSeats();
	TestSplitDeckDerivedFindsProfitableSplit();
	TestSplitDeckDerivedRejectsWhenNotProfitable();
	TestSplitDeckDerivedNotTrustworthyDefersToCaller();
	TestHardEighteenHitsOnThreeStandsOnFour();
	TestHardFifteenHitsToSeventeenEvenThoughStillLosing();
	TestHardTwentyStandsExceptDoublesOnKnownAce();
	TestSoftEighteenHitsIntoAWinningTwentyOne();
	TestSoftEighteenStandsWhenNextCardOnlyLowersTotal();
	TestPushCanBeUpgradedOrMustBeProtected();
	TestFallbackStillHitsWhenTheKnownCardIsSafe();
	TestFallbackBustOverrideWithAQueen();
	TestFallbackKnownDowngradeCardOverridesToStand();
	TestDealerPatAtEighteenTrustsDoubleRegardlessOfOtherSeats();
	TestDealerDrawSharesTheSameCursorAsAPlayerHit();
	TestFallbackWithNoFutureCardsIsSafe();
	TestSplitEightsRejectedWhenKnownCardsMakeItWorse();
	TestSplitAcesForcedStandStillBeatsTheUnsplitAlternative();
	TestSplitHandsNeverDouble();
	TestSplitNotTrustworthyWithTooFewFutureCards();
	TestFallbackLooksPastAKnownDowngradeToALaterImprovement();
	TestFallbackKnownImprovementOverridesTextbookStand();
	TestSplitCountsADoubledHandAsTwoUnits();
	TestDealerSimulationStopsAtSeventeen();
	TestRoundPlanNaturals();
	TestRoundPlanStandPatResults();
	TestRoundPlanDoubleWinIsTwoBets();
	TestRoundPlanSplitWinIsTwoBets();
	TestBetAmountForADoubleWin();
	TestBetAmountForLossesAndNaturals();
	TestOutcomeRanking();
	TestNaturalsBeatThreeCardTwentyOne();
	TestPreDealBettingIgnoresALowerSeatNatural();
	TestAiTableMatchesFunc623();
	TestAiDecideOverrides();
	TestSeatsAfterDrawBeforeTheDealer();
	TestRecordedRounds();

	if (g_failures == 0)
	{
		std::printf("\nALL PASS\n");
		return 0;
	}

	std::printf("\n%d FAILURE(S)\n", g_failures);
	return 1;
}
