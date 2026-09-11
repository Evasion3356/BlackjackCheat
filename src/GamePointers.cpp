#include "GamePointers.h"
#include "PatternScan.h"
#include "Log.h"

#include <cstdio>
#include <cstring>

namespace
{
	// HorseMenu's "ScriptThreads&RunScriptThreads" signature -- see
	// D:\Backup\Stuff\RDR2 Shit\HorseMenu\src\game\pointers\Pointers.cpp,
	// line ~83. The matched instruction is `LEA reg, [rip+disp32]`
	// (48 8D 0D), so the RIP-relative operand starts right after that
	// 3-byte opcode.
	constexpr const char* kScriptThreadsPattern = "48 8D 0D ? ? ? ? E8 ? ? ? ? EB 0B 8B 0D";
	constexpr int kScriptThreadsOperandOffset = 3;
}

namespace GamePointers
{
	rage::atArray<rage::scrThread*>* GetScriptThreads()
	{
		static rage::atArray<rage::scrThread*>* cached = []() -> rage::atArray<rage::scrThread*>*
		{
			auto match = PatternScan::FindInMainModule(kScriptThreadsPattern);
			if (!match)
			{
				Log::Write("GamePointers::GetScriptThreads: pattern not found");
				return nullptr;
			}

			auto resolved = PatternScan::ResolveRip(*match, kScriptThreadsOperandOffset);
			Log::Write("GamePointers::GetScriptThreads: pattern matched at 0x{:X}, resolved to 0x{:X}",
				static_cast<unsigned long long>(*match), static_cast<unsigned long long>(resolved));
			return reinterpret_cast<rage::atArray<rage::scrThread*>*>(resolved);
		}();

		return cached;
	}

	rage::scrThread* FindScriptThread(rage::joaat_t scriptHash)
	{
		auto threads = GetScriptThreads();
		if (!threads)
			return nullptr;

		for (auto& thread : *threads)
		{
			if (thread && thread->m_Context.m_ThreadId && thread->m_Context.m_ScriptHash == scriptHash)
				return thread;
		}

		return nullptr;
	}

	void* ReadScriptLocal(rage::scrThread* thread, std::uint32_t index)
	{
		if (!thread || !thread->m_Stack)
			return nullptr;

		if (thread->m_Context.m_StackSize <= index)
			return nullptr;

		return reinterpret_cast<void**>(thread->m_Stack)[index];
	}

	void* GetScriptLocalAddress(rage::scrThread* thread, std::uint32_t index)
	{
		if (!thread || !thread->m_Stack)
			return nullptr;

		if (thread->m_Context.m_StackSize <= index)
			return nullptr;

		return reinterpret_cast<void**>(thread->m_Stack) + index;
	}

	bool DumpLocalStackJsonl(rage::scrThread* thread, std::uint32_t startSlot, std::uint32_t count, const char* outPath)
	{
		if (!thread || !thread->m_Stack)
		{
			Log::Write("GamePointers::DumpLocalStackJsonl: no thread/stack");
			return false;
		}

		std::uint32_t stackSize = thread->m_Context.m_StackSize;
		std::uint32_t end = startSlot + count; // count is caller-controlled and small in practice; overflow would only make this MORE conservative via the clamp below, never read out of bounds
		if (end > stackSize || end < startSlot)
			end = stackSize;

		if (startSlot >= end)
		{
			Log::Write("GamePointers::DumpLocalStackJsonl: startSlot {} >= stack size {}, nothing to dump", startSlot, stackSize);
			return false;
		}

		FILE* f = nullptr;
		fopen_s(&f, outPath, "w");
		if (!f)
		{
			Log::Write("GamePointers::DumpLocalStackJsonl: failed to open {}", outPath);
			return false;
		}

		// Every slot is 8 raw bytes (same addressing ReadScriptLocal uses)
		// -- dumped as every plausible interpretation rather than picking
		// one, since the whole point is not yet knowing which is right.
		const std::uint64_t* slots = reinterpret_cast<const std::uint64_t*>(thread->m_Stack);
		for (std::uint32_t i = startSlot; i < end; i++)
		{
			std::uint64_t raw = slots[i];
			std::uint32_t u32 = static_cast<std::uint32_t>(raw);
			std::int32_t i32 = static_cast<std::int32_t>(u32);
			float f32;
			std::memcpy(&f32, &u32, sizeof(f32));

			fprintf(f, "{\"slot\":%u,\"i32\":%d,\"u32\":%u,\"i64\":%lld,\"f32\":%g,\"hex\":\"%016llX\"}\n",
				i, i32, u32, static_cast<long long>(static_cast<std::int64_t>(raw)),
				static_cast<double>(f32), static_cast<unsigned long long>(raw));
		}

		fclose(f);
		Log::Write("GamePointers::DumpLocalStackJsonl: wrote slots [{}, {}) to {}", startSlot, end, outPath);
		return true;
	}

	bool DumpLocalStackJsonl(rage::scrThread* thread, const char* outPath)
	{
		if (!thread || !thread->m_Stack)
			return false;

		return DumpLocalStackJsonl(thread, 0, thread->m_Context.m_StackSize, outPath);
	}
}
