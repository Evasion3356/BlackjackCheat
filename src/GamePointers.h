/*
	Resolves the small set of raw engine pointers ScriptHookRDR2's SDK
	doesn't expose but this mod needs -- specifically the live pool of
	running rage::scrThread instances, so we can find bjack_sp's own
	running thread and read its script-local variables directly (that's
	where the game's actual table-state struct lives -- see
	BlackjackCheat.cpp's header comment and docs/JOURNAL.md). Vendored
	unchanged from PokerCheat's GamePointers.h/.cpp -- generic
	infrastructure, nothing blackjack-specific.
*/

#pragma once

#include "..\external\RDR-Classes\script\scrThread.hpp"
#include "..\external\RDR-Classes\rage\atArray.hpp"
#include "..\external\RDR-Classes\rage\joaat.hpp"

#include <string>

namespace GamePointers
{
	// Lazily resolves and caches the address of RDR2.exe's live script
	// thread pool, via the same AOB signature HorseMenu uses (its
	// "ScriptThreads&RunScriptThreads" pattern -- see PatternScan.h and
	// GamePointers.cpp). Returns nullptr if the pattern isn't found (e.g.
	// a game update changed the surrounding code).
	rage::atArray<rage::scrThread*>* GetScriptThreads();

	// Finds the running scrThread for the given script name hash (e.g.
	// rage::Joaat("bjack_sp")), or nullptr if it's not currently running.
	// Same matching logic as HorseMenu's Scripts::FindScriptThread.
	rage::scrThread* FindScriptThread(rage::joaat_t scriptHash);

	// Reads script-local slot `index` of `thread` as a raw pointer/value --
	// i.e. *(void**)(thread->m_Stack + index * 8), matching HorseMenu's
	// ScriptLocal addressing. Returns nullptr if the thread has no stack
	// or the index is out of its declared stack size.
	void* ReadScriptLocal(rage::scrThread* thread, std::uint32_t index);

	// Returns the ADDRESS of script-local slot `index` -- i.e.
	// thread->m_Stack + index*8 itself, not what's stored there. Kept for
	// parity with PokerCheat's GamePointers.h even though this mod's
	// advisor-only design (no hand-rank native call) hasn't needed it yet.
	void* GetScriptLocalAddress(rage::scrThread* thread, std::uint32_t index);

	// Dumps every script-local slot of `thread` (or just [startSlot,
	// startSlot+count) for the overload below) to a JSONL file at
	// `outPath` -- one JSON object per line, every plausible
	// interpretation of that slot's raw 8 bytes (i32/u32/i64/f32/hex),
	// with NO theory about what any given slot means.
	//
	// Built specifically to break out of the trace-a-theory / probe-it /
	// re-trace-if-wrong loop this project (and PokerCheat before it) has
	// been running one offset at a time -- when a probe shows a field
	// reading garbage (e.g. BlackjackCheat's dealer hole card, Session 6),
	// the fastest way to find where the REAL data actually lives is to
	// dump a wide raw window and grep/jq it for the expected value
	// (a specific rank/suit pair, a known bet amount, etc.) rather than
	// guess-and-recheck one candidate offset at a time.
	//
	// The decompiled script's own f_N field names are exactly this
	// dump's "slot" column minus whatever struct's base slot you already
	// know (RDR2's script VM flattens nested struct fields to plain
	// linear offsets -- confirmed repeatedly in both this project and
	// PokerCheat's own derivations, e.g. Table.f_23 IS the slot at
	// kTableSlot+23) -- so a hit in the dump at slot S under a struct
	// known to start at base B is directly "f_(S-B)" in the decompile,
	// no further translation needed. Generic -- not blackjack-specific,
	// intended to be reused by any future script-memory reversing here,
	// same spirit as this file's other functions.
	bool DumpLocalStackJsonl(rage::scrThread* thread, const std::string& outPath);
	bool DumpLocalStackJsonl(rage::scrThread* thread, std::uint32_t startSlot, std::uint32_t count, const std::string& outPath);
}
