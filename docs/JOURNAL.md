# Journal

Chronological record of what's been built and where each piece currently
stands. Read `../CLAUDE.md` first for orientation, `PITFALLS.md` for the
lessons behind these decisions.

## Session 1 -- scaffold + static struct trace, advisor HUD, no live confirmation yet

Built as a sibling of `../PokerCheat`, reusing its exact toolchain and
conventions (ScriptHookSDK-based ASI, F10 menu vendored from the same
source, `Log::Write` file logger, `vcxproj` build/deploy setup, inipp-based
`Config`) rather than inventing new ones. Vendored `external\RDR-Classes`
and `external\inipp` as local copies (PokerCheat's own copies live inside
its project folder, not shared at the `RDR2 Shit` root, so they couldn't
be referenced directly).

User's explicit scope choices for this first pass (via three clarifying
questions before any work started):
1. Project name **BlackjackCheat** (not "BjackCheat", even though the
   target script is `bjack_sp`).
2. **Advisor HUD only** -- read dealer/player hand data, show values +
   basic-strategy hit/stand/double/split advice. Explicitly NOT an
   auto-win/deck-manipulation cheat, matching PokerCheat's own eventual
   design philosophy (it deliberately settled on advisor-only after a
   dead-end chasing the game's own hand-rank native -- see PokerCheat's
   own JOURNAL.md).
3. **Scaffold + static trace** -- no running game/live memory available
   this session, so every struct offset below is a STATIC TRACE ONLY,
   explicitly labeled UNCONFIRMED throughout, unlike PokerCheat's
   (now-confirmed) equivalents.

### Hand-value/basic-strategy logic: pure math, fully tested, no game dependency

`src/BlackjackHandEval.h` -- hand value with correct soft/hard Ace
counting (demote 11->1 only as needed to avoid busting), bust/blackjack
detection, and the standard published basic-strategy chart (4-8 deck,
dealer stands soft 17, double-after-split allowed, no surrender modeled)
for hard totals, soft totals, and pair-splitting. This part does NOT
depend on any struct offset being right -- it's the same
separation-of-concerns choice PokerCheat's `PokerHandEval.h` made, and for
the same reason (a hand-scoring bug that's only ever eyeballed in-game can
go undetected for a long time -- see PokerCheat's JOURNAL.md, Session 9).

`tests/BlackjackHandEvalTests.cpp` covers hard/soft/bust/blackjack
detection and a representative sample of basic-strategy decisions (hard
16 vs. dealer 10 = Hit, hard 11 vs. dealer 6 = Double but vs. Ace = Hit,
soft 18's three-way split behavior vs. weak/medium/strong dealer cards,
A,A always splits, 5,5 never splits, K+10 mixed-rank "pair" never splits,
double/split gating when illegal). Built and run this session:

```
MSBuild.exe tests\BlackjackHandEvalTests.vcxproj /p:Configuration=Debug /p:Platform=x64
bin\Debug\BlackjackHandEvalTests.exe
```

Result: **ALL PASS** (27 checks).

### Struct trace: bjack_sp.ysc.c, static only

All line numbers refer to
`D:\Backup\Stuff\RDR2 Shit\Scripts\rdr2-scripts-decompiled\1491.50\script_rel\bjack_sp.ysc.c`.
Full derivation/confidence notes for every constant live in
`src/BlackjackCheat.cpp`'s file header comment -- not fully repeated here,
just the summary:

- `uScriptParam_0` (LaunchArgs) at absolute slot 3624 -- HIGH confidence,
  pure slot counting (last declared local is `uLocal_3623`), same method
  that gave PokerCheat's `kLaunchArgsSlot`.
- Table = `uLocal_14` itself, flatter than poker_sp's model (poker's Table
  needed an extra `.f_114` hop; blackjack's fields hang directly off
  `uLocal_14`). Traced via `func_85(&uLocal_14)` (line 3961) resetting
  fields (`f_9`, `f_17`, `f_1532`) that independently match direct
  `uLocal_14.f_N` uses found elsewhere in the file.
- `Table.f_756` = the real per-round game-state struct (`func_222`'s
  target, line 5048) -- `kTableSlot = 14 + 756 = 770`. `func_222` itself
  calls `MINIGAME::_0x6480723D3BE535B6(-1150372370)`, the SAME constant
  seen in poker_sp's `kKnownStakesHashes` -- an indirect but real
  cross-game confirmation this is the right kind of function.
- `Table.f_2` = dealer's hand struct. Traced via `func_1236` (the one deal
  call OUTSIDE the per-seat dealing loop) and corroborated by the dealer's
  own stand-on-17 loop (`while (uParam0->f_2.f_24 < 17)`, line ~26058) AND
  by struct-size arithmetic (a 25-word hand struct at offset 2 ends
  exactly at offset 27, where the seats array begins).
- `Table.f_27` = seats array, 4 seats, 60 words/seat. Occupancy marker at
  seat+0 (`!= -1`, confirmed via `func_116`, line 5789). Per seat:
  `f_8[handIndex]` = up to `kMaxHandsPerSeat` (assumed 2 -- main + one
  split, NOT confirmed as a real array-size cap) 25-word hand structs,
  `f_59` = how many are in use (confirmed via `func_1235`, line 40739).
