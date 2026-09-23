/*
	Thread-safe file logger backed by spdlog (vendored as a git submodule
	at external\spdlog, pinned to v1.17.0 -- `git submodule update --init`
	after a fresh clone). Writes BlackjackCheat.log next to the .asi, same
	location/append-forever behavior as before.

	Replaces an earlier hand-rolled Log::Write that called fopen_s(...,
	"a")/fclose on every single call -- synchronous disk I/O on whatever
	thread happened to call it, and not safe against concurrent callers
	(unsynchronized fopen_s calls could interleave). SPDLOG_USE_STD_FORMAT
	makes every Log::Write call use real std::format {}-style placeholders
	(compile-time checked against the argument list) instead of spdlog's
	bundled fmt library or the old printf-style %d/%s/%llX specifiers.

	SYNCHRONOUS in both Debug and Release -- deliberate, not a placeholder,
	and not merely the Debug-only fallback an earlier version of this file
	had. Session 9 live testing traced a real eject/reinject hang directly
	to spdlog's ASYNC thread pool: its destructor (thread_pool-inl.h) calls
	`t.join()` on its one worker thread, and that destructor runs when the
	last owning shared_ptr (the registry's, or this file's own static) gets
	torn down during DLL unload -- i.e. from inside DllMain's
	DLL_PROCESS_DETACH, which already holds the OS loader lock for the
	whole call. The worker thread can't actually finish exiting without
	that same lock (needed to fire DLL_THREAD_DETACH), so `join()` and the
	thread's own exit deadlock each other -- not a crash, a silent hang,
	confirmed both by this project's own live testing and by spdlog's own
	still-open upstream issues (gabime/spdlog#1183, #1214). No signaling
	scheme (event/semaphore instead of join, a hand-rolled thread instead
	of spdlog's) fixes this from inside DllMain -- Microsoft's own DLL
	guidance is a blanket "never synchronize with another thread from
	DllMain, this can deadlock," not "don't join specifically," because
	any wait can transitively need the loader lock through code paths that
	aren't stable across library versions. The only fixes that actually
	work are calling spdlog::shutdown() proactively during normal
	execution, well before unload starts (not from DllMain at all -- this
	project has no such hook exposed by ScriptHookRDR2's SDK, and would
	require a manual pre-eject user step otherwise), or never destroying
	the thread pool at all (a deliberate leak, trading the deadlock for an
	unbounded per-eject thread/handle leak and a real, if narrow, crash
	race if the worker is mid-write when FreeLibrary unmaps the DLL).
	Given all of that, going synchronous everywhere was chosen over any of
	those tradeoffs -- and it costs nothing measurable: `Log::Write` is
	never called from `OnTick()`'s per-frame hot path (see
	`BlackjackCheat.cpp`'s `OnTick`, which only calls `DrawOverlay()`);
	every other call site is either Debug-only (`#ifdef _DEBUG`) or a
	Probe*-/Dump*-prefixed function only reachable from the Debug-only F11
	menu. In Release, `Log::Write` fires a handful of times total per session
	(`DllMain`'s `Config::Reload`/`GamePointers::GetScriptThreads`,
	`ScriptMain`'s "started" line, `SetEnabled(true)`) -- never per-tick --
	so even a synchronous write+flush's worst-case latency on a tired
	5400rpm HDD (low single-digit milliseconds) is a one-time cost buried
	in game-load/toggle time, not a per-frame one.

	SPDLOG_WCHAR_TO_UTF8_SUPPORT adds a second Log::Write overload taking
	a wide (L"...") format string + wide args -- spdlog formats and
	converts it to UTF-8 internally (details::os::wstr_to_utf8buf) before
	handing off to the (narrow) file sink. That's the one place a wide/
	narrow conversion happens for logging purposes, and it's entirely
	inside spdlog/this header -- callers like Config.cpp's
	ResolveIniPath() (wide path, deliberately does no narrow/wide
	conversion of its own -- see that file's header comment) can log a
	std::wstring directly without ever touching a conversion themselves.

	Log::Trace is a Debug-only step tracer for the injection phase
	(DllMain attach/detach, Config load, the scrThread-pool AOB scan,
	ScriptMain startup) -- added after a user report of a load-screen crash
	with no BlackjackCheat.log at all, i.e. somewhere before the first
	Log::Write ever ran. Each line goes to OutputDebugStringA (tagged
	"[BlackjackCheat]", viewable with Sysinternals DebugView) BEFORE the
	file sink, so the last step reached is still visible even when opening
	the log file itself is the thing that dies. Release compiles every
	call to an empty inline body (format strings are still compile-time
	checked) -- keep Trace arguments free of side effects, since the
	argument expressions themselves are still evaluated there.

	Where the file goes: next to the .asi when that folder is writable,
	otherwise %LOCALAPPDATA%\RDR2ASIMods\ (see LogFallback.h), with a first
	line saying which path was rejected. Creating the logger never throws --
	if nothing is writable, Log::Write silently does nothing. It used to throw
	spdlog_ex out of the first Log::Write, which runs early enough in game
	load to crash RDR2 when the install folder is read-only (e.g. a Rockstar
	Launcher install under C:\Program Files). tests/LogFallbackTests covers
	exactly that scenario.
*/

