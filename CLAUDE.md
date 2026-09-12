# BlackjackCheat

A ScriptHookRDR2 ASI mod that advises the player during RDR2's
single-player blackjack minigame (`bjack_sp`). Built as a sibling of
`../PokerCheat` (itself a sibling of `../CollectorOffline`), reusing the
exact same toolchain and conventions -- see those projects' `CLAUDE.md`
files for the full backstory on why this stack (ScriptHookRDR2 + native
C++, not an injected mod-menu framework) was chosen.

**Current status: advisor HUD + deterministic deck-ahead prediction.
Session 6 got the first LIVE memory confirmation** -- `kTableFieldOffset`,
`kSeatHandsOffset`, `kHandCountOffset`, and `kHandValueOffset` are now
CONFIRMED against a running game (via a before/after diff of a new raw
stack-dump tool, see `docs/JOURNAL.md` Session 6), and all three needed
correcting from their original static-trace values. Everything else in
`src/BlackjackCheat.cpp`'s file header comment is still a STATIC TRACE
ONLY, the same position PokerCheat started from before its own many
rounds of live confirmation (see `../PokerCheat/docs/JOURNAL.md`) --
Session 6's corrections are proof that matters: the original guesses for
these same four constants were wrong (off by one or two words each).
`src/BlackjackCheat.cpp`'s file header comment lays out every candidate
offset with its own confidence level and the exact decompiled call sites
it was traced from; `docs/JOURNAL.md`'s Session 2/3/4/5 entries have the
full citation trail.