- Hand struct (25 words, shared shape for dealer's `f_2` and every seat's
  `f_8[h]`): cards at +0..+20 (11 slots, 2 words each, **no header word**
  -- confirmed via `func_1072`, line 35512), count at +23 (`f_23`), total
  value at +24 (`f_24`, confirmed via the bust check `f_24 > 21` in
  `func_1069`, line 35494, and the dealer's own stand loop above). HIGH
  confidence on all three offsets -- three independent call sites agree.
- Deck = `Table.f_592`, no header word (confirmed via `func_458`'s build
  loop, line 18168), cursor at `+105`, count at `+106`. Shuffle
  (`func_933`, line 31274) is a byte-for-byte structural match of
  poker_sp's own 5-pass Fisher-Yates (`func_1195`). Not currently read by
  `OnTick()` at all (first-pass scope is advisor-only on already-dealt
  cards, no future-card prediction) -- only logged by `ProbeTableStruct()`
  for a future session.
- "Your seat" candidate: `uLocal_14.f_9` (slot 23), reset to -1 by
  `func_85` -- LOW-MEDIUM confidence, same reset-to-(-1) pattern as
  poker's equivalent field but with no second independent call site
  cross-checking it (unlike poker's, which had a predicate function
  testing the value a second way).

Card encoding (rank 2-14, suit 0-3, `{rank,suit}` 2-field struct) assumed
identical to poker_sp's own -- `func_458`'s deck-build loop has the exact
same nested-loop shape (suit outer 0-3, rank inner 2-14) as poker's
`func_589`. The specific suit->real-suit letter mapping (0=Hearts,
1=Diamonds, 2=Spades, 3=Clubs) is carried over from poker's CONFIRMED
finding on the assumption both minigames share the same card-texture
convention (bjack_sp's own `func_697`, line ~25174, does build a
`"card_set_N"` texture dictionary name, same as poker's) -- not
independently re-confirmed for blackjack specifically. Doesn't affect
hand scoring either way since `BlackjackHandEval.h` only reads rank.

Not traced this session: bet/bankroll fields (`seat.f_4`-shaped, seen once
in `func_1070` but not chased further), insurance (the game clearly has
one -- `"MGBLK_MSG_INSURANCE_STARTED"` and related strings found -- not
modeled at all), split-hand bet tracking. All explicitly out of scope for
the advisor-only first pass.

### Build status

Both configs build clean and deploy (RDR2.exe was not running during this
session, so the `PostBuildEvent` copy succeeded both times -- not
something to assume will always be true, see CLAUDE.md's build note).
**Not run against a live game at all this session** -- no game session was
available. The F10 menu's three `Probe*` items
(`ProbeTableStruct`/`DumpLocalStackRange`/`ProbeSeatHands`) exist
specifically so a future session can do exactly what PokerCheat's own
JOURNAL.md documents happening over many rounds: run the probe, compare
the log against the real screen, find the first offset that's wrong, and
re-derive from there. Expect at least one of the MEDIUM/LOW-confidence
candidates above (`kMaxHandsPerSeat`'s cap, `kMySeatSlot`, possibly the
seat stride/count) to need correcting -- none of PokerCheat's own struct
layout survived its first live probe run unchanged either.

One real bug caught and fixed before the first successful build: the file
header comment originally quoted the decompiler's own `/*60*/`-style
stride-annotation syntax verbatim inside a `/* ... */` C block comment --
the embedded `*/` closed the comment early, and MSVC reported it as a long
cascade of unrelated-looking syntax errors starting partway through the
comment's own text rather than anything resembling "unterminated comment".
Fixed by paraphrasing those as "(stride N)" instead of copying the
decompiler's literal comment syntax -- see `docs/PITFALLS.md`.

## Session 2 -- deeper static re-trace, still no live game available

Continuation of Session 1's static-only work (still no running game/live
memory this session either -- everything below is another round of
"read the decompile harder", not confirmation). Went looking specifically
for evidence that could upgrade the MEDIUM/LOW-MEDIUM offsets, and for the
actual traced game rules (double/split/surrender/dealer-soft-17) instead
of assumed textbook defaults. All line numbers refer to
`bjack_sp.ysc.c` unless noted.

### Confidence upgrades

- **`kSeatCount` (4): MEDIUM-HIGH -> HIGH.** `func_280` (line 12838:
  `if (iParam1 >= 0 && iParam1 < 4 && uParam0->f_27[iParam1].f_59 > 0)`)
  is a direct bounds check of the seat index against the literal `4` --
  the same kind of real capacity evidence poker's own array-size header
  words gave, not just an incidental loop count.
- **`kMaxHandsPerSeat` (2), `kSeatHandsOffset`/`kSeatHandCountOffset`:
  MEDIUM/LOWER -> HIGH.** `func_1237` (line 40757) is the actual
  hit/double/split/stand action-legality gate (cases 4/5/6/7). Case 6
  (split) returns `false` outright when `uParam1->f_59 > 1` (line 40785)
  -- the game itself enforces "at most one split, ever" as a real rule,
  not an inferred cap from limited observation.
- **`kMySeatSlot`: LOW-MEDIUM -> MEDIUM.** Two more plausible corroborating
  call sites found (line 4255-4256, a camera/ped-focus lookup chain; line
  20114, a per-seat stat gate) but neither is the kind of independent
  predicate-function proof poker's equivalent field had. Important
  caution recorded in `BlackjackCheat.cpp`: `f_9` is heavily overloaded
  across unrelated structs throughout this file (UI widget handles,
  animation task data, etc. all coincidentally also use field index 9)
  -- any future "f_9" hit must be checked for which struct it's really on
  before counting it as corroboration.
- **New fields found**: `seat.f_1` (bankroll) and `seat.f_4[handIndex]`
  (bet per hand) -- both read together in `func_1237`'s funds check (line
  ~40775: `if (seat.f_1 < seat.f_4[handIndex]) return false`). MEDIUM
  confidence, one call site. Not used by `OnTick()`/the HUD (still out of
  scope), but now logged by `ProbeSeatHands()` for a future live sanity
  check. Also found `seat.f_2` = insurance bet, paid out 2:1
  (`seat.f_1 += 2 * seat.f_2; seat.f_2 = 0`, line ~26052) when the
  dealer's up card is an Ace and the dealer has blackjack -- not modeled,
  noted for whenever insurance becomes in scope.

### Rule tracing -- corrected `BlackjackHandEval.h` against the real game logic

- **Card value + hand total: now a confirmed structural match, not an
  assumption.** `func_1010` (line 33066) is a case-by-case exact match of
  `CardValue()` (2-10 face, J/Q/K=10, A=11). `func_645` (line 23356) is a
  byte-for-byte structural match of `EvaluateHand()`'s own ace-demotion
  loop (`while (total > 21 && aces > 0) { total -= 10; aces--; }`). This
  means `hand.f_24` is definitely an ace-adjusted BEST total, not a raw
  sum.
- **Dealer stands on soft 17 -- now a solid derivation, not a guess.**
  Because `f_24` is confirmed ace-adjusted (see above), a soft 17 (e.g.
  A,6) computes `f_24 = 17`, not 27 -- so the dealer's own draw loop
  (`while (dealerHand.f_24 < 17) { hit }`, line 26058) necessarily stops
  there without needing any separate soft-ace branch. `BlackjackHandEval.h`
  already assumed this; no strategy-table change needed, just a much
  stronger citation.
- **Double: any 2 cards, no total restriction -- confirmed.** `func_1237`
  case 4 (line 40771) only checks card count (`<= 2`) and bankroll, no
  total-value gate. Matches the existing implementation; no fix needed.
- **Split requires exact RANK match, not just blackjack VALUE -- a real
  bug, now fixed.** `func_1237` case 6 (line 40791:
  `if (hand[0] != hand[1]) return false`) compares raw card ranks (2-14),
  not `CardValue()`'s 2-11 bucket. `GetBasicStrategyAction()` was
  comparing `CardValue(ranks[0]) == CardValue(ranks[1])`, which would
  have recommended splitting a King+Queen (both worth 10, but rank 13 vs
  12) -- illegal in this game. Fixed to compare raw ranks directly. In
  practice this changes NO test's output today: the only blackjack-value
  bucket with more than one rank in it is "10" (J/Q/K all map to 10), and
  the strategy chart already says "never split tens" unconditionally --
  so the bug was latent/unobservable given the current chart, not a live
  wrong-advice incident, but it was still wrong on its own terms and
  would have become a real bug the moment that chart entry ever changed.
  Added/clarified `tests/BlackjackHandEvalTests.cpp`'s existing "K,10
  never splits" case to record this explicitly.
- **No surrender -- now confirmed absent, not just unchecked.** An
  exhaustive case-insensitive search of `bjack_sp.ysc.c` for "surrender"
  returns zero hits (contrast with insurance/double/split, which all have
  multiple message-string hits). Matches the existing assumption.
- **No ace-specific hit/double restriction found on split hands.**
  Several message strings exist that mention Aces specifically
  (`MGBLK_MSG_ACE_CANT_DOUBLE`, `_SPLIT`, `MGBLK_MSG_SPLIT_ACES`,
  `MGBLK_MSG_ACE_11`) and initially looked like they might mean "split
  aces can't double" or "split aces get one card only" (both common
  real-casino rules). Traced their actual trigger (`func_596`, line
  ~20620) and found they're purely cosmetic wording variants of the SAME
  underlying funds/card-count legality checks (`func_997`=CanSplit,
  `func_998`=CanDouble), selected via `func_996` testing "does this hand
  contain a rank-14 card" purely for flavor text -- `func_1237`'s actual
  hit (case 5) and double (case 4) gates apply identically regardless of
  whether the hand contains an Ace or came from splitting a pair of
  Aces. Static-trace only, moderate confidence in the negative (didn't
  exhaustively rule out a check living somewhere else entirely) -- worth
  specifically watching for in live play, since it would be an unusual
  deviation from typical real-casino rules if true.
