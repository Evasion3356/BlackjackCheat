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

	Release uses spdlog's global ASYNC thread pool (1 background thread,
	8192-slot queue, blocking-overflow policy so a burst never silently
	drops a diagnostic line) -- the calling thread just pushes an
	already-formatted message and returns immediately. Debug uses a plain
	SYNCHRONOUS logger instead -- deliberate, not a placeholder: Session 9
	live testing traced a real eject/reinject hang directly to this
	logger. `spdlog::async::thread_pool`'s destructor (thread_pool-inl.h)
	calls `t.join()` on its worker thread, and that destructor runs when
	this file's function-local static `logger` (holding the last
	shared_ptr to it) gets torn down during DLL unload -- i.e. from
	inside DllMain's DLL_PROCESS_DETACH. Joining a thread from inside
	DllMain is a well-documented Windows deadlock trap (the OS loader
	lock is held for the whole call), which manifested exactly as "eject
	just doesn't complete" -- not a crash, a hang. Debug is where this
	project's own workflow (build -> eject -> reinject -> repeat, many
	times an hour) actually exercises DLL unload constantly, so it gets
	the deadlock-proof synchronous logger; Release loads once at game
	launch and is never hot-ejected in normal play, so it keeps the
	async logger's lower per-call overhead (user directive: "ONLY for
	debug. Release should be [a]sync[hronous]"). The one place this
	still touches Release's own risk profile is the ordinary
	DLL_PROCESS_DETACH that happens at normal game exit -- untested this
	session (the eject hang was always caught via manual eject, not
	process exit), flagged as a real, if lower-probability, open
	question rather than assumed safe.

	SPDLOG_WCHAR_TO_UTF8_SUPPORT adds a second Log::Write overload taking
	a wide (L"...") format string + wide args -- spdlog formats and
	converts it to UTF-8 internally (details::os::wstr_to_utf8buf) before
	handing off to the (narrow) file sink. That's the one place a wide/
	narrow conversion happens for logging purposes, and it's entirely
	inside spdlog/this header -- callers like Config.cpp's
	ResolveIniPath() (wide path, deliberately does no narrow/wide
	conversion of its own -- see that file's header comment) can log a
	std::wstring directly without ever touching a conversion themselves.
*/

#pragma once

#define SPDLOG_USE_STD_FORMAT
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT

#include "..\external\spdlog\include\spdlog\spdlog.h"
#include "..\external\spdlog\include\spdlog\async.h"
#include "..\external\spdlog\include\spdlog\sinks\basic_file_sink.h"

#include <memory>
#include <utility>

namespace Log
{
	namespace detail
	{
		inline const std::shared_ptr<spdlog::logger>& GetLogger()
		{
			static const std::shared_ptr<spdlog::logger> logger = []
			{
				// Debug: synchronous, deliberately -- no background thread
				// pool means nothing for DLL_PROCESS_DETACH to deadlock
				// joining, see this file's own header comment for the real
				// eject-hang this fixed. Release: async, per user
				// directive -- Release is never hot-ejected in normal
				// play, so it keeps the lower per-call overhead.
#ifdef _DEBUG
				auto l = spdlog::basic_logger_mt<spdlog::synchronous_factory>(
					"BlackjackCheat", "BlackjackCheat.log", /*truncate*/ false);
#else
				auto l = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>(
					"BlackjackCheat", "BlackjackCheat.log", /*truncate*/ false);
#endif
				l->set_pattern("[%H:%M:%S.%e] %v");
				l->flush_on(spdlog::level::trace); // flush after every line, same durability as the old fclose-every-call behavior
				return l;
			}();
			return logger;
		}
	}

	template <typename... Args>
	void Write(spdlog::format_string_t<Args...> fmt, Args&&... args)
	{
		detail::GetLogger()->info(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Write(spdlog::wformat_string_t<Args...> fmt, Args&&... args)
	{
		detail::GetLogger()->info(fmt, std::forward<Args>(args)...);
	}
}
