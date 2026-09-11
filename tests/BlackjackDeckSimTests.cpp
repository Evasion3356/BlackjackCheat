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

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, /*canDouble*/ true, /*isSplitAceHand*/ false);
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

		Action action = DetermineCheatAction(player, 3, dealer, 2, future, 0, true, false);
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

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 2, /*canDouble*/ true, false);
		Check(action == Action::Double, "a known card that makes 21 on a 2-card 11 is Double",
			"exactly one more card wins outright (21 vs dealer's resulting 18) and no further hit could do better");

		Action actionNoDouble = DetermineCheatAction(player, 2, dealer, 2, future, 2, /*canDouble*/ false, false);
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

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, /*isSplitAceHand*/ true);
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

		Action action = DetermineCheatAction(player, 2, dealer, 2, future, 1, true, false);
		Check(action == Action::Stand, "a hand that would bust on the only known next card stays Stand",
			"extraHits=1 busts and must never be recorded as the new best, even though extraHits=0 already wins outright");
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
}

int main()
{
	TestHardElevenAlwaysHits();
	TestNoFutureCardsStands();
	TestKnownWinningCardIsDouble();
	TestSplitAceHandForcesStand();
	TestBustingCardIsRejected();
	TestDealerSimulationStopsAtSeventeen();
	TestOutcomeRanking();

	if (g_failures == 0)
	{
		std::printf("\nALL PASS\n");
		return 0;
	}

	std::printf("\n%d FAILURE(S)\n", g_failures);
	return 1;
}