- **Not resolved**: blackjack payout ratio (3:2 vs 6:5) -- a `1.5f`
  constant near line 7246 looked promising but turned out to belong to
  an unrelated card-prop/caddy setup function (`func_207`), not payout
  math. Genuinely untraced still. Doesn't affect strategy advice either
  way.

### Build/test status

`BlackjackHandEvalTests.exe`: rebuilt after the split-fix, still **ALL
PASS (27/27)**. `BlackjackCheat.vcxproj` Debug and Release both rebuilt
clean, 0 warnings/errors, both deployed via `PostBuildEvent` (RDR2.exe
was not running this session either).

### Next concrete step (updated)

Same live-probing loop as before -- still nothing here has touched a
running game. Priority order for the first live session, given what's
now known:
1. F10 -> "Probe Table Struct" / "Probe Seat Hands" while seated with a
   hand dealt -- do the logged dealer/seat cards match the real screen?
2. Specifically check the NEW `seat.f_1`/`seat.f_4[h]` bankroll/bet log
   lines against the real on-screen stack/bet numbers -- these are only
   MEDIUM confidence and completely unverified.
3. If possible, split a hand once and try to split again -- `kMaxHandsPerSeat`
   being HIGH confidence now (a real code-enforced cap) means the game
   itself should refuse a second split; watch for whether the "Split"
   option even appears/is selectable at that point.
4. If possible, get a hand containing a split Ace to more than 2 cards or
   attempt to double it -- tests the "no ace-specific restriction found"
   claim directly. If the game actually blocks this, that's a real
   divergence from what `func_1237`'s static trace suggested and needs
   re-deriving.
5. `kMySeatSlot` is still only MEDIUM -- cross-check the logged candidate
   against which seat is actually yours on screen every time a probe
   runs, not just once.

## Session 3 -- resolved the ace-split question, a better mySeat method, single-deck correction, card counting added

Still no live game/memory this session either -- more static re-tracing,
plus new pure-math/logic features (card counting) that don't depend on any
struct offset being right. All line numbers refer to `bjack_sp.ysc.c`
unless noted.

### Split Aces DO get the "one card each, forced stand" restriction (reverses Session 2)

Session 2 concluded bjack_sp had no ace-specific hit/double restriction,
based only on `func_1237`'s legality gate (which is indeed ace-blind).
That missed the actual enforcement mechanism, found this session one
level up in the per-seat turn state machine: a field `f_699` ("how many
hands to force-resolve without further player input"). `func_1067` (the
action executor) sets `f_699=1` after Stand (case 7, line ~35474) and
Double (case 4, line ~35437) -- both end that hand's turn -- and leaves
it at 0 after an ordinary Hit (case 5) or a non-Ace Split (case 6, no
`f_699` write). But case 6 has one more line (~35469-35470):
`if (hand[num][0] == 14) f_699 = 2` -- specifically when the ORIGINAL
pair being split was Aces. The consuming loop (state machine's turn-
advance step, line ~26027:
`for (j = seat.f_3; func_1068(...) && f_699 > 0 || ...; j++) { resolve
hand j; seat.f_3++; f_699--; }`) force-resolves exactly `f_699` hands in
a row with no player input -- so `f_699=2` after an Ace split walks
through BOTH new hands automatically, each already holding the one card
`func_1067`'s split branch dealt it, never re-prompting hit/stand/double
on either. This is the classic real-casino "split Aces get one card
only" rule. Fixed in `BlackjackHandEval.h`: `GetBasicStrategyAction()`
takes a new `isSplitAceHand` parameter that forces `Stand` unconditionally
when set; `BlackjackCheat.cpp`'s `DrawOverlay()` detects it (a seat with
2 hands -- only reachable via exactly one split -- whose hand's first
card is an Ace; since split requires exact RANK equality, that can only
happen if the split pair was Aces). New tests in
`TestSplitAceHands()` cover both a case that would normally recommend
Hit and one that would normally recommend Double, confirming both are
overridden to Stand.

