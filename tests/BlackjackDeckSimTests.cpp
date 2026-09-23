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

#include <cstdio>

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
	using BlackjackDeckSim::EvaluateBettingConfidence;
	using BlackjackDeckSim::BettingAdvice;
	using Confidence = BlackjackHandEval::BettingConfidence;

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
		// must now defer entirely to basic strategy instead of Stand --
		// hard 9 vs a dealer up-card of 6 (dealerRanks[1], the real
		// visible card) is a textbook Double (BlackjackHandEval.h,
		// total==9 && dealerVal in [3,6]).
		Action notLastSeat = DetermineCheatAction(player, 2, dealer, 2, future, 9, /*canDouble*/ true, false, /*isLastSeatBeforeDealer*/ false);
		Check(notLastSeat == Action::Double, "the same hand with another seat still to act falls back to basic strategy, never Stand",
			"a bust-proof hard 9 must never Stand when this function can't trust its own dealer simulation -- basic strategy vs dealer 6 says Double");

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
		Check(decision.shouldSplit, "known upcoming cards make splitting these tens strictly more profitable than standing pat",
			"21 + 19, both beating a dealer 17, is +2 bet units versus +1 for doubling the un-split pair to a lone winning 21");
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

	// Deck-derived Split, the Double-within-a-split-hand case: each new
	// hand from splitting a pair of 5s gets a card that makes an
	// ordinary 2-card 11, and the NEXT known card for each makes a
	// winning 21 -- PlayHandOut() must recognize that as a Double (one
	// card, then forced stand), not an open-ended Hit, exactly the same
	// way DetermineCheatAction() already does for an un-split hand.
	void TestSplitFivesDoublesWithinEachNewHand()
	{
		std::printf("TestSplitFivesDoublesWithinEachNewHand:\n");

		std::int32_t player[2] = { 5, 5 };
		std::int32_t dealer[2] = { 10, 9 }; // already 19, stands pat
		std::int32_t future[4] = { 6, 6, 10, 10 }; // hand 1 gets 6 (5,6=11) then doubles into the first 10 (21); hand 2 gets 6 (5,6=11) then doubles into the second 10 (21)

		SplitDecision decision = EvaluateSplit(player, 2, dealer, 2, future, 4, /*canDoubleAfterSplit*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(decision.trustworthy, "dealer already pat at 19 is trustworthy regardless of other seats", "same rule as every other case");
		Check(decision.shouldSplit, "splitting 5s and doubling each new hand into a 21 beats the un-split pair's own best line",
			"two doubled 21s beating a 19 (+2) versus the un-split pair's own best achievable line with the same four cards (a losing 16, -1)");
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

	// Session 13 addition -- Betting Advice's deck-derived path
	// (EstimateBettingConfidence()'s own non-deck-derived fallback is
	// tested in tests/BlackjackHandEvalTests.cpp). A natural blackjack
	// resolves immediately against the dealer's own already-dealt two
	// cards -- exact and trustworthy regardless of isLastSeatBeforeDealer
	// or the future deck, since neither side draws.
	void TestBettingConfidenceNaturalBlackjackIsHigh()
	{
		std::printf("TestBettingConfidenceNaturalBlackjackIsHigh:\n");

		std::int32_t player[2] = { 14, 10 }; // A,10 = natural 21
		std::int32_t dealer[2] = { 2, 3 }; // 5, not a blackjack, still needs to hit
		std::int32_t future[1] = { 0 }; // unused -- must never be read for this immediate case

		BettingAdvice advice = EvaluateBettingConfidence(player, 2, dealer, 2, future, 0, /*canDouble*/ true, /*isLastSeatBeforeDealer*/ false);
		Check(advice.trustworthy, "a natural blackjack is trustworthy even with another seat still to act and a dealer that hasn't finished",
			"neither side draws when the player already has blackjack -- it resolves against the dealer's own already-dealt two cards immediately");
		Check(advice.outcome == Outcome::Win, "a natural blackjack against a non-blackjack dealer is a Win", "21 on the first two cards beats anything except a matching dealer blackjack");
		Check(advice.confidence == Confidence::High, "a natural blackjack is always High confidence", "the strongest possible starting hand");
	}

	// Two natural blackjacks push -- Low confidence, not a Win.
	void TestBettingConfidenceBothBlackjackIsPush()
	{
		std::printf("TestBettingConfidenceBothBlackjackIsPush:\n");

		std::int32_t player[2] = { 14, 10 };
		std::int32_t dealer[2] = { 14, 12 }; // A,Q = also a natural 21

		BettingAdvice advice = EvaluateBettingConfidence(player, 2, dealer, 2, nullptr, 0, true, false);
		Check(advice.trustworthy, "both-blackjack is still an immediate, trustworthy resolution", "same short-circuit as the single-blackjack case");
		Check(advice.outcome == Outcome::Push, "two natural blackjacks push", "neither side has a stronger 21 than the other");
		Check(advice.confidence == Confidence::Low, "a push is Low confidence, not High, even off a natural blackjack", "a push doesn't favor betting more, regardless of how the hand got there");
	}

	// Without a natural blackjack, the same dealerOutcomeTrustworthy
	// precondition DetermineCheatAction()/EvaluateSplit() need applies
	// here too -- a dealer that still needs to hit, with another seat
	// still to act, means the future deck can't be trusted at all.
	void TestBettingConfidenceNotTrustworthyWithoutBlackjack()
	{
		std::printf("TestBettingConfidenceNotTrustworthyWithoutBlackjack:\n");

		std::int32_t player[2] = { 10, 6 }; // hard 16, not blackjack
		std::int32_t dealer[2] = { 2, 3 }; // 5, must hit
		std::int32_t future[1] = { 5 };

		BettingAdvice advice = EvaluateBettingConfidence(player, 2, dealer, 2, future, 1, true, /*isLastSeatBeforeDealer*/ false);
		Check(!advice.trustworthy, "a dealer that still needs to hit, with another seat still to act, can't be trusted",
			"same precondition as DetermineCheatAction()/EvaluateSplit() -- the caller must fall back to the textbook heuristic instead");
	}

	// A win reached by simply standing on the hand as dealt (the dealer
	// is already pat, so PlayHandOut() never needs to consume a future
	// card) is High confidence -- the "blackjack/high probability of
	// winning" case from the user's own description, even without an
	// actual natural blackjack.
	void TestBettingConfidenceStandPatWinIsHigh()
	{
		std::printf("TestBettingConfidenceStandPatWinIsHigh:\n");

		std::int32_t player[2] = { 10, 9 }; // hard 19
		std::int32_t dealer[2] = { 10, 7 }; // already 17, stands pat

		BettingAdvice advice = EvaluateBettingConfidence(player, 2, dealer, 2, nullptr, 0, true, false);
		Check(advice.trustworthy, "a dealer already pat at 17 is trustworthy regardless of other seats", "same short-circuit DetermineCheatAction() itself already uses");
		Check(advice.outcome == Outcome::Win, "hard 19 already beats a dealer pat at 17", "19 > 17, no draw needed on either side");
		Check(advice.confidence == Confidence::High, "a win with no extra hits needed is High confidence", "the hand as already dealt already wins outright");
	}

	// A win that only materializes by hitting/doubling into it is Medium
	// -- exactly the user's own "could win it if the cards advance/double
	// down on the right card" description. Reuses the same known-winning
	// double scenario as TestKnownWinningCardIsDouble above (hard 11,
	// known next card makes 21).
	void TestBettingConfidenceWinViaDoubleIsMedium()
	{
		std::printf("TestBettingConfidenceWinViaDoubleIsMedium:\n");

		std::int32_t player[2] = { 5, 6 }; // hard 11
		std::int32_t dealer[2] = { 10, 5 }; // 15, must hit
		std::int32_t future[2] = { 10, 3 }; // player doubles into 21; dealer's own subsequent draw makes 18

		BettingAdvice advice = EvaluateBettingConfidence(player, 2, dealer, 2, future, 2, /*canDouble*/ true, /*isLastSeatBeforeDealer*/ true);
		Check(advice.trustworthy, "last seat before the dealer makes this trustworthy", "same precondition as DetermineCheatAction()");
		Check(advice.outcome == Outcome::Win, "doubling the known next card into 21 beats the dealer's resulting 18", "11+10=21 vs 15+3=18");
		Check(advice.confidence == Confidence::Medium, "a win reached only by doubling into it is Medium, not High",
			"the win only exists because of the known upcoming card, not the hand as already dealt -- \"could win it if the cards advance\"");
	}

	// Push and loss both bucket to Low -- neither favors betting more.
	void TestBettingConfidencePushAndLossAreBothLow()
	{
		std::printf("TestBettingConfidencePushAndLossAreBothLow:\n");

		std::int32_t pushPlayer[2] = { 10, 9 }; // hard 19
		std::int32_t pushDealer[2] = { 10, 9 }; // also 19, stands pat
		BettingAdvice pushAdvice = EvaluateBettingConfidence(pushPlayer, 2, pushDealer, 2, nullptr, 0, true, false);
		Check(pushAdvice.trustworthy, "dealer already pat is trustworthy regardless of other seats", "same short-circuit as every other pat-dealer case");
		Check(pushAdvice.outcome == Outcome::Push, "identical stand-pat totals push", "19 vs 19");
		Check(pushAdvice.confidence == Confidence::Low, "a push is Low confidence", "no reason to bet more on a hand that only ties");

		std::int32_t lossPlayer[2] = { 10, 8 }; // hard 18
		std::int32_t lossDealer[2] = { 10, 9 }; // 19, stands pat, already beats 18
		BettingAdvice lossAdvice = EvaluateBettingConfidence(lossPlayer, 2, lossDealer, 2, nullptr, 0, true, false);
		Check(lossAdvice.trustworthy, "dealer already pat is trustworthy regardless of other seats", "same short-circuit as every other pat-dealer case");
		Check(lossAdvice.outcome == Outcome::Loss, "hard 18 already loses to a dealer pat at 19", "18 < 19, and this hand's own best line (Stand) can't change that");
		Check(lossAdvice.confidence == Confidence::Low, "a loss is Low confidence", "no reason to bet more on a hand that's already losing");
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
	TestSplitFivesDoublesWithinEachNewHand();
	TestSplitNotTrustworthyWithTooFewFutureCards();
	TestDealerSimulationStopsAtSeventeen();
	TestBettingConfidenceNaturalBlackjackIsHigh();
	TestBettingConfidenceBothBlackjackIsPush();
	TestBettingConfidenceNotTrustworthyWithoutBlackjack();
	TestBettingConfidenceStandPatWinIsHigh();
	TestBettingConfidenceWinViaDoubleIsMedium();
	TestBettingConfidencePushAndLossAreBothLow();
	TestOutcomeRanking();
	TestNaturalsBeatThreeCardTwentyOne();

	if (g_failures == 0)
	{
		std::printf("\nALL PASS\n");
		return 0;
	}

	std::printf("\n%d FAILURE(S)\n", g_failures);
	return 1;
}
