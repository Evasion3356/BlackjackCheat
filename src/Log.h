/*
	Async, thread-safe file logger backed by spdlog (vendored as a git
	submodule at external\spdlog, pinned to v1.17.0 -- `git submodule
	update --init` after a fresh clone). Writes BlackjackCheat.log next
	to the .asi, same location/append-forever behavior as before.

	Replaces an earlier hand-rolled Log::Write that called fopen_s(...,
	"a")/fclose on every single call -- synchronous disk I/O on whatever
	thread happened to call it, and not safe against concurrent callers
	(unsynchronized fopen_s calls could interleave). SPDLOG_USE_STD_FORMAT
	makes every Log::Write call use real std::format {}-style placeholders
	(compile-time checked against the argument list) instead of spdlog's
	bundled fmt library or the old printf-style %d/%s/%llX specifiers.

	Logging runs on spdlog's global async thread pool (1 background
	thread, 8192-slot queue, blocking-overflow policy so a burst never
	silently drops a diagnostic line) -- the calling thread just pushes
	an already-formatted message and returns immediately. The logger is
	a function-local static, constructed (and the background thread
	spawned) on the first Log::Write call of the process. In practice
	that first call currently happens during DllMain/DLL_PROCESS_ATTACH,
	via Config::Reload()'s own startup log line (see main.cpp) -- worth
	specifically watching on first load after this change, since
	spawning a thread from DllMain is usually fine only when that thread
	does nothing but wait on a condition variable and write to an
	already-open handle (true here: no LoadLibrary/COM/other-DLL-init
	dependency in spdlog's worker), but it's still a different risk
	shape than the fully synchronous logging DllMain relied on before.
	If that ever causes a load-order problem, the fix is a one-line
	change: spdlog::create_async -> a plain (synchronous) spdlog logger.

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
				auto l = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>(
					"BlackjackCheat", "BlackjackCheat.log", /*truncate*/ false);
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