### A better "my seat" method than f_9 (kMySeatSlot stays MEDIUM, unresolved on its own terms)

Pushed on `kMySeatSlot` from a new angle (input/ped-ownership) rather than
re-searching for more `f_9` sites. Found `uLocal_14.f_1724` is a SIBLING
struct to Table (like poker_sp's own `f_3310` "scene" struct -- a
separate camera/ped/animation-task layer, not the core game-logic Table
at `f_756`): traced via `func_44`'s teardown code (line ~4267-4306,
passes `&(uLocal_14.f_1724)` to `func_117`/`func_118`/`func_121`/
`func_122`/`func_123`/`func_125`, all seat-indexed 0-3) and
`func_236`/`func_239` (lines ~9656-9789, a per-seat ped animation-task
state machine threading `&(uParam0->f_1724)` through the same helpers).
Its `f_946[seat]` (stride 46) is the seat's live Ped HANDLE -- `func_117`
(line 5797: `f_946[i] != 0`) and `func_118` (line 5806:
`return f_946[i];`) both use the array element directly as a scalar Ped
value, the same "array[i] alone = offset+0 of the stride" convention
already confirmed for `Table.f_27`'s own occupancy marker. HIGH confidence
on `kPedSceneFieldOffset=1724`/`kSeatPedArrayOffset=946`/
`kSeatPedStride=46` (multiple independent call sites, consistent seat
indexing, consistent scalar-access convention, and the game's OWN code
already feeds `f_946[i]` straight into `PED::` natives, e.g. line ~4280
`PED::IS_PED_A_PLAYER(ped)`). This gives a fundamentally more reliable
"my seat" mechanism: read each seat's ped handle and compare it against
`PLAYER::PLAYER_PED_ID()` via a real native call
(`FindMySeatByPed()`, now the PRIMARY method used by `DrawOverlay()` and
both `Probe*` functions) -- it only depends on confirming `f_946` holds a
real ped handle (which the game's own code already treats it as), not on
guessing what an opaque scalar means.

`kMySeatSlot` (f_9) itself is UNCHANGED at MEDIUM and kept only as a
SECONDARY logged candidate for comparison (`ProbeTableStruct`/
`ProbeSeatHands` now log both and flag AGREE/DISAGREE). Line 4276
(`func_116(&(uLocal_14.f_17), i)`) initially looked like it might
contradict the Table.f_27 derivation itself (func_116 reads `f_27`
relative to WHATEVER struct it's given, and this call passes
`&(uLocal_14.f_17)`, not `&uLocal_14`/Table directly) -- traced further
and concluded it's `func_116` being reused generically on a DIFFERENT,
unrelated `-1`-sentinel array living on the `f_17` struct (not Table),
since the overwhelming majority of `func_116` call sites (e.g. line
25996, inside the very turn state machine already relied on for the
f_699 trace above) pass `uParam0`/`uParam1` directly, threading Table
through unchanged. Also re-confirmed the existing f_9-overloading
caution: `uParam1->f_9` at lines ~9743/9769/9774/9781 turned out to be a
seat-index field on a THIRD, different per-seat animation-task struct,
not `uLocal_14.f_9` at all -- muddying rather than resolving the f_9
question, which is exactly why the ped-array method above was pursued
as an alternative instead.

### Single 52-card deck, reshuffled every round -- NOT a 4-8 deck shoe

`func_458` (deck build, line ~18168) is unambiguous:
`for (i=0;i<4;i++) for (j=2;j<15;j++) { build card rank=j suit=i }` --
4 suits x 13 ranks = 52, `f_106 = num2` always lands on exactly 52.
`func_718` case 0 (line ~25823-25827, the "settle previous hand, start
new one" step -- confirmed by the payout code immediately following it,
`seat.f_1 += winnings`) calls `func_459`, which rebuilds AND reshuffles
that same 52-card array via `func_458`+`func_933` -- at the start of
EVERY round, not just game startup. HIGH confidence (exact loop bounds,
real call site tying the rebuild to every round). This corrects
`BlackjackHandEval.h`'s and `BlackjackCheat.cpp`'s prior "4-8 deck shoe"
assumption -- updated both files' header comments. Real single-deck
basic strategy differs from multi-deck at a handful of borderline hands
(commonly cited: doubling 8 vs. 5/6, doubling soft 18/19 vs. 2, splitting
6,6 vs. 7, splitting 4,4/3,3/2,2 vs. a few more dealer upcards) -- the
strategy chart itself was deliberately NOT changed this session (no
verified single-deck-specific numeric chart was re-derived from a
reliable source under this session's time budget) -- this is an
explicitly flagged gap for a future session, not a silent carry-over.

### Card counting + insurance advice added (new feature, pure math + read-only)

New `src/BlackjackCardCounting.h` (Hi-Lo tags, running count, true count,
decks-remaining, two of the "Illustrious 18" deviations -- insurance at
true count >= 3, stand hard 16 vs. 10 at true count >= 0), unit-tested in
new `tests/BlackjackCardCountingTests.cpp`/`.vcxproj` (16 checks, all
pass). Explicitly scoped down from the full 18 deviations given the
single-deck-reshuffled-every-round finding above: within one ~2-10 card
round dealt from a fresh full deck, there's limited room for the count to
swing far from 0 before it resets, so most of the classic 18 would rarely
if ever trigger here -- implementing all of them wasn't worth the effort
this session.

Wired into `BlackjackCheat.cpp` via `UpdateCardCounting()`, called every
tick from `DrawOverlay()` regardless of HUD toggles (so the count stays
correct even if the panel is hidden). Only feeds cards that are actually
VISIBLE to the player, never the undealt remainder of the deck:
- Every player hand's cards are counted as soon as they appear (always
  genuinely visible), tracked via a per-seat/per-hand "cards counted so
  far" high-water mark so re-reading an unchanged hand never
  double-counts.
- The dealer's hand struct holds both the up card AND the face-down hole
  card from the moment it's dealt, but the hole card isn't actually shown
  on screen until the round resolves -- so only `ranks[0]` (the up card)
  is counted as soon as the dealer has any cards; the rest (hole card +
  any dealer hits) is deferred and only counted once the round fully ends
  (dealer hand count drops back to 0, detected by comparing against the
  last-seen snapshot) -- by definition already fully revealed by then.

HUD: running/true count shown as a Debug-only text panel line (new
`cfg.ShowCardCount` toggle); a Release+Debug insurance Yes/No
recommendation (new `DrawInsuranceStatus()`, same `$Font5`/UIDEBUG
pipeline and Release-visibility convention as the existing advice
readout) shown only when the dealer's up card is an Ace (new
`cfg.ShowInsuranceAdvice` toggle). Both new Config fields added to
`Config.h`/`.cpp` following the existing INI-round-trip pattern.

