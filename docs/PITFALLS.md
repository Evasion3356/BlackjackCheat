# Pitfalls, dead ends, and lessons learned

Read this before touching native calls or struct offsets in this project.
Seeded from lessons already paid for in the sibling `PokerCheat` and
`CollectorOffline` projects (same toolchain, same machine, same general
hazards) so they aren't relearned the hard way here too.

## Carried over from PokerCheat / CollectorOffline

- **MP-only content wall.** Rockstar's own compiled scripts and some game
  systems are gated to real multiplayer sessions in ways that aren't
  obvious from the outside, and the failure mode is usually a **crash**,
  not a clean error. `bjack_sp` is explicitly the single-player script
  (there's presumably a separate MP blackjack variant, not checked) --
  this project should be safe by construction, but hasn't needed to touch
  anything MP-adjacent so far anyway (no shared `act_gen_*` framework file
  was found for blackjack, unlike poker).
- **Trace decompiled source precisely, don't reconstruct from memory.**
  Every struct offset in `BlackjackCheat.cpp`'s file header comment cites
  an actual line number and function name from `bjack_sp.ysc.c` --
  keep that discipline for any future offset added. If a value's exact
  meaning matters, `Read`/`Grep` the actual `.ysc.c` file again before
  writing code or claiming a field's purpose.
- **Arrays don't consistently carry a leading header/size word.** PokerCheat
  hit this directly (`Table.f_15`/`f_39` DO have a leading count word,
  matching `RDR-Classes\script\types.hpp`'s `SCR_ARRAY::Size`, but the deck
  array `f_606` does NOT). The exact same split shows up here: the deck
  (`Table.f_592`) and every hand struct (`Table.f_2`, `seat.f_8[h]`) have
  NO leading header word (confirmed via their own build/deal functions
  writing straight to element 0) -- don't assume one convention applies
  file-wide just because it held for one array.
- **A field name like `f_N` in this decompiler's output IS the literal
  byte offset within its immediate parent struct** (in 8-byte-slot units,
  same `alignas(8)` convention `RDR-Classes\script\types.hpp` documents) --
  not an arbitrary label. This is how `BlackjackCheat.cpp`'s struct-size
  arithmetic (`Table.f_2`, a 25-word hand struct, ending exactly where
  `Table.f_27`'s seats array begins) was cross-checked as a sanity signal
  -- if two independently-traced offsets don't add up like that, suspect
  one of them before trusting both.
- **Don't write block comments containing decompiler-style `/*N*/` stride
  annotations verbatim.** `BlackjackCheat.cpp`'s own file header comment
  originally quoted the decompile's own `uParam0->f_27[iParam1 /*60*/]`
  syntax directly inside a `/* ... */` block comment -- the embedded `*/`
  closed the OUTER comment early, and every following line became raw
  (broken) source code. MSVC's errors for this look nothing like "unterminated
  comment" -- they show up as a cascade of unrelated syntax errors many
  lines below the real cause (stray backticks, "newline in string literal",
  etc.), so if a wall of syntax errors appears in a file that clearly has
  balanced braces, check for a `*/` sequence hiding inside a `/* */` block
  comment first. Fixed here by paraphrasing as "(stride N)" instead of
  copy-pasting the decompiler's own comment syntax.
- **Log before and after every native call whose success isn't visually
  obvious.** `Log::Write` (see `src/Log.h`) is already wired up -- use it
  liberally, same "read the log, not the screen" methodology as PokerCheat/
  CollectorOffline.
- **Don't hand-guess struct offsets for this exact game build.** Every
  offset in this project is a STATIC trace only so far (see CLAUDE.md) --
  treat every one as a hypothesis to confirm via the F10 `Probe*` menu
  items against a live game, not a fact, until `docs/JOURNAL.md` records
  that confirmation actually happening (the way PokerCheat's own JOURNAL.md
  does for its struct layout).

## Session 2

- **A "same value" simplification isn't always the same as "same
  underlying value" the game actually checks.** `BlackjackHandEval.h`'s
  split-pair detection originally compared `CardValue(rank0) ==
  CardValue(rank1)` (blackjack VALUE, 2-11) instead of the raw ranks
  (2-14) `bjack_sp`'s own split-legality gate (`func_1237` case 6)
  actually compares. It happened to produce the same final answer for
  every case the test suite covered, purely because the only
  value-but-not-rank collision (J/Q/K all -> value 10) also happens to be
  the one pair value the strategy chart always declines to split anyway
  -- so the bug was real but silently unobservable given the current
  chart. Lesson: when translating a game's own legality/comparison logic
  into a simplified domain (blackjack value instead of raw rank, hand
  category instead of raw cards, etc.), keep the comparison at the SAME
  level of abstraction the game itself uses unless there's a specific
  reason the simplification is provably equivalent -- don't assume a
  derived value is an adequate substitute for equality-testing just
  because it's more convenient, even if today's test cases can't tell the
  difference.
- **A message string that LOOKS like a special-case rule isn't
  necessarily one.** `MGBLK_MSG_ACE_CANT_DOUBLE`/`_SPLIT` and
  `MGBLK_MSG_SPLIT_ACES` initially looked like strong evidence of a
  common real-casino "split aces can't double / get one card only" rule.
  Tracing the actual call site (not just the string's existence) showed
  it's purely cosmetic wording layered on the SAME general funds/
  card-count legality check, selected via a separate "does this hand
  contain an Ace" flavor check -- no distinct rule exists in the code
  path that was traced. Don't infer a game-logic rule from a
  suggestively-named string/message constant alone; find and read the
  function that actually decides when it fires.

## Session 16

- **A per-language ini override doesn't make the game load that
  language's assets -- test glyph rendering with the game's REAL
  language set, not just this mod's override.** Same lesson
  `../PokerCheat/docs/PITFALLS.md` already documents from its own
  localization work (Session 20/21 there): `BlackjackCheat.ini`'s
  `Language` key only changes which of `Localization.cpp`'s translated
  strings THIS MOD draws; it can't make RDR2 itself stream in a
  different language's font/text assets. Any non-default-language
  rendering test needs the game's REAL language changed (Steam
  Properties -> Language) and the game relaunched first -- not
  re-verified independently in this project since it uses the
  identical `$Font5`/`UIDEBUG::_BG_DISPLAY_TEXT` pipeline PokerCheat
  already confirmed on this exact game build, but the underlying
  mechanism (and pitfall) is the same regardless of which mod is doing
  the drawing.
- **A project can silently be missing `/utf-8` even when a sibling
  project already needed it, for a completely different reason.**
  `BlackjackCheat.vcxproj` had no `/utf-8` compiler flag at all before
  this session, because `SPDLOG_USE_STD_FORMAT` (see `src/Log.h`)
  sidesteps the specific spdlog/fmt static_assert that's the ONLY
  reason `PokerCheat.vcxproj` already carries the flag -- so simply
  diffing "does the sibling project have this flag for the same
  reason" would have missed it. The real, project-independent
  requirement is: any `.cpp` file with non-ASCII string literals saved
  as UTF-8 needs `/utf-8` (or equivalent `/source-charset`/
  `/execution-charset` flags) on ITS OWN project, regardless of what
  any other build reason might already justify it for elsewhere.
  Without it, MSVC silently reinterprets the UTF-8 bytes against the
  current ANSI code page and mangles every non-Latin character instead
  of raising a hard error -- caught here only because the translation
  tables were checked by decoding the compiled source file's bytes and
  pattern-matching Unicode blocks, not by anything the compiler flagged
  on its own.

Add new entries above this line as real mistakes happen.
