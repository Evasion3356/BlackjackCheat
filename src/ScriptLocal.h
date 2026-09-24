/*
	Chainable script-local field/array accessor, adapted from HorseMenu's
	own `game/rdr/ScriptLocal.hpp`/`ScriptGlobal.hpp`
	(D:\Backup\Stuff\RDR2 Shit\HorseMenu\src\game\rdr\) by way of
	..\DominoCheat\src\ScriptLocal.h (same class; the only change here is
	that Index() is constexpr, so a chain can be pinned to a live-confirmed
	slot with a static_assert -- see BlackjackCheat.cpp's layout block).

	Why: BlackjackCheat.cpp used to hand-flatten every nested `.f_N` access
	into one absolute slot number and re-derive "+1 for the array's size
	word" by hand at each field. That produced most of its live-corrected
	offsets (table 756 -> 757, seat hands 8 -> 10, bets 4 -> 5, deck
	cursor/count, the presentation copy 17 -> 18), and left its "Table" base
	pointing one word past the real struct, so every table-level field was
	numbered one lower than the decompile's own f_N.

	The rule for which At() overload to use is MECHANICAL, read straight off
	the decompile's own syntax at each step:
	  - `something.f_N` (no brackets) -> At(N). No size word implied.
	  - `something[i]` (brackets; the decompiler notes the element
	    stride S in a comment) -> At(i, S). Skips the array's leading
	    size word automatically, then advances i * S.
	It does NOT know a struct's size (the stride S) or whether a value
	is read through a native instead of `[i]` -- those still come from the
	decompile or a live dump.
*/

#pragma once

#include "GamePointers.h"

#include <cstdint>
#include <cstring>

class ScriptLocal
{
	rage::scrThread* m_Thread;
	std::uint32_t m_Index;

public:
	constexpr ScriptLocal(rage::scrThread* thread, std::uint32_t index) :
		m_Thread(thread), m_Index(index)
	{
	}

	// Plain nested-struct field access -- the decompile shows `.f_N`.
	constexpr ScriptLocal At(std::uint32_t fieldOffset) const
	{
		return ScriptLocal(m_Thread, m_Index + fieldOffset);
	}

	// Array-element access -- the decompile shows `something[i]` (stride S).
	// Skips the array's size word, then advances index * elementStride.
	constexpr ScriptLocal At(std::uint32_t index, std::uint32_t elementStride) const
	{
		return ScriptLocal(m_Thread, m_Index + 1 + index * elementStride);
	}

	constexpr std::uint32_t Index() const { return m_Index; }

	// The value lives in the low 4 bytes of the 8-byte slot.
	std::int32_t AsInt32() const
	{
		void* raw = GamePointers::ReadScriptLocal(m_Thread, m_Index);
		return static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(raw));
	}

	// The project's one memory write (the bet hotkeys' Tab = bet max, see
	// BlackjackCheat.cpp's UpdateBetHotkeys()). Writes the low 4 bytes,
	// where AsInt32() reads. Returns false if the slot is out of range.
	bool SetInt32(std::int32_t value) const
	{
		void* address = GamePointers::GetScriptLocalAddress(m_Thread, m_Index);
		if (!address)
			return false;
		*static_cast<std::int32_t*>(address) = value;
		return true;
	}

	// Same slot, bit-reinterpreted as IEEE-754 (for a `float` local).
	float AsFloat() const
	{
		void* raw = GamePointers::ReadScriptLocal(m_Thread, m_Index);
		std::uint32_t bits = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(raw));
		float value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}
};