### Build/test status

`BlackjackHandEvalTests.exe`: 31/31 pass (27 previous + 4 new
`TestSplitAceHands` cases). `BlackjackCardCountingTests.exe` (new): 16/16
pass. `BlackjackCheat.vcxproj` Debug and Release both rebuilt clean, 0
warnings/errors, both deployed via `PostBuildEvent` (RDR2.exe was not
running this session).

### Next live session priority (updated, supersedes Session 2's list)

1. F10 -> "Probe Table Struct" / "Probe Seat Hands" while seated with a
   hand dealt -- do the logged dealer/seat cards match the real screen?
2. Compare the two logged mySeat candidates against your real seat: does
   the new ped-array method (PRIMARY) agree with f_9 (SECONDARY)? If they
   disagree, trust the ped-array one unless the real seat matches neither.
3. Check the `seat.f_1`/`seat.f_4[h]` bankroll/bet log lines against the
   real on-screen numbers (still unverified from Session 2).
4. Split a hand once, then try to split again -- should be refused
   (`kMaxHandsPerSeat=2` is a real enforced cap per Session 2).
5. Split a pair of Aces specifically and watch what actually happens --
   this session's most important claim to verify: does the game deal one
   card to each new hand and immediately move on with NO further
   hit/stand/double prompt, matching the f_699 trace above? If the game
   actually lets you act further on a split-Ace hand, the f_699 trace
   needs re-deriving.
6. With the Debug HUD up, watch the running/true count across a few
   rounds -- does it reset to 0 visibly between hands (confirming the
   single-deck-reshuffled-every-round finding), and do the counted
   values look plausible against cards you can see on screen (don't
   trust the dealer's hole card being counted immediately -- it should
   only show up in the running count after the round ends, per the
   deferred-counting design above)?
7. When the dealer shows an Ace, check the new Insurance Yes/No line
   appears and reads "No" under normal (low true count) conditions --
   the "Yes" case will be rare to trigger live given the single-deck
   finding, but worth a look if the count happens to run hot mid-round.

## Session 4 -- deterministic deck-ahead prediction (user course-correction), still no live game

The user pushed back on Session 3's card-counting feature: since this mod
already traces the deck/shoe struct offsets (`kDeckSlot`/cursor/count),
it should read the deck directly for deterministic predicted future cards,
the same way PokerCheat's `BuildPredictedBoard()` reads poker_sp's deck --
counting is strictly worse information once direct reads are available.
This is reinforced by Session 3's own finding: bjack_sp deals the ENTIRE
round from one fixed, already-shuffled 52-card deck, so the whole round's
remaining cards are knowable in advance from the cursor, not just a
handful of community cards like poker.

### What was built

- **`SimulateDealerOutcome()`** (new, `BlackjackCheat.cpp`) -- the
  blackjack equivalent of `BuildPredictedBoard()`. Two things fall out of
  the single-fixed-deck property:
  1. The dealer's hole card is REAL, already-dealt data the instant the
     round starts (`dealerHand.ranks[1]`) -- not a prediction at all, same
     category as PokerCheat showing opponents' real hole cards. Read
     directly, no deck-cursor involvement.
  2. The dealer's own forced stand-on-17 draw-out (see
     `BlackjackHandEval.h`'s header comment for the `f_24`-based
     derivation of that rule) is simulated by reading the next undrawn
     deck cards in cursor order and re-running `EvaluateHand()`'s own
     total/bust logic until total >= 17, exactly mirroring bjack_sp's own
     `while (dealerHand.f_24 < 17) { hit }` loop.
- **New caveat this needed that PokerCheat's board prediction didn't**:
  bjack_sp's deck cursor is shared by every seat's hits AND the dealer's
  draws, advancing in whatever the real turn order actually is (untraced).
  The simulation is only exact if nothing else has drawn from the deck
  between the snapshot and the dealer's real turn -- called right when the
  round starts (before any hits), it's a "what if no one else draws"
  prediction that will legitimately drift once any seat actually hits.
  Documented plainly in the code rather than glossed over.
- **Self-validating "PredictionCheck" log line** (`UpdateDeckPrediction()`,
  Debug-only) -- captures the prediction once per round (on the
  false->true dealer-cards transition) and compares it against the
  dealer's real final hand once the round ends (true->false transition,
  same detection UpdateCardCounting already used), logging MATCH/MISMATCH
  automatically every round with zero F10 interaction needed -- the same
  technique PokerCheat's own "PredictionCheck ... MATCH" line used to
  validate poker_sp's board prediction. This is the single most useful
  piece of automatic live evidence a future session can gather.
- **`DrawDealerHoleCardStatus()`** -- new Release+Debug status line (same
  `UIDEBUG::_BG_DISPLAY_TEXT`/`$Font5` pipeline as the existing advice/
  insurance readouts) showing the dealer's real hole card. Required moving
  `RankName()`/`SuitLetter()` out of the `#ifdef _DEBUG` guard they were
  previously behind (they're small, pure formatting helpers -- no
  behavioral change, just availability).
