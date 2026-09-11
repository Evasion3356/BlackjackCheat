// Unit tests for src/BlackjackHandEval.h -- links against the SAME header
// the mod itself uses (not a hand-copied duplicate), same rationale as
// PokerCheat's tests/PokerHandEvalTests.cpp: a hand-scoring bug that's only
// ever eyeballed in-game can go undetected for a long time (see that
// project's docs/JOURNAL.md, Session 9, for exactly that happening to
// PokerCheat's original hand-rank code).
//
// Build & run (from the BlackjackCheat directory):
//   MSBuild.exe tests\BlackjackHandEvalTests.vcxproj /p:Configuration=Debug /p:Platform=x64
//   bin\Debug\BlackjackHandEvalTests.exe
// Exits 0 and prints "ALL PASS" if every case passes, exits 1 and lists
// failures otherwise.

#include "../src/BlackjackHandEval.h"

#include <cstdio>

namespace
{
	using BlackjackHandEval::HandValue;
	using BlackjackHandEval::EvaluateHand;
	using BlackjackHandEval::GetBasicStrategyAction;
	using Action = BlackjackHandEval::Action;

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

	void TestHardTotals()
	{
		std::printf("TestHardTotals:\n");

		std::int32_t handA[2] = { 10, 7 }; // 10,7 = 17
		HandValue a = EvaluateHand(handA, 2);
		Check(a.total == 17 && !a.soft && !a.bust, "10+7 is hard 17", "expected total 17, not soft, not bust");

		std::int32_t handB[3] = { 10, 6, 8 }; // 10+6+8 = 24 -> bust
		HandValue b = EvaluateHand(handB, 3);
		Check(b.bust && b.total == 24, "10+6+8 busts at 24", "expected bust with total 24");

		std::int32_t handC[2] = { 13, 12 }; // K,Q = 20
		HandValue c = EvaluateHand(handC, 2);
		Check(c.total == 20 && !c.soft && !c.blackjack, "K+Q is hard 20, not blackjack", "face cards sum to 20, no Ace involved");
	}

	void TestSoftTotals()
	{
		std::printf("TestSoftTotals:\n");

		std::int32_t handA[2] = { 14, 6 }; // A,6 = soft 17
		HandValue a = EvaluateHand(handA, 2);
		Check(a.total == 17 && a.soft, "A+6 is soft 17", "Ace should count as 11 here (6+11=17, no bust)");

		std::int32_t handB[3] = { 14, 6, 9 }; // A,6,9 = 11+6+9=26 -> demote ace to 1 -> 16
		HandValue b = EvaluateHand(handB, 3);
		Check(b.total == 16 && !b.soft && !b.bust, "A+6+9 becomes hard 16", "Ace must demote to 1 to avoid busting (11+6+9=26 -> 1+6+9=16)");

		std::int32_t handC[2] = { 14, 14 }; // A,A = soft 12
		HandValue c = EvaluateHand(handC, 2);
		Check(c.total == 12 && c.soft, "A+A is soft 12", "one Ace stays 11, the other demotes to 1 (11+1=12)");

		std::int32_t handD[2] = { 14, 13 }; // A,K = blackjack
		HandValue d = EvaluateHand(handD, 2);
		Check(d.total == 21 && d.blackjack, "A+K is a natural blackjack", "2-card 21 must set blackjack=true");
	}

	void TestBasicStrategyHard()
	{
		std::printf("TestBasicStrategyHard:\n");

		std::int32_t hand16[2] = { 10, 6 }; // hard 16
		Check(GetBasicStrategyAction(hand16, 2, 10, true, true) == Action::Hit, "hard 16 vs dealer 10 is Hit", "16 vs 10 is a classic hit, never stand");
		Check(GetBasicStrategyAction(hand16, 2, 6, true, true) == Action::Stand, "hard 16 vs dealer 6 is Stand", "dealer bust card (6) means stand on stiff hands 13-16");

		std::int32_t hand11[2] = { 6, 5 }; // hard 11
		Check(GetBasicStrategyAction(hand11, 2, 6, true, true) == Action::Double, "hard 11 vs dealer 6 is Double", "11 doubles against every upcard except Ace");
		Check(GetBasicStrategyAction(hand11, 2, 14, true, true) == Action::Hit, "hard 11 vs dealer Ace is Hit", "11 vs Ace is the one exception to always-double");
		Check(GetBasicStrategyAction(hand11, 3, 6, false, true) == Action::Hit, "hard 11 with 3 cards falls back to Hit", "double isn't legal past the first two cards");

		std::int32_t hand12[2] = { 10, 2 }; // hard 12
		Check(GetBasicStrategyAction(hand12, 2, 4, true, true) == Action::Stand, "hard 12 vs dealer 4 is Stand", "12 stands only against 4-6");
		Check(GetBasicStrategyAction(hand12, 2, 2, true, true) == Action::Hit, "hard 12 vs dealer 2 is Hit", "12 vs 2/3/7+ is Hit, not Stand");
	}

	void TestBasicStrategySoft()
	{
		std::printf("TestBasicStrategySoft:\n");

		std::int32_t soft18[2] = { 14, 7 }; // A,7
		Check(GetBasicStrategyAction(soft18, 2, 9, true, true) == Action::Hit, "soft 18 vs dealer 9 is Hit", "A7 hits against 9/10/A, doesn't just stand on 18");
		Check(GetBasicStrategyAction(soft18, 2, 6, true, true) == Action::Double, "soft 18 vs dealer 6 is Double", "A7 doubles against the dealer's weakest cards (3-6)");
		Check(GetBasicStrategyAction(soft18, 2, 8, true, true) == Action::Stand, "soft 18 vs dealer 8 is Stand", "A7 vs 2/7/8 stands");

		std::int32_t soft13[2] = { 14, 2 }; // A,2
		Check(GetBasicStrategyAction(soft13, 2, 5, true, true) == Action::Double, "soft 13 vs dealer 5 is Double", "A2 only doubles against 5-6");
		Check(GetBasicStrategyAction(soft13, 2, 9, true, true) == Action::Hit, "soft 13 vs dealer 9 is Hit", "A2 vs a strong dealer card is just Hit");
	}