#pragma once

#define SPDLOG_USE_STD_FORMAT
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#define SPDLOG_WCHAR_FILENAMES

#include "..\external\spdlog\include\spdlog\spdlog.h"
#include "..\external\spdlog\include\spdlog\sinks\basic_file_sink.h"

#include "LogFallback.h"

#include <memory>
#include <string>
#include <utility>

#ifdef _DEBUG
#include <windows.h>
#include <format>
#endif

namespace Log
{
	namespace detail
	{
		// Logs to preferredDir + fileName, or to fallbackDir + fileName when
		// that can't be written. Never throws; nullptr if neither works.
		// Not registered with spdlog's global registry, so a second call with
		// the same name (tests, a hot-reload) can't throw "already exists".
		inline std::shared_ptr<spdlog::logger> CreateLogger(const std::string& name, const std::wstring& preferredDir,
			const std::wstring& fileName, const std::wstring& fallbackDir)
		{
			try
			{
				const LogFallback::Resolved resolved = LogFallback::Resolve(preferredDir, fileName, fallbackDir);

				// The preferred path can still fail inside spdlog after passing
				// Resolve()'s probe (e.g. locked in between) -- retry at the
				// fallback before giving up.
				const std::wstring candidates[2] = {
					resolved.path,
					resolved.usedFallback || fallbackDir.empty() ? std::wstring() : fallbackDir + fileName };
				for (int i = 0; i < 2; i++)
				{
					if (candidates[i].empty())
						continue;
					try
					{
						if (i == 1)
							LogFallback::EnsureDirectory(fallbackDir);
						auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(candidates[i], false);
						auto logger = std::make_shared<spdlog::logger>(name, std::move(sink));
						logger->set_pattern("[%H:%M:%S.%e] %v");
						logger->flush_on(spdlog::level::trace);
						if (resolved.usedFallback || i == 1)
							logger->info("Log redirected here: could not write {}",
								LogFallback::ToUtf8(resolved.usedFallback ? resolved.rejectedPath : preferredDir + fileName));
						return logger;
					}
					catch (...)
					{
					}
				}
			}
			catch (...)
			{
			}
			return nullptr;
		}

		inline const std::shared_ptr<spdlog::logger>& GetLogger()
		{
			static const std::shared_ptr<spdlog::logger> logger = CreateLogger(
				"BlackjackCheat", LogFallback::ModuleDirectory(), L"BlackjackCheat.log", LogFallback::FallbackDirectory());
			return logger;
		}
	}

	template <typename... Args>
	void Write(spdlog::format_string_t<Args...> fmt, Args&&... args)
	{
		if (const auto& logger = detail::GetLogger())
			logger->info(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Write(spdlog::wformat_string_t<Args...> fmt, Args&&... args)
	{
		if (const auto& logger = detail::GetLogger())
			logger->info(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
	{
#ifdef _DEBUG
		std::string line = std::format(fmt, std::forward<Args>(args)...);
		std::string debugLine = "[BlackjackCheat] TRACE " + line + "\n";
		OutputDebugStringA(debugLine.c_str());
		if (const auto& logger = detail::GetLogger())
			logger->info("TRACE {}", line);
#else
		(void)fmt;
		((void)args, ...);
#endif
	}

	template <typename... Args>
	void Trace(spdlog::wformat_string_t<Args...> fmt, Args&&... args)
	{
#ifdef _DEBUG
		std::wstring line = std::format(fmt, std::forward<Args>(args)...);
		std::wstring debugLine = L"[BlackjackCheat] TRACE " + line + L"\n";
		OutputDebugStringW(debugLine.c_str());
		if (const auto& logger = detail::GetLogger())
			logger->info(L"TRACE {}", line);
#else
		(void)fmt;
		((void)args, ...);
#endif
	}
}