- **`ProbeDeckPrediction()`** -- new F10 diagnostic (Debug-only) logging
  the dealer's real hole card, the simulated draw-out with its predicted
  final total, and the next 6 raw undrawn deck cards, specifically so a
  live session can eyeball the hole card against the real screen once it
  flips over and watch the deck-ahead cards land as actual draws happen.
- **HUD reordering**: the Debug text panel now shows dealer hand ->
  predicted dealer draws (new, primary) -> card count (existing, now
  explicitly labeled "(secondary)") -> player hands, matching the new
  priority. `Config.h` gained `ShowDeckPrediction` (Release+Debug default
  true).
- `BlackjackCardCounting.h`'s header comment updated to state plainly that
  it's superseded/secondary for this specific game now that direct deck
  reads are available -- kept because it's still correct and costs nothing
  to leave in, not because it's the recommended approach anymore.

### What's still NOT done

Predicting a SPECIFIC seat's next hit card was deliberately left alone --
turn order across the 4 seats sharing one deck cursor was never traced, so
there's no way to know how many cursor slots advance before any particular
seat's next hit. Only the dealer's draw-out (which happens last, after
every seat has finished) is simulated. `ProbeDeckPrediction()` still logs
the next few raw undrawn cards for manual inspection regardless of whose
turn is next -- that's the honest limit of what's known without tracing
the turn-order state machine.

### Build/test status

