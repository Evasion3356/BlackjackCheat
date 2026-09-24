/*
	RoundRecord.h -- the round log's JSONL line format, shared by the mod
	(which writes BlackjackCheat_rounds.jsonl, Debug build only -- see
	BlackjackCheat.cpp's RoundRecorder section) and
	tests/BlackjackDeckSimTests.cpp (which replays lines copied from it
	into tests/fixtures/rounds.jsonl). No game dependency.

	Two line types, each a flat JSON object on its own line so any single
	line can be copied into the fixture file as a self-contained test case:

	  {"type":"decision", ...}  one per hit/stand/double/split advice shown
	      for my hand, with every input DetermineFullAdvice() took:
	      playerRanks, dealerRanks, futureRanks (the deck from the live
	      cursor), canDouble, canSplit, isSplitAceHand, the seats still to
	      act after mine (seatsAfterKnown, seatsAfterRanks -- every seat's
	      cards back to back -- seatsAfterCounts, seatsAfterCanAfford),
	      plus the action the mod showed ("action"), what I actually did
	      ("taken": Hit/Stand/Double/Split, HitOrDouble when a last-action
	      push can't tell, Unknown if never seen) and "followedAdvice".
	      Older lines carry isLastBeforeDealer instead; the replay test
	      still reads it. Money (bet, bankroll*, net) is in cents.
	  {"type":"round", ...}     one per round, written when it ends: the
	      whole deck as dealt (deckRanks, deckRanks[0] = first card dealt),
	      seatsDealt, mySeat -- every input EvaluatePreDealBetting() takes --
	      plus the betting advice shown before the deal ("betting"), and
	      the end state: my final hands and their outcome vs the dealer,
	      the dealer's final hand ("dealerRanks") vs the hand replayed off
	      the deck with the AI model and the cards I actually drew
	      ("myCardsDrawn", "dealerPredicted", "dealerPredictionMatch" --
	      BlackjackDeckSim::ReplayDealer()),
	      where that end state came from ("endStateFrom": the lagging
	      presentation copy, or the last live state as a fallback), and
	      the money: bet, bankrollBeforeRound (bankroll + bet during
	      betting), bankrollAfter and net.

	Cards are ranks 2..14 (J=11, Q=12, K=13, A=14) in the *Ranks arrays;
	the plain-text "deck"/"cards"/"dealer" strings are for reading only.

	To turn a line into a test: copy it into tests/fixtures/rounds.jsonl
	and add "expectBetting":"Low" and/or "expectDealer":[...] (round lines;
	expectDealer is the dealer's real final ranks, i.e. a copy of
	dealerRanks) or "expectAction":"Stand" (decision lines). Lines with no expect* key are skipped.

	The reader below only understands this file's own flat format (a
	top-level key's int, bool, string or int array) -- not general JSON.
	Same hand-written approach as GamePointers::DumpLocalStackJsonl.
*/

#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace RoundRecord
{
	class JsonLine
	{
	public:
		JsonLine& Add(std::string_view key, std::int64_t value)
		{
			Key(key);
			m_out << value;
			return *this;
		}

		JsonLine& Add(std::string_view key, bool value)
		{
			Key(key);
			m_out << (value ? "true" : "false");
			return *this;
		}

		JsonLine& Add(std::string_view key, std::string_view value)
		{
			Key(key);
			Quoted(value);
			return *this;
		}

		JsonLine& Add(std::string_view key, const char* value)
		{
			return Add(key, std::string_view(value));
		}

		JsonLine& Add(std::string_view key, const std::int32_t* values, std::int32_t count)
		{
			Key(key);
			m_out << '[';
			for (std::int32_t i = 0; i < count; i++)
				m_out << (i ? "," : "") << values[i];
			m_out << ']';
			return *this;
		}

		JsonLine& Add(std::string_view key, const std::vector<std::string>& values)
		{
			Key(key);
			m_out << '[';
			for (std::size_t i = 0; i < values.size(); i++)
			{
				if (i)
					m_out << ',';
				Quoted(values[i]);
			}
			m_out << ']';
			return *this;
		}

		std::string Str() const
		{
			return m_out.str() + "}";
		}

	private:
		void Key(std::string_view key)
		{
			m_out << (m_first ? "{" : ",");
			m_first = false;
			Quoted(key);
			m_out << ':';
		}

		void Quoted(std::string_view text)
		{
			m_out << '"';
			for (char c : text)
			{
				if (c == '"' || c == '\\')
					m_out << '\\';
				m_out << c;
			}
			m_out << '"';
		}

		std::ostringstream m_out;
		bool m_first = true;
	};

	// Position just past `"key":` (skipping spaces), or npos. Only matches
	// a key, not the same text inside a string value, since a key is
	// always directly preceded by '{' or ','.
	inline std::size_t FindValue(std::string_view line, std::string_view key)
	{
		const std::string needle = "\"" + std::string(key) + "\"";
		for (std::size_t pos = line.find(needle); pos != std::string_view::npos; pos = line.find(needle, pos + 1))
		{
			std::size_t before = pos;
			while (before > 0 && line[before - 1] == ' ')
				before--;
			if (before == 0 || (line[before - 1] != '{' && line[before - 1] != ','))
				continue;

			std::size_t after = pos + needle.size();
			while (after < line.size() && line[after] == ' ')
				after++;
			if (after >= line.size() || line[after] != ':')
				continue;
			after++;
			while (after < line.size() && line[after] == ' ')
				after++;
			return after;
		}
		return std::string_view::npos;
	}

	inline bool ParseInt(std::string_view line, std::size_t& pos, std::int32_t& out)
	{
		bool negative = pos < line.size() && line[pos] == '-';
		if (negative)
			pos++;
		if (pos >= line.size() || line[pos] < '0' || line[pos] > '9')
			return false;
		std::int64_t value = 0;
		while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9')
			value = value * 10 + (line[pos++] - '0');
		out = static_cast<std::int32_t>(negative ? -value : value);
		return true;
	}

	inline bool GetInt(std::string_view line, std::string_view key, std::int32_t& out)
	{
		std::size_t pos = FindValue(line, key);
		return pos != std::string_view::npos && ParseInt(line, pos, out);
	}

	inline bool GetBool(std::string_view line, std::string_view key, bool& out)
	{
		std::size_t pos = FindValue(line, key);
		if (pos == std::string_view::npos)
			return false;
		if (line.substr(pos, 4) == "true")
		{
			out = true;
			return true;
		}
		if (line.substr(pos, 5) == "false")
		{
			out = false;
			return true;
		}
		return false;
	}

	inline bool GetString(std::string_view line, std::string_view key, std::string& out)
	{
		std::size_t pos = FindValue(line, key);
		if (pos == std::string_view::npos || line[pos] != '"')
			return false;
		out.clear();
		for (pos++; pos < line.size(); pos++)
		{
			if (line[pos] == '"')
				return true;
			if (line[pos] == '\\' && pos + 1 < line.size())
				pos++;
			out += line[pos];
		}
		return false;
	}

	inline bool GetIntArray(std::string_view line, std::string_view key, std::vector<std::int32_t>& out)
	{
		std::size_t pos = FindValue(line, key);
		if (pos == std::string_view::npos || line[pos] != '[')
			return false;
		out.clear();
		pos++;
		while (pos < line.size())
		{
			while (pos < line.size() && (line[pos] == ' ' || line[pos] == ','))
				pos++;
			if (pos < line.size() && line[pos] == ']')
				return true;
			std::int32_t value = 0;
			if (!ParseInt(line, pos, value))
				return false;
			out.push_back(value);
		}
		return false;
	}
}
