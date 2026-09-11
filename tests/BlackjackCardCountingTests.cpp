// Unit tests for src/BlackjackCardCounting.h -- same rationale/pattern as
// tests/BlackjackHandEvalTests.cpp (links against the exact header the mod
// itself uses).
//
// Build & run (from the BlackjackCheat directory):
//   MSBuild.exe tests\BlackjackCardCountingTests.vcxproj /p:Configuration=Debug /p:Platform=x64
//   bin\Debug\BlackjackCardCountingTests.exe

#include "../src/BlackjackCardCounting.h"

#include <cstdio>

namespace
{
	using namespace BlackjackCardCounting;

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

	void CheckNear(double actual, double expected, const char* testName, const char* detail)
	{
		constexpr double kEpsilon = 0.0001;
		Check(actual > expected - kEpsilon && actual < expected + kEpsilon, testName, detail);
	}

	void TestHiLoTag()
	{
		std::printf("TestHiLoTag:\n");

		Check(HiLoTag(2) == 1 && HiLoTag(6) == 1, "2-6 tag +1", "low cards should be +1 (favors the player as they're removed)");
		Check(HiLoTag(7) == 0 && HiLoTag(8) == 0 && HiLoTag(9) == 0, "7-9 tag 0", "neutral cards should be 0");
		Check(HiLoTag(10) == -1 && HiLoTag(11) == -1 && HiLoTag(12) == -1 && HiLoTag(13) == -1, "10/J/Q/K tag -1", "all ten-value cards should be -1");
		Check(HiLoTag(14) == -1, "Ace tags -1", "standard Hi-Lo counts Aces as high cards, same as tens");
	}

	void TestRunningCount()
	{
		std::printf("TestRunningCount:\n");

		Count c;
		c.AddCard(2);  // +1
		c.AddCard(5);  // +1
		c.AddCard(14); // -1
		c.AddCard(9);  // 0
		Check(c.running == 1, "2,5,A,9 gives running count +1", "expected (+1)+(+1)+(-1)+(0) = 1");

		c.Reset();
		Check(c.running == 0, "Reset zeroes the running count", "Reset() must clear accumulated state for the next round (bjack_sp reshuffles every round -- see this file's header comment)");
	}

	void TestDecksRemaining()
	{
		std::printf("TestDecksRemaining:\n");

		CheckNear(DecksRemaining(52, 0), 1.0, "full untouched deck is 1.0 decks remaining", "0 cards drawn from 52 = 1 full deck left");
		CheckNear(DecksRemaining(52, 26), 0.5, "half-drawn deck is 0.5 decks remaining", "26 of 52 drawn = half a deck left");
		CheckNear(DecksRemaining(52, 52), 0.0, "fully-drawn deck is 0.0 decks remaining", "all 52 cards drawn = nothing left");
		CheckNear(DecksRemaining(52, 60), 0.0, "cursor past total clamps to 0.0, not negative", "defensive clamp -- an unconfirmed/misread cursor should never produce a negative decks-remaining");
	}

	void TestTrueCount()
	{
		std::printf("TestTrueCount:\n");

		CheckNear(TrueCount(4, 1.0), 4.0, "running +4 over a full deck is true count +4", "single-deck true count with a full deck remaining equals the running count itself");
		CheckNear(TrueCount(4, 0.5), 8.0, "running +4 over half a deck is true count +8", "same running count over half the cards doubles the true count");
		CheckNear(TrueCount(2, 4.0 / 52.0), 26.0, "true count at the floored minimum decks-remaining", "2 / (4/52) = 26 -- exercises the floor exactly at its boundary");
		CheckNear(TrueCount(2, 0.0), 26.0, "decksRemaining=0.0 is floored, not a division by zero", "0.0 must be floored to the same 4-card minimum as above, not produce inf/NaN");
	}

	void TestDeviations()
	{
		std::printf("TestDeviations:\n");

		Check(!ShouldTakeInsurance(2.9), "insurance not recommended below true count 3", "the Illustrious 18 insurance deviation triggers at true count >= 3, not below it");
		Check(ShouldTakeInsurance(3.0), "insurance recommended at true count 3", "boundary case: exactly 3.0 should trigger");
		Check(ShouldTakeInsurance(5.0), "insurance recommended well above true count 3", "clearly positive case");

		Check(!ShouldStandHard16VsTen(-0.5), "hard 16 vs 10 still Hits below true count 0", "the deviation only kicks in at a non-negative true count");
		Check(ShouldStandHard16VsTen(0.0), "hard 16 vs 10 Stands at true count 0", "boundary case: exactly 0.0 should trigger the deviation");
		Check(ShouldStandHard16VsTen(2.0), "hard 16 vs 10 Stands at a positive true count", "clearly positive case");
	}
}

int main()
{
	TestHiLoTag();
	TestRunningCount();
	TestDecksRemaining();
	TestTrueCount();
	TestDeviations();

	if (g_failures == 0)
	{
		std::printf("\nALL PASS\n");
		return 0;
	}

	std::printf("\n%d FAILURE(S)\n", g_failures);
	return 1;
}