`BlackjackCheat.vcxproj` Debug and Release both rebuilt clean, 0
warnings/errors, both deployed via `PostBuildEvent` (RDR2.exe was not
running this session). `BlackjackHandEvalTests.exe` 31/31 and
`BlackjackCardCountingTests.exe` 16/16 both still pass unchanged (neither
header was touched by this session's changes).

### Next live session priority (updated, supersedes Session 3's list)

1. F10 -> "Probe Table Struct" / "Probe Seat Hands" while seated with a
   hand dealt -- do the logged dealer/seat cards match the real screen?
2. **New, highest-value check**: with the Debug build running, just play
   normally for a few rounds and read `BlackjackCheat.log` afterward for
   "PredictionCheck" lines -- do they say MATCH or MISMATCH? A MATCH
   confirms `kDeckSlot`/cursor/count AND the dealer-draws-uninterrupted
   assumption all at once. A MISMATCH is still informative (expected if
   another seat hit before the dealer's turn) but worth checking the
   mismatch is actually explainable that way, not a wrong offset.
3. F10 -> "Probe Deck Prediction" right after a hand is dealt -- does the
   logged hole card match what's actually under the dealer's face-down
   card once it flips over at round resolution?
4. Compare the two logged mySeat candidates (ped-array PRIMARY vs. f_9
   SECONDARY) against your real seat.
5. Check the `seat.f_1`/`seat.f_4[h]` bankroll/bet log lines against real
   on-screen numbers (still unverified since Session 2).
6. Split a hand once, then try again (should refuse); split a pair of
   Aces specifically and confirm the forced-stand-after-one-card behavior
   (Session 3's most important unverified claim).

## Session 5 -- turn order traced, NPC seats confirmed real+deterministic, mySeat upgraded, prediction made self-correcting

Still no live game this session -- pure static re-tracing plus a code
fix to `SimulateDealerOutcome()`/`UpdateDeckPrediction()` that follows
directly from what was found. All line numbers refer to `bjack_sp.ysc.c`.

### Turn order: confirmed, strictly ascending, dealer strictly last

`func_718` (line 25809) is the Table's own round-phase state machine.
Case 4 (line 25947) scans `for (i=0;i<4;i++)` and picks the first seat
where `func_1063` (line 35344: `func_280(uParam0,i) &&
uParam0->f_27[i].f_3 < uParam0->f_27[i].f_59`) is true -- occupied AND
still has unresolved hands. That seat then plays out entirely (cases
5/6/7, including the `f_699` forced multi-hand walk from Session 3)
before case 4 runs again for the next such seat. Once none qualify
(`num2 == -1`), case 4 goes straight to case 8, the dealer's own
`while (f_2.f_24 < 17) hit` draw-out already used by
`SimulateDealerOutcome()`. So: seat 0, then 1, 2, 3 (skipping
unoccupied/finished ones), then the dealer, strictly in that order,
sharing one deck cursor throughout. HIGH confidence -- the scan order and
termination condition are both unambiguous, no interpretation needed.

### NPC seats are real AND fully deterministic -- no randomness in the decision itself

Traced the full production/consumption chain for a seat's queued action:
`func_1065`/`func_1066` (line 35356/35374, the consumer func_718 calls)
branch on `iParam1 == uParam0->f_582` -- a real player-seat action QUEUE
(`f_570`/`f_579`) vs. a single-slot mailbox (`f_561[seat]`) for every
other seat. The producer, `func_1077` (line 35593), is reached from a UI
state machine (case 19 around line 8744) that branches on `func_597`
(line 20684: `return uParam0->f_9 == uParam1;`) -- the real player seat
waits for actual pad input (`func_600`); any OTHER occupied seat calls
`func_602` -> `func_1002` (line 32949), a PURE function of the hand's
total (`f_24`) and the dealer's up-card rank (`Table.f_2[0]`, no suit/
soft distinction at all) that looks up an action via `func_623` (line
21123, a ~1860-line fully-unrolled table keyed on dealer-upcard-rank x
hand-total/pair-total), with two hardcoded overrides: a pair of Aces
always Splits regardless of the table, and a table result of Double gets
demoted to Hit when the hand already has 3+ cards, funds are short, or
the seat has already split once (an AI *policy* choice -- `func_1237`
itself would legally allow doubling after split, per Session 2). Spot-
checked several split-table entries (pair totals 4/6/8/10 vs. dealer
2/3/4) against known-correct split/no-split conventions -- all matched.
The per-seat 1-6s random "thinking delay" timer only gates WHEN an NPC's
decision gets submitted, never WHAT it decides. HIGH confidence on the
mechanism; `func_623`'s full table was spot-checked, not exhaustively
transcribed (not needed -- see the fix below).

### Fix: deck-ahead prediction is now self-correcting instead of a one-shot guess

Given the above, `SimulateDealerOutcome()` doesn't need to replicate
`func_623` at all. `UpdateDeckPrediction()` now re-simulates from the
LIVE deck cursor every tick instead of snapshotting once at round start.
Since the cursor only ever advances as real draws happen, the last
simulation computed right before the dealer's real turn begins is
automatically exact -- by then every occupied seat ahead of the dealer
has already consumed its share of the cursor for real. This became the
HUD's live prediction (`g_predictedDealerOutcome`). Validating that this
mechanism actually works needed a SEPARATE frozen snapshot
(`g_predictionBaseline`, captured once at the old round-start trigger)
-- re-simulating all the way to round end would otherwise just compare
the dealer's real final hand against itself and "match" trivially,
proving nothing. The round-end `PredictionCheck` log line now validates
that frozen baseline specifically. (This fixes a real bug introduced and
caught within this same session, before ever building: the first draft
just made `g_predictedDealerOutcome` live and left `PredictionCheck`
comparing it to itself.)

### kMySeatSlot (f_9): MEDIUM -> HIGH

`func_597` (quoted above, line 20684) is exactly the independent,
second-derivation smoking gun PokerCheat's own confirmed mySeat field
had and this project's `f_9` candidate was missing until now -- the game
itself uses `f_9 == seat` to decide the single highest-stakes thing
there is: whether to wait for real button input or auto-play a seat.
Still a STATIC trace, not a live read -- kept at HIGH (static-trace), not
promoted to "confirmed". `FindMySeatByPed()` (Session 3) remains PRIMARY
in code since it doesn't depend on f_9 at all, but both candidates now
have real justification instead of one solid and one merely-plausible.

### New field: seat.f_3 (current hand index)

`func_1063` (quoted above) compares it directly against `f_59`. HIGH
confidence purely via the `f_N=offset+N` convention already load-bearing
throughout this struct. Not consumed by `OnTick()` (the self-correcting
fix above doesn't need it) -- logged by `ProbeSeatHands()` as a new
"still acting this round" sanity field.

### New rule found, NOT incorporated: 7-card Charlie auto-win

Case 7's per-hand resolution loop (line 26027) treats 7+ cards
(`f_23 >= 7`) the same as an already-made 21 -- both trigger a real win
payout instead of continuing to prompt hit/stand/double. The dealer's
own draw-out has no equivalent cap, so `SimulateDealerOutcome()` is
unaffected. Doesn't change `GetBasicStrategyAction()`'s thresholds either,
but is a real edge case standard strategy charts don't cover (hitting a
6-card hand specifically to try to lock in the auto-win can occasionally
be correct even when standing would otherwise be chart-optimal) --
flagged as a known gap, not implemented (would need real game-theoretic
work).

### Build/test status

`BlackjackCheat.vcxproj` Debug and Release both rebuilt clean, 0
warnings/errors, both deployed via `PostBuildEvent` (RDR2.exe was not
running this session). `BlackjackHandEvalTests.exe`/
`BlackjackCardCountingTests.exe` untouched this session (no header
changed) -- still 31/31 and 16/16 from Session 3/4.

### Next live session priority (updated, supersedes Session 4's list)

1. F10 -> "Probe Table Struct" / "Probe Seat Hands" while seated with a
   hand dealt -- do the logged dealer/seat cards match the real screen?
   Also check the new `currentHandIndex(f_3)` field looks sane (0 while a
   seat is still mid-turn, equal to handCount once it's done).
2. **Still the highest-value, zero-interaction check**: play a few normal
   rounds with the Debug build running, then read `BlackjackCheat.log`
   for "PredictionCheck" lines -- MATCH now specifically validates the
   OLD round-start-only baseline (deliberately frozen, see above); the
   HUD's live prediction is a separate thing that isn't logged
   automatically -- eyeball it on screen as the round plays out instead,
   it should visibly settle down/stop changing once your own turn (and
   any other occupied seats after you) are done.
3. Compare the two logged mySeat candidates (ped-array vs. f_9) -- both
   are now well-justified; a live DISAGREE would be a genuinely
   surprising, high-priority thing to chase.
4. Check `seat.f_1`/`seat.f_4[h]` bankroll/bet against real numbers
   (unverified since Session 2).
5. Split a hand once, then try again (should refuse); split a pair of
   Aces specifically (Session 3's forced-stand claim).
6. If another seat is occupied (AI opponent), watch the HUD's "Predicted
   dealer draws" line update as that seat plays its turn -- this is the
   most direct live test of the Session 5 turn-order/determinism finding.

## Session 6 -- FIRST LIVE MEMORY CONFIRMATION: kTableFieldOffset/kSeatHandsOffset/kHandCountOffset/kHandValueOffset corrected

The user finally had a live game session running. Instead of relying on
one `Probe*` function at a time (which assumes the offset theory being
tested is basically right and just needs a yes/no check), built a new
generic tool first: `GamePointers::DumpLocalStackJsonl()` (declared in
`GamePointers.h`, two overloads -- full-stack and an explicit
`[startSlot, startSlot+count)` range), wired to a new F10 "Dump Full
Stack JSONL" item (`BlackjackCheat::DumpFullStackJsonl()`). It dumps
EVERY script-local slot of bjack_sp's thread to a JSONL file -- one
object per slot, every plausible raw interpretation (`i32`/`u32`/`i64`/
`f32`/`hex`), no assumption about what any slot means. Filenames are
timestamped (`BlackjackCheat_stackdump_YYYYMMDD_HHMMSS.jsonl`) after the
user pointed out a fixed filename would clobber the previous dump before
a before/after diff could be taken.

### Why this was necessary: the original offsets were confirmed wrong

The very first live check (`ProbeDeckPrediction`/the on-screen HUD) showed
the dealer's hole card reading as garbage (`"Dealer hole: 27"` one run,
`"??"` another) while the up card and hand count both looked right --
i.e. SOME of `kTableSlot`'s downstream arithmetic was correct (close
enough to land on real hand-count data) but not all of it. One-probe-at-
a-time re-guessing wasn't converging fast enough against a live,
time-limited session, hence the stack-dump tool.

### The fix: a real before/after diff across a known state change

Took a dump, had the user hit at the blackjack table (a real, user-
identified Queen of Diamonds got drawn to the dealer), took a second
dump. Grepped BOTH dumps for the dealer's known cards (`9H, 4S` before
the hit) and found them at absolute slot 773 in both -- correctly
extended to `9H, 4S, QD` in the after-dump. Critically, several OTHER
slots also matched `9H,4S` in a single static snapshot (scattered
scratch/argument copies, most likely from a card-texture-drawing routine
called repeatedly with whatever card is currently being rendered) but
did NOT update between the two dumps the way the real, persistent struct
must -- a single dump alone would have been ambiguous; the diff wasn't.

`773 = 14 (uLocal_14) + kDealerHandOffset(2) + 757` -- so
`kTableFieldOffset` is **757, not 756**: a plain off-by-one in the
original Session 1 static trace. `kTableSlot` is now **771, not 770**.

### Independent cross-check: all 4 seats at once

The user's real 4-card hand (`2S,4H,4C,AD`, hard-computed value 21) was
live at the same moment. Using the corrected `kTableSlot=771` plus the
already-assumed `kSeatsBase(27)`/`kSeatStride(60)`, all 4 seats' hands
were pulled from the after-dump simultaneously. Every one produced a
card list whose sum matched its own logged "value" field EXACTLY
(19=8+3+8, 18=2+9+7, 21=A+Q, 21=2+4+4+A) -- strong corroboration for
`kTableSlot=771` from a completely independent direction (seats, not the
dealer), not a single lucky offset match.

Getting the seats to line up needed two more corrections, found by
direct trial against this same live data:
- **`kSeatHandsOffset`: 8 -> 10.** A seat's hand array actually starts 2
  words later than Session 2 traced. The dealer's own hand (living
  directly on `Table.f_2`, not inside a seat struct) did NOT need this
  correction -- its cards were already right at `handBase+0` -- so this
  is specific to the per-seat path, not a universal "+2 gap" rule.