The PRIMARY feature is deck-ahead prediction (the blackjack equivalent of
PokerCheat's `BuildPredictedBoard()`): since bjack_sp deals an entire
round from one fixed, already-shuffled 52-card deck (Session 3 finding),
the dealer's hole card is real already-dealt data (just hidden on screen)
and the dealer's own forced stand-on-17 draw-out can be simulated
deterministically straight off the deck array -- see
`SimulateDealerOutcome()`/`UpdateDeckPrediction()` in
`src/BlackjackCheat.cpp`. Session 5 traced the game's actual turn order
(strictly ascending seat 0->1->2->3, dealer last, confirmed via
`func_718`'s own state machine) and found non-player seats are real AND
fully deterministic (no randomness in the decision, only in when it's
submitted -- see `func_1002`/`func_623`), which meant the prediction
could be made self-correcting: it now re-simulates from the LIVE deck
cursor every tick instead of freezing a single round-start guess, so it's
provably exact by the time the dealer's real turn begins, with no need to
replicate the AI's own decision table. A separate frozen round-start
baseline still self-validates automatically every round via a Debug-only
"PredictionCheck" log line, no F11 interaction needed. Card counting
(`src/BlackjackCardCounting.h`, Hi-Lo running/true count + insurance/
16-vs-10 deviations) is SECONDARY -- kept because it's still correct and
tested, but superseded by direct deck reads for this specific game.
`src/BlackjackHandEval.h` (hand value + basic-strategy hit/stand/double/
split logic) and `BlackjackCardCounting.h` are both pure math with NO
game-memory dependency and ARE fully correct/tested regardless of
whether the struct offsets are right.

Rule assumptions traced directly from the game's own logic rather than a
generic textbook chart (see `BlackjackHandEval.h`'s own header comment for
citations): dealer stands soft 17, double any 2 cards, split requires
exact rank match and is capped at one split, split Aces get exactly one
card each with NO further action (reverses an earlier Session 2 finding
-- see Session 3), no surrender. Session 3 also found bjack_sp deals from
a SINGLE 52-card deck, rebuilt+reshuffled every round -- NOT a 4-8 deck
shoe as originally assumed; the basic-strategy chart itself has NOT yet
been corrected for single-deck play (a handful of borderline hands differ
between single- and multi-deck charts) -- a known, explicitly flagged
gap, see `docs/JOURNAL.md` Session 3.

Scope is deliberately advisor-only (read hand/deck data, show values +
basic strategy + deterministic deck-ahead prediction + card count/
insurance advice) -- read-only, no memory writes, no native game-state
manipulation. "No deck prediction" was the original Session 1-3 scope;
Session 4 added it after the user pointed out the deck offsets were
already traced and unused -- still squarely read-only, same risk category
as PokerCheat's own predicted board. Read `docs/JOURNAL.md` for the full
derivation history and what a future session needs to do to confirm the
offsets, and `docs/PITFALLS.md` before touching anything native-related.

## Coding conventions

**No C-style strings/buffers.** No `char buf[N]` locals, `sprintf_s`,
`strcpy_s`, or hand-rolled size-tracked buffers anywhere in this project's
own code -- `std::string`/`std::ostringstream` only (see `FormatCard`/
`FormatCardRun`/`WrapBgFormatText` in `src/BlackjackCheat.cpp` for the
established pattern; their own header comments explain a real crash this
already caused once via a fixed `char[]` + `sprintf_s`/`strcat_s` combo).
The one unavoidable exception is the literal call-site boundary into a
ScriptHookRDR2 native that requires `char*` (e.g. `GRAPHICS::DRAW_SPRITE`,
`TEXTURE::HAS_STREAMED_TEXTURE_DICT_LOADED`) -- build the value as
`std::string` and pass `const_cast<char*>(str.c_str())` only at that
call, never a manual fixed-size buffer upstream of it. This applies even
when porting/adapting code from `../PokerCheat`, which still has some
older char[]-based helpers of its own -- don't carry that pattern over.

## Build & deploy

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" BlackjackCheat.vcxproj /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal
```

The project's `PostBuildEvent` copies the built `.asi` straight into the
game folder (`E:\SteamLibrary\steamapps\common\Red Dead Redemption 2`).
**RDR2.exe must be closed first** or the copy fails with a file-in-use
error -- check `tasklist //FI "IMAGENAME eq RDR2.exe"` before every build.

A `Debug|x64` configuration also exists (`/p:Configuration=Debug` in the
same command) -- `/MTd` static debug CRT, optimizations disabled, PDB
deployed alongside the `.asi`. This is also the configuration with the F11
test menu and the `Probe*` diagnostics (see below) -- Release enables the
advisor unconditionally with no menu at all, same convention as
PokerCheat.

Runtime log: `<game folder>\BlackjackCheat.log`, written by `Log::Write`
(see `src/Log.h`).

## Tests

`tests/BlackjackHandEvalTests.vcxproj` unit-tests `src/BlackjackHandEval.h`
(hand value + basic strategy) in complete isolation from the game -- plain
console app, no ScriptHookRDR2/game dependency, links against the exact
same header the mod itself includes:

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" tests\BlackjackHandEvalTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
bin\Debug\BlackjackHandEvalTests.exe
```

`tests/BlackjackCardCountingTests.vcxproj` does the same for
`src/BlackjackCardCounting.h` (Hi-Lo tagging, running/true count,
decks-remaining, the two modeled deviations). Card counting itself was
removed from the mod in Session 7 (user directive: "remove this card
counting crap, just have it do pure cheating") -- this header/test
project are left on disk unused rather than deleted (no version control
in this folder to undo a deletion with):

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" tests\BlackjackCardCountingTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
bin\Debug\BlackjackCardCountingTests.exe
```

`tests/BlackjackDeckSimTests.vcxproj` unit-tests `src/BlackjackDeckSim.h`
(the deterministic "pure cheat" hit/stand/double engine that replaced
`GetBasicStrategyAction()` as the mod's actual advice source in Session
7 -- see that header's own header comment for the full derivation and the
live bug, a hard 11 always advising Stand, that this test suite was
written specifically to pin down and prevent regressing):

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" tests\BlackjackDeckSimTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
bin\Debug\BlackjackDeckSimTests.exe
```

All three exit 0 and print `ALL PASS` if every case passes; nonzero with
a `[FAIL]` line per failing case otherwise. Add a new case before
changing anything in any of these headers -- PokerCheat's own hand-eval
logic went unverified against real hands for 9 sessions previously
specifically because there was no automated check on it (see
`../PokerCheat/docs/JOURNAL.md`, Session 9); don't repeat that here.

In-game (Debug build only): press F11 for the test menu (NUMPAD 8/2 move,
NUMPAD 5 select, NUMPAD 0/Backspace/F11 back -- same controls as
PokerCheat/CollectorOffline).

## Source layout

- `src/main.cpp` -- `DllMain`, registers `ScriptMain` with ScriptHookRDR2
  and the keyboard handler. Vendored from PokerCheat with only the
  identifiers renamed -- no blackjack-specific logic.
- `src/script.h` / `script.cpp` -- entry point (`ScriptMain`) and the F11
  menu shell (Toggle, four `Probe*` diagnostics, Reload Config).
- `src/BlackjackCheat.h` / `.cpp` -- the actual cheat module. `Enabled`
  flag, `Toggle()`, `OnTick()`. **Its file header comment in `.cpp` is the
  single most important thing to read before touching struct offsets** --
  it documents every candidate offset's derivation and confidence level.
  Session 4's deck-ahead prediction (`SimulateDealerOutcome()`,
  `UpdateDeckPrediction()`, `DrawDealerHoleCardStatus()`,
  `ProbeDeckPrediction()`) lives here too, since it depends on the same
  struct offsets, not in a separate pure-math header.
- `src/BlackjackHandEval.h` -- self-contained hand-value + basic-strategy
  logic (hard/soft totals, bust/blackjack detection, hit/stand/double/split
  recommendation, split-Ace forced-stand handling). Zero game dependency,
  shared by both the mod and `tests/BlackjackHandEvalTests.cpp`. Chart is
  still the standard multi-deck one even though Session 3 found bjack_sp
  is actually single-deck -- see that file's own header comment for the
  gap and what would need re-deriving.
- `src/BlackjackCardCounting.h` -- self-contained Hi-Lo running/true count
  + insurance/16-vs-10 deviations. SECONDARY as of Session 4 (superseded by
  direct deck-ahead reading for this specific game, see its own header
  comment) but still correct/tested. Zero game dependency, shared by the
  mod and `tests/BlackjackCardCountingTests.cpp`. See its own header comment
  for the single-deck-reshuffled-every-round caveat that limits how much
  a count-based edge actually applies to this specific game.
- `src/scriptmenu.h/.cpp`, `src/keyboard.h/.cpp` -- vendored unchanged from
  PokerCheat (itself adapted from the ScriptHookRDR2 SDK's NativeTrainer
  sample).
- `src/Log.h` -- thread-safe file logger (`BlackjackCheat.log`) built on
  spdlog (`external\spdlog`, a real git submodule -- see "External
  resources" below). `Log::Write` takes `std::format`-style `{}`
  placeholders (compile-time checked), not printf's `%d`/`%s`. Replaced
  an earlier hand-rolled version that reopened the file with
  `fopen_s`/`fclose` on every call (synchronous, and not safe against
  concurrent callers). Debug uses a plain SYNCHRONOUS spdlog logger;
  Release uses spdlog's ASYNC logger (1 background thread) -- this split
  is deliberate, not a placeholder: a live eject/reinject hang was traced
  to the async thread pool's destructor joining its worker thread from
  inside `DLL_PROCESS_DETACH` (a well-known Windows deadlock trap), which
  this project's Debug-heavy build-eject-reinject workflow hits
  constantly and Release effectively never does -- see that header's own
  header comment and `docs/JOURNAL.md`'s Session 9 fourth finding for the
  full mechanism and the one open caveat (Release's own risk at ordinary
  game-exit DLL_PROCESS_DETACH, not yet tested).
- `src/GamePointers.h/.cpp`, `src/PatternScan.h/.cpp` -- generic
  scrThread-pool resolution / AOB pattern scanning, vendored unchanged
  from PokerCheat (nothing poker- or blackjack-specific in either file).
- `src/Config.h/.cpp` -- INI-backed HUD toggles (`BlackjackCheat.ini`),
  same inipp-based approach as PokerCheat's Config.h/.cpp (see that
  file's header comment for the full mINI-vs-inipp backstory).
- `src/ExtraNatives.h` -- `UIDEBUG::_BG_DISPLAY_TEXT`/`_BG_SET_TEXT_COLOR`,
  the only text-draw native pair confirmed to actually render on this game
  build (1491.50) -- `UI::DRAW_TEXT`/`SET_TEXT_COLOR_RGBA` are nullsub here
  (a game-build fact carried over from PokerCheat's own research, not
  poker-specific -- see PokerCheat.cpp's `DrawFontTest()` header comment
  for the full derivation).

## External resources

- `D:\Backup\Stuff\RDR2 Shit\Scripts\rdr2-scripts-decompiled\1491.50\script_rel\bjack_sp.ysc.c`
  -- the actual target, already decompiled for our exact game build
  (1491.50), ~42k lines. `bjack_launch_sp.ysc.c` (~16.5k lines) is the
  launcher/wrapper. No `act_gen_blackjack.ysc.c`-style shared framework
  file was found next to it (unlike poker's `act_gen_poker.ysc.c`) --
  everything traced so far was self-contained in `bjack_sp.ysc.c` itself.
- `..\ScriptHookSDK\` -- local copy of Alexander Blade's ScriptHookRDR2 SDK,
  same shared copy PokerCheat/CollectorOffline use. The `ScriptHookRDR2.dll`
  runtime itself (not redistributed here) must be downloaded from
  http://www.dev-c.com/rdr2/scripthookrdr2/ matching game build 1491.50.
- `external\RDR-Classes\`, `external\inipp\` -- vendored copies (same
  content as PokerCheat's own `external\`, copied rather than shared since
  PokerCheat's copy lives inside its own project folder, not at the
  `RDR2 Shit` root).
- `external\spdlog\` -- a real git submodule (`.gitmodules`), unlike the
  two entries above, pinned to release tag v1.17.0
  (https://github.com/gabime/spdlog). Backs `src/Log.h`. **A fresh clone
  of this repo needs `git submodule update --init` before it'll build**
  -- the headers won't exist otherwise. Header-only (no `.cpp`/`.lib` to
  add to the vcxproj); built with `SPDLOG_USE_STD_FORMAT` +
  `SPDLOG_WCHAR_TO_UTF8_SUPPORT` (both defined in `src/Log.h` before
  including any spdlog header) rather than spdlog's bundled fmt.
- `..\PokerCheat\` -- read `docs/JOURNAL.md` there for the actual
  live-probing methodology (trace statically, run a `Probe*` menu item
  in-game, compare the log against the real screen, re-derive when wrong)
  this project needs to go through next -- it hasn't started that process
  yet, PokerCheat's history is the template for how to do it.
- `..\CollectorOffline\CLAUDE.md` -- documents
  `D:\Backup\Stuff\RDR2 Shit\EXEs\1491.50\RDR2_Dumped.exe.i64`, an
  already-analyzed IDA database for this exact build, useful if a struct
  offset needs confirming below the script-source level (shouldn't be
  needed for this mod -- everything so far is plain script-local slot
  arithmetic, same as PokerCheat).

## Next concrete step

None of `src/BlackjackCheat.cpp`'s struct offsets have been checked
against a live game, even the ones now rated HIGH confidence from static
tracing alone. To start confirming them: launch RDR2 with a Debug build
deployed, sit at a blackjack table with a hand dealt, press F11 -> "Probe
Table Struct" (and "Probe Seat Hands"), then read `BlackjackCheat.log` and
compare against the real screen. Priority order (see `docs/JOURNAL.md`
Session 5, which supersedes Session 4's list, for the reasoning behind
each):
1. Do the logged dealer/seat cards match what's actually showing? Also
   check the new `currentHandIndex(f_3)` field looks sane (0 while a seat
   is still mid-turn, equal to handCount once done).
2. **Still the highest-value, zero-interaction check**: with the Debug
   build running, just play a few normal rounds and read
   `BlackjackCheat.log` afterward for "PredictionCheck" lines -- this now
   specifically validates a FROZEN round-start baseline (Session 5); the
   HUD's live prediction is a separate, continuously-updating thing --
   watch it on screen instead, it should visibly settle/stop changing
   once your own turn (and any other occupied seats after you) are done.
3. If another seat is occupied (an AI opponent), watch the HUD's
   "Predicted dealer draws" line update as that seat plays its turn --
   the most direct live test of Session 5's turn-order/determinism trace.
4. F11 -> "Probe Deck Prediction" right after a hand is dealt -- does the
   logged dealer hole card match what's actually under the face-down card
   once it flips over at round resolution?
5. Compare the two logged mySeat candidates against your real seat --
   both are now well-justified (ped-array PRIMARY and f_9 SECONDARY are
   both HIGH confidence as of Session 5); a live DISAGREE would be a
   genuinely surprising, high-priority thing to chase.
6. Do the `seat.f_1`/`seat.f_4[h]` bankroll/bet log lines look like
   plausible dollar amounts matching the real on-screen stack/bet?
7. Split a hand once, then try to split again -- should be refused
   (`kMaxHandsPerSeat=2` is a real enforced cap per Session 2).
8. Split a pair of Aces specifically and watch what happens -- Session
   3's most important claim to verify: does the game deal one card to
   each new hand and immediately move on with NO further hit/stand/double
   prompt (the f_699 trace)? If the game lets you act further, that trace
   needs re-deriving.
Expect at least one wrong candidate to need re-deriving regardless --
PokerCheat's own struct layout took multiple sessions of exactly this loop
before every constant was confirmed correct, and "static tracing agrees
with itself" is not the same bar as "matches a live memory read."