	void TestBasicStrategyPairs()
	{
		std::printf("TestBasicStrategyPairs:\n");

		std::int32_t aces[2] = { 14, 14 };
		Check(GetBasicStrategyAction(aces, 2, 10, true, true) == Action::Split, "A,A always splits", "splitting aces is correct even against a strong dealer 10");

		// K,10 -- different RANKS (13 vs 10) but the same blackjack VALUE
		// (10). bjack_sp's own split-legality gate requires exact rank
		// equality (func_1237 case 6, see GetBasicStrategyAction()'s own
		// header comment) -- this case happens to return the same answer
		// either way today (ShouldSplitPair(10, ...) always says "never
		// split tens" regardless of which two 10-value ranks paired up),
		// so it can't externally distinguish rank-equality from
		// value-equality, but it still guards against ever regressing to
		// the old (wrong) CardValue()-based comparison, which would have
		// been a live bug the moment any 10-value split rule changed.
		std::int32_t tens[2] = { 13, 10 }; // K,10 -- different ranks, same blackjack value
		Check(GetBasicStrategyAction(tens, 2, 6, true, true) != Action::Split, "K,10 never splits", "mixed-rank 10-value pair should never split, even vs a weak dealer 6");
		Check(GetBasicStrategyAction(tens, 2, 6, true, true) == Action::Stand, "K,10 vs dealer 6 stands", "falls through to the hard-20 rule, which always stands");

		std::int32_t fives[2] = { 5, 5 };
		Check(GetBasicStrategyAction(fives, 2, 6, true, true) == Action::Double, "5,5 vs dealer 6 doubles, never splits", "5,5 must be played as hard 10, not split");

		std::int32_t eights[2] = { 8, 8 };
		Check(GetBasicStrategyAction(eights, 2, 10, true, true) == Action::Split, "8,8 always splits, even vs dealer 10", "classic \"always split 8s\" rule regardless of dealer strength");

		std::int32_t nines[2] = { 9, 9 };
		Check(GetBasicStrategyAction(nines, 2, 7, true, true) == Action::Stand, "9,9 vs dealer 7 stands (no split)", "9,9 is the one pair that stands rather than splits or hits vs a 7");
		Check(GetBasicStrategyAction(nines, 2, 6, true, true) == Action::Split, "9,9 vs dealer 6 splits", "9,9 splits against most cards except 7/10/A");

		std::int32_t canSplitFalse[2] = { 8, 8 };
		Check(GetBasicStrategyAction(canSplitFalse, 2, 10, true, false) != Action::Split, "8,8 with canSplit=false never returns Split", "gating must be honored even for an always-split pair");
	}

	// Session 3: bjack_sp force-resolves both hands from a split pair of
	// Aces after dealing each exactly one card, never re-offering
	// hit/stand/double (traced via the f_699 "hands to auto-advance"
	// counter -- see BlackjackHandEval.h's header comment, "Split Aces DO
	// get..."). isSplitAceHand=true must always return Stand, regardless
	// of how strong Hit/Double would otherwise look, since the game will
	// never actually let either happen.
	void TestSplitAceHands()
	{
		std::printf("TestSplitAceHands:\n");

		std::int32_t weakTotal[2] = { 14, 2 }; // A,2 = soft 13 -- would normally Hit/Double vs most upcards
		Check(GetBasicStrategyAction(weakTotal, 2, 5, true, true, true) == Action::Stand, "split-Ace hand (soft 13) vs dealer 5 is forced Stand", "the game never re-offers hit/double after a split Ace, regardless of how weak the resulting total looks");

		std::int32_t bustRisk[2] = { 14, 6 }; // A,6 = soft 17 -- would normally Hit/Double vs a weak dealer card
		Check(GetBasicStrategyAction(bustRisk, 2, 6, true, true, true) == Action::Stand, "split-Ace hand (soft 17) vs dealer 6 is still forced Stand", "isSplitAceHand overrides the soft-17-vs-weak-dealer Double/Hit logic entirely");

		std::int32_t strongTotal[2] = { 14, 8 }; // A,8 = soft 19 -- non-split-Ace strategy already says Stand here
		Check(GetBasicStrategyAction(strongTotal, 2, 6, true, true, false) == Action::Stand, "sanity check: non-split-Ace A,8 vs 6 is Stand anyway", "confirms the flag matters by picking a case where the ordinary answer already agrees, as a control");
		Check(GetBasicStrategyAction(strongTotal, 2, 6, true, true, true) == Action::Stand, "split-Ace A,8 vs 6 is Stand (same answer, forced not chosen)", "same result as the control case, but for the forced reason, not the ordinary strategy reason");
	}
}

int main()
{
	TestHardTotals();
	TestSoftTotals();
	TestBasicStrategyHard();
	TestBasicStrategySoft();
	TestBasicStrategyPairs();
	TestSplitAceHands();

	if (g_failures == 0)
	{
		std::printf("\nALL PASS\n");
		return 0;
	}

	std::printf("\n%d FAILURE(S)\n", g_failures);
	return 1;
}