- **`kHandCountOffset`: 23 -> 22, `kHandValueOffset`: 24 -> 23.** The
  11-card array occupies relative offsets 0-21 (22 words), so count/
  value naturally follow at 22/23, not 23/24 -- the original trace
  (Session 1/2) had counted one slot too many somewhere across the
  11*2=22-word card region.

All three corrections were required TOGETHER and validated against 4
independent real seats simultaneously in the same dump -- not a single
lucky match on one field.

### Side effect: `kSeatHandCountOffset` (`seat.f_59`) demoted HIGH -> MEDIUM

Session 2's confidence for offset 8 partly rested on "8 + 25*2 = 58, one
spare word, then `f_59` at 59 -- a tight structural fit". With hands
now confirmed to start at 10 instead, that same arithmetic no longer
lands cleanly: `10 + 25 + 24 = 59` puts slot 59 inside hand[1]'s own
trailing spare word, not past both hands. `seat.f_59` itself was NOT
re-verified this session (not needed by anything `OnTick()` currently
reads) -- demoted pending a live check, not silently left at its old
confidence.

### STILL OPEN: dealer count/value fields read 0/0 even with corrected offsets

Applying the same corrected `kHandCountOffset(22)`/`kHandValueOffset(23)`
to the dealer's own hand (base 773) read 0/0 in this one live sample,
despite the dealer's CARDS at that same base being confirmed correct
twice (before and after the hit). Possible explanations, none confirmed:
this particular copy of the dealer hand is a stale/secondary copy for
just those two trailing fields (odd, since a single struct copy
shouldn't split like that); a timing/read-race artifact; or a genuine
dealer-specific struct quirk not yet understood. Doesn't block anything
`OnTick()` currently reads (hand totals are always recomputed from raw
cards via `BlackjackHandEval.h`, never trusted from these fields
directly -- a policy already in place before this session, which is
exactly why this anomaly didn't block the HUD from working correctly
once the card-offset fixes landed) but is a real open question for the
next live session.

### Methodology note

The biggest practical lesson: a SINGLE stack dump is not enough to trust
a match on its own. The stack is full of transient scratch copies of
real-looking values (most likely function arguments to something like a
card-texture-drawing routine, called once per visible card every frame)
that coincidentally look exactly like real hand data from one snapshot
alone -- several were found this session that matched the dealer's cards
just as well as slot 773 did, right up until a second dump showed they'd
gone stale. A before/after diff across a KNOWN, real state change (here:
a dealer hit) is what actually separates the one persistent struct from
its many short-lived look-alikes. Future sessions chasing a
still-garbage field should reach for `DumpFullStackJsonl()` + a
before/after diff before re-guessing candidate offsets one at a time.

### Build/test status

`BlackjackCheat.vcxproj` Debug rebuilt clean and redeployed twice this
session (once for the new stack-dump tool, once for the offset
corrections), 0 warnings/errors both times. `BlackjackHandEvalTests.exe`/
`BlackjackCardCountingTests.exe` untouched this session (no header
changed) -- still 31/31 and 16/16 from Session 3/4. Release not rebuilt
this session (all testing was against the Debug build's F10 menu).

### Next live session priority (updated, supersedes Session 5's list)

1. **Highest priority**: with the corrected offsets now deployed, watch
   the live HUD (dealer hand, seat hands, hand values, advice) across a
   few full rounds -- do hand values/counts now consistently look right
   for EVERY hand, not just the ones already spot-checked this session?
2. Specifically watch the dealer's count/value fields (or anything
   derived from them) for the still-open 0/0 anomaly above -- does it
   happen every round, or was this session's sample unusual (e.g. mid-
   hit read timing)?
3. `seat.f_59` (hand count) needs a fresh live check now that its old
   structural corroboration no longer holds -- watch whether a split
   seat's `handCount` value (logged by `ProbeSeatHands()`) still reads 2
   correctly after a real split.
4. Compare the two logged mySeat candidates (ped-array vs. f_9) against
   the real seat -- not specifically re-tested this session.
5. Split a hand once, then try again (should refuse); split a pair of
   Aces specifically (Session 3's forced-stand claim) -- neither was
   re-tested this session, though `kMaxHandsPerSeat`'s own offset chain
   changed (`kSeatHandsOffset` 8->10) so it's worth re-confirming rather
   than assuming Session 2/3's live-untested claims still hold exactly.
6. **New, cheap habit for future sessions**: if any field ever looks
   wrong again, reach for F10 -> "Dump Full Stack JSONL" before/after a
   known real state change and diff, rather than re-guessing one offset
   at a time -- this session's actual working method, now proven.
