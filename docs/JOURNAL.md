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

## Session 9 -- deck-before-bet timing traced, pre-bet deal prediction built

**Note for future sessions**: this journal skipped straight from Session
6 to 9 -- Sessions 7 and 8's work (card-counting removal, the deck-derived
"pure cheat" engine, the dealer up/hole-card order fix, the deck cursor/
count offset fix, and the card-face-icon HUD redesign) all happened but
were only ever written up in `BlackjackCheat.cpp`'s own file header
comment, never backfilled here. Read that comment's "Session 7"/"Session
8" addenda for the real history; not reproduced in this file to avoid
transcribing it out of sync with the authoritative copy. This entry uses
"Session 9" to match the `.cpp` file's own numbering, not this journal's.

### The question: does betting happen before or after the deck is set?

The user asked directly. Traced `func_718` (the table's own round-phase
state machine, line 25809 of the decompile): case 0 -- entered the
instant the PREVIOUS round's dealer draw-out finishes (case 8/9 both end
by calling `func_1047(uParam0, 0)`) -- calls `func_459` (the same
rebuild+reshuffle Session 3 already found ties to every round) on its
very FIRST tick, unconditionally, before the state machine even starts
waiting for the next round's bets. That wait is `func_1055`/`func_1056`
(line 35235/35248): `func_1056` specifically requires EVERY occupied
seat's `seat.f_7` to be nonzero before state 0 can hand off to state 1,
and state 1 is what actually calls `func_1057`, the real initial-deal
function. So the full sequence per round is:

    previous round resolves -> deck rebuilt+reshuffled -> table waits
    for every seat's bet to be confirmed -> func_1057 deals from the
    ALREADY-fixed deck

The deck is fully determined **before** the bet, not after -- betting has
zero causal influence on the shuffle. HIGH confidence: this reuses the
exact same `func_459`/`func_718` case 0 call site the Session 3 deck
finding already cited (that session read what it does; this session read
when it runs relative to everything else).

### New field: `seat.f_7` (kSeatBetConfirmedOffset)

Found while tracing the above. `func_759` (line 27401) is a one-line
getter that reads exactly `seat.f_7`. That same field is what BOTH
`func_1056` (the state-0-exit gate) and `func_1057` (the real per-seat
deal gate, alongside `seat.f_4[0]`/`func_492` for the bet amount) check
before treating a seat as "playing this round". HIGH confidence -- a real
field independently load-bearing in two different genuine game-logic
gates, not an inferred offset guessed from a single site.

### New feature: pre-bet deal prediction (`SimulatePreDeal()`)

Since the deck is fixed before the bet, and `func_1057`'s own dealing
algorithm is now fully traced (seat 0->1->2->3 in order, 2 cards each to
any seat with both `f_7` and `f_4[0]>0`, then 2 final cards to the
dealer), the whole deal can be predicted straight off the raw,
not-yet-touched deck array -- the same "read it directly instead of
guessing" philosophy as the existing dealer-hole-card and dealer-draw-out
prediction (Session 4), just moved one phase earlier.

`SimulatePreDeal()` (`BlackjackCheat.cpp`, right after
`FindMySeatByPed()`) only produces a result when the deck is caught in
its untouched post-reshuffle state (`cursor==0`, `count==52`) -- outside
that window there's nothing safe to predict from (either a round is
already mid-deal, which the existing `dealerHand.count>0` codepaths
already cover, or the state is otherwise not what this expects). Wired
into `DrawOverlay()` as:
  - Debug panel text: "Predicted dealer (before deal)", "Predicted your
    hand (before deal)", "Next after deal".
  - Release+Debug card-face icons via the new `DrawPredictedHandIcons()`
    helper -- the dealer's predicted 2 cards reuse the exact same
    top-right slot `DrawDealerHoleCardIcon()` already occupies (growing
    LEFT instead of showing just one icon, since pre-deal NEITHER dealer
    card is visible on the real table yet, unlike post-deal where only
    the hole card needs hiding), and the player's own predicted 2 cards
    reuse the exact slot the post-deal "Next cards" row already occupies
    (growing RIGHT). No new config keys added -- the two states are
    mutually exclusive in time, so the existing `HoleCardIconX/Y` and
    `NextCardIconBaseX/Y/SpacingX/Width/Height` tunables were reused
    directly rather than adding a parallel, untested set.

Self-correcting the same way `SimulateDealerOutcome()` already is: early
in the "waiting for bets" phase a seat's `f_7`/`f_4[0]` can still change
while that seat is mid-adjustment on its own bet slider, so this is only
a "if dealing happened right now" guess until then -- but `func_1056`
already REQUIRES every occupied seat to be confirmed before state 0 can
exit, so the LAST call made right before that transition is exact by
construction, not a guess, at the exact moment it matters (immediately
before `func_1057` runs the real deal).

### Build/test status

`BlackjackCheat.vcxproj` Debug AND Release both rebuilt clean, 0
warnings/errors, both deployed via `PostBuildEvent` (RDR2.exe was not
running). `BlackjackHandEvalTests.exe` re-run unchanged: still 27/27 (no
header touched this session).

### NOT yet live-confirmed

Everything in this entry is a fresh static trace + a brand-new feature,
same starting position every other feature in this file began at:

- `seat.f_7` has never been read by this codebase before today and needs
  its own live check -- does a seat's HUD-predicted hand actually match
  what gets dealt once `dealerHand.count`/cursor flip to the post-deal
  state?
- The two new icon positions are an unverified guess that the same slot
  works for 2 stacked icons as well as it did for 1 -- may well need the
  same kind of live Reload-Config retuning `HoleCardIconX` itself needed
  (0.821 -> 0.957) once actually seen on screen.

### Next live session priority

1. Sit down, watch the HUD BEFORE placing a bet: does "Predicted your
   hand (before deal)" / the icon row match the 2 cards you're actually
   dealt once betting closes?
2. Same question for "Predicted dealer (before deal)" against the
   dealer's real hole+up cards once dealt.
3. Do the two new icon rows actually land somewhere sane on screen, or
   do they need their own `HUD` config keys and a live-tuning pass the
   way `HoleCardIconX`/`NextCardIconBaseX` did?
4. If another seat is occupied, watch whether that seat's bet
   confirming/un-confirming (adjusting their bet) visibly flips
   `seatWillPlay` for them before settling -- the clearest live test of
   the self-correcting claim above.
5. Backfill Sessions 7 and 8 into this journal from `BlackjackCheat.cpp`'s
   own file header comment (see the note at the top of this entry) --
   not attempted this session, flagged as a known gap rather than
   silently left inconsistent.

### Live confirmation addendum (same session, immediately after)

The user paused right at the bet prompt -- before placing a bet, before
any cards were dealt -- and ran F10 -> "Dump Full Stack JSONL"
(`BlackjackCheat_stackdump_20260911_165143.jsonl`). Then placed a $2 bet,
let the round deal, and dumped again
(`BlackjackCheat_stackdump_20260911_165458.jsonl`).

**Deck state at the pre-bet dump**: slot 1467 (`kDeckCursorOffset`) = 0,
slot 1468 (`kDeckCountOffset`) = 52 -- exactly the untouched,
freshly-reshuffled window `SimulatePreDeal()` targets.

**Replaying `SimulatePreDeal()`'s own algorithm by hand against that raw
dump**, seat by seat (`kTableSlot`=771, `kSeatsBase`=27, `kSeatStride`=60 ->
seat bases 798/858/918/978):

| Seat | occupied (offset+0) | f_7 confirmed (offset+7) | f_4[0] bet (offset+4) | Predicted from deck |
|---|---|---|---|---|
| 0 (human) | 0 (occupied) | **0** (not yet) | 2 | deck[0..1] |
| 1 (NPC) | 1 (occupied) | **1** (already!) | 2 | deck[2..3] |
| 2 | -1 (empty) | -- | -- | skipped |
| 3 (NPC) | 3 (occupied) | **1** (already!) | 2 | deck[4..5] |
| dealer | -- | -- | -- | deck[6..7] |

The two NPC seats had ALREADY confirmed their bets before the human even
saw the bet prompt -- direct, live proof of the exact mechanism
`func_1056` was traced from (every occupied seat must have `f_7` set
before the table leaves state 0; NPCs clearly do this instantly, so the
table visibly waits on the human).

Raw deck contents at that moment (rank,suit pairs, slots 1363+): index
0-1 = 7,0 (7H); 2-3 = 9,3 (9C); 4-5 = 13,0 (KH); 6-7 = 4,3 (4C); 8-9 = 8,3
(8C); 10-11 = 5,3 (5C); 12-13 = 13,1 (KD); 14-15 = 2,0 (2H).

**Second dump, after betting and dealing** -- checked against the user's
own read of the real screen (dealer KD/2H, their own hand 7H/9C):

- Deck cursor (slot 1467): **8** -- exactly 4 pairs consumed (3 seats +
  dealer), matching the prediction table above.
- Deck cards (slots 1363-1382 spot-checked): byte-identical to the first
  dump -- direct proof the deck was never touched by the bet, only the
  cursor advanced.
- Seat 0 hand (slot 808+): 7,0 then 9,3 = **7H, 9C** -- matches the
  user's real hand exactly.
- Seat 1 hand (slot 868+): 13,0 then 4,3 = **KH, 4C** -- matches the
  predicted deck[2..3] exactly.
- Seat 3 hand (slot 988+): 8,3 then 5,3 = **8C, 5C** -- matches the
  predicted deck[4..5] exactly.
- Dealer hand (`Table.f_2`, slot 773+): 13,1 then 2,0 = **KD (hole),
  2H (up)** -- matches both the predicted deck[6..7] AND the user's real
  screen read exactly.
- Seat 0's `f_7` (slot 805): **0 in dump 1, 1 in dump 2** -- the human's
  own confirm, caught live, while seat 1/3's `f_7` stayed 1 the whole
  time.
- Seat 0's bankroll (slot 799): **400 -> 398**, exactly the $2 bet --
  incidentally confirms `kSeatBankrollOffset`/`kSeatBetOffset` together.
- `mySeat` via `f_9` (slot 23): **0 in both dumps**, consistent with
  seat 0 being the human's real hand.

**Result: every offset this feature touches -- `kSeatBetConfirmedOffset`
(brand new this session), plus the reused `kSeatOccupiedOffset`,
`kSeatBetOffset`, `kDeckCursorOffset`/`kDeckCountOffset`, `kTableSlot`,
`kDealerHandOffset`, `kSeatHandsOffset` -- was correct on the FIRST live
test.** No corrections needed, a rare clean pass for this project (compare
Session 6, where the same kind of test needed 4 corrections). Promoted
`kSeatBetConfirmedOffset` to CONFIRMED LIVE in `BlackjackCheat.cpp`.

**Still open**: the two new Release+Debug icon positions (dealer's
predicted 2 cards reusing the `HoleCardIconX/Y` slot, the player's
reusing `NextCardIconBaseX/Y`) were not visually confirmed this pass --
the user read the real screen and the raw dumps, not the on-screen icon
layout. May still need the same kind of live Reload-Config retuning
`HoleCardIconX` itself needed (0.821 -> 0.957) once actually looked at.

### Second live-testing round (same session) -- two real bugs found, `ShowCardsBeforeBet` added

Play continued (Debug build still deployed) and immediately surfaced
exactly the "still open" item above, plus one more real bug:

**Bug 1 -- the predicted own-hand icon never appeared.** The user
confirmed this directly: "It's just showing the dealer's cards, not my
cards." Root cause: `SimulatePreDeal()` required `seat.f_7` (bet
CONFIRMED) for every seat including the human's own, matching
`func_1057`'s real gate exactly -- but `func_1056` ALSO requires the
human's own `f_7` before the table can leave state 0 to deal, so the
real window where "my own `f_7` just flipped to 1 AND `dealerHand.count`
is still 0" both hold is at most one script tick, often zero visible
frames. The dealer's icon showed fine because it never depends on
`mySeat` -- the first live-confirmation addendum above already showed
both NPC seats confirm their bets well before the human even sees the
bet prompt, so `preDeal.valid` was true for the entire deciding window;
only the human-specific gate was ever starved for a visible frame.

Checked PokerCheat's own opponent/community-card drawing
(`DrawCommunityCardIcons()`/`DrawSeatCardIcons()` in `PokerCheat.cpp`) as
the user asked, specifically to rule out a rendering-technique problem --
it's architecturally identical to what this file already does
(`DRAW_SPRITE` + `BuildCardTextureName()` + a streamed `card_set_N`
dict, no header/config difference worth porting). Confirms the bug was
never in HOW cards get drawn -- it was in WHEN this file decided a hand
was safe to draw.

**Fix**: `SimulatePreDeal()` now takes an explicit `mySeat` parameter and
treats that one seat as "will play" the moment its bet AMOUNT (`f_4[0]`)
is nonzero, without waiting for `f_7` -- the first live-confirmation
addendum's own dump data already showed this field reads nonzero well
before confirming (the bet slider's live value). Every OTHER seat still
requires the real `f_7`, unchanged from before. This is a strictly more
provisional guess for `mySeat` specifically (could show a hand that never
gets dealt if the human backs their bet down to 0 without ever
confirming) -- accepted tradeoff, since showing an occasionally premature
preview during a window that otherwise showed NOTHING at all is the
entire point of the feature.

**Bug 2 -- a real dealer misprediction, caught on the very next round**:
the user reported "It showed a 9S 10C but the dealer ended up having 2D
5D." Root cause understood, NOT fully solvable in general: unlike the
dealer draw-out prediction (`SimulateDealerOutcome()`, where the cursor
only ever advances forward, making self-correction monotonic), this
pre-deal prediction's seat-to-deck-index mapping can shift
non-monotonically any time ANOTHER seat's bet confirms during the
waiting phase -- any occupied seat, not just the human's, could in
principle confirm right up against the same "last tick before dealing"
edge Bug 1's fix addresses for the human specifically, and this file has
no way to distinguish "no more seats are joining" from "one more seat is
about to confirm" ahead of time.

Rather than chase this further blind, added `ValidatePreDeal()` (right
after `SimulatePreDeal()`) as an **instrument, not a fix** -- the same
"PredictionCheck every round, no F10 needed" self-validation
`UpdateDeckPrediction()` already does for the dealer draw-out. It now
logs a per-seat + dealer MATCH/MISMATCH ("PreDealCheck" lines) every
single round automatically, so future sessions can characterize how
often/why this actually happens instead of relying on the user noticing
by eye and reporting it manually (exactly what happened this time).

**New feature, user request**: `Config::Values::ShowCardsBeforeBet`
(Release+Debug, default true) -- a dedicated toggle for this specific
feature, separate from `ShowDeckPrediction` (which now only ever governs
the post-deal dealer-hole-card icon/"Next cards" row). Reasoning: this
pre-bet prediction is a distinctly more provisional guess than anything
else `ShowDeckPrediction` gates (see both bugs above), so the user should
be able to turn it off independently. `Config.h`/`Config.cpp` updated the
same way every prior toggle was (INI key `ShowCardsBeforeBet` under
`[General]`).

### Build/test status

`BlackjackCheat.vcxproj` Debug compiled clean (0 warnings/errors) while
RDR2.exe was still running for live testing -- deploy step correctly
failed on the file lock both times the game was still open, succeeded
once the user fully closed RDR2.exe. Confirmed via this session's own
back-and-forth that ejecting a loaded ASI through the loader's own
eject/reinject flow does NOT reliably release the OS file handle (two
consecutive eject/reinject attempts still left the file locked) --
closing the process is the only mechanism confirmed to work here. No
pure-math header (`BlackjackHandEval.h`/`BlackjackDeckSim.h`) touched
this session, so their test projects were not re-run.

### Third live-testing round (same session) -- Bug 1's fix confirmed, a new sit-down-timing theory, and a deliberate simplification

Redeployed the Bug 1/Bug 2/`ShowCardsBeforeBet` build above and kept
playing.

**Bug 1's fix confirmed live**: the user's own predicted hand appeared in
the reused "Next cards" slot and matched their real dealt hand. The very
same round's automatic `PreDealCheck` log line also came back clean:

```
PreDealCheck: dealer predicted=[ 10S QH ] actual=[ 10S QH ] MATCH
PreDealCheck: seat 1 predicted=[ AS 8H ] actual=[ AS 8H ] MATCH
PreDealCheck: seat 2 predicted=[ 3S 7H ] actual=[ 3S 7H ] MATCH
PreDealCheck: seat 3 predicted=[ 2D 6H ] actual=[ 2D 6H ] MATCH
```

But the user separately reported the top-right predicted-DEALER icon
(both cards, pre-deal) looked wrong on that same sit-down -- despite the
log for that exact round showing a clean match. Reconciling the two: the
log only captures the LAST snapshot right before the deal (by
definition, the one that matters), but the user could easily have looked
at an EARLIER, not-yet-settled snapshot -- and first-hand-after-sitting-
down is specifically the highest-risk moment for that, since the human's
own seat takes several real seconds to register (walk up + sit + place a
first bet) versus NPCs, which the earlier addendum already showed
confirm in a single tick. More time spent "unsettled" means more chance
of eyeballing a guess before it self-corrects. On the very next hand
(already seated, no sit-down delay), the user confirmed it displayed
correctly -- consistent with this theory, though not yet proven by a
dedicated stand-up/sit-back-down test.

**Deliberate simplification (user directive): "Assume everyone at the
table will be betting."** Rather than keep chasing the general race
(any occupied seat's bet can in principle confirm late enough to shift
the dealer's predicted index, as Bug 2 already showed), `SimulatePreDeal()`
now gates purely on **occupancy** (`kSeatOccupiedOffset`) for every seat,
human and NPC alike -- the `seat.f_7` (bet confirmed)/`seat.f_4[0]` (bet
amount) checks, and the human-specific bypass Bug 1's fix added, are
gone entirely. Rationale: in this minigame every seated player
realistically does bet every round, so treating "occupied" as "will
play" collapses both known races into one much smaller one (a seat's
occupancy marker should register as soon as the seat is taken -- well
before any bet-related field is even meaningful) at the cost of a known,
accepted inaccuracy if a seat is ever occupied but genuinely sits a
round out. `ValidatePreDeal()`'s existing "PreDealCheck" log line will
surface that as a real MISMATCH automatically if it ever actually
happens, rather than this file silently assuming it never does --
exactly the kind of case that instrument exists to catch. `kSeatBetOffset`/
`kSeatBetConfirmedOffset` remain valid, confirmed fields (Probe-only
again, not read by `OnTick()`), not removed -- only this file's own
gating logic changed.

Rebuilt Debug (0 warnings/errors) while RDR2.exe was still running for
live testing -- deploy step correctly failed on the file lock again,
matching the established pattern; redeploy once the user closes the
game. Not yet re-tested live as of this edit.

### Fourth finding (same session) -- the real reason eject was hanging: `Log.h`'s async logger

Separately from the deck-prediction work above, the user had been unable
to get a build's eject/reinject cycle to actually release the `.asi`
file handle across several attempts this session (see the "Build/test
status" note a few sections up: two consecutive eject/reinject attempts
still left the file locked, only a full RDR2.exe close ever worked). The
user asked directly whether `Log.h`'s logging framework -- specifically,
whether it keeps a thread alive -- could be the cause.

It is. `Log::detail::GetLogger()` used `spdlog::create_async<...>`,
which spawns spdlog's global thread pool (1 background worker thread).
That worker's code is compiled directly into this DLL (spdlog is
header-only, and nothing else in this project links it), so the thread
is running code inside BlackjackCheat.asi's own mapped pages for as long
as it's alive. Traced the actual mechanism in
`external/spdlog/include/spdlog/details/thread_pool-inl.h`:
`thread_pool::~thread_pool()` calls `t.join()` on that worker thread.
That destructor runs when `GetLogger()`'s function-local static
`logger` (the last shared_ptr keeping the pool alive) gets torn down
during DLL unload -- i.e. from inside `DllMain`'s `DLL_PROCESS_DETACH`.
Joining a thread from inside `DllMain` is a well-documented Windows
deadlock trap: the OS loader lock is held for the entire call, and
`main.cpp`'s own `DLL_PROCESS_DETACH` case (`scriptUnregister()` +
`keyboardHandlerUnregister()`, no explicit `spdlog::shutdown()`) gave no
indication this was even a risk until traced. This matches the observed
symptom exactly -- not a crash, a silent HANG, which from the injector's
side looks exactly like "eject just doesn't complete."

**Fix, per explicit user direction ("ONLY for debug. Release should be
async")**: `Log.h` now branches on `_DEBUG`. Debug uses
`spdlog::basic_logger_mt<spdlog::synchronous_factory>(...)` -- a plain
synchronous logger, no background thread pool at all, so there is
nothing for `DLL_PROCESS_DETACH` to deadlock joining. Release keeps the
original `spdlog::create_async<...>` unchanged, on the reasoning that
Release loads once at game launch and is never hot-ejected during normal
play, so it keeps the async logger's lower per-call overhead where the
deadlock risk realistically never gets exercised. This project's own
build-test-eject-reinject loop happens overwhelmingly against Debug
(many times an hour), which is exactly where the fix now lives.

**Caveat, explicitly not verified this session**: Release's own
`DLL_PROCESS_DETACH` still happens at ordinary game exit (not just a
manual eject) -- this session's hang was only ever observed via manual
eject, never via closing RDR2.exe normally (which was the ONLY thing
that reliably worked to unstick the file lock all session), so Release's
same theoretical risk at normal process exit was never actually
triggered or tested. Flagged as a real, if lower-probability, open
question rather than assumed safe.

**A second lesson from this same fix, unrelated to the logger itself**:
Debug and Release both deploy to the SAME game folder via the shared
`PostBuildEvent` (`E:\SteamLibrary\...\Red Dead Redemption 2`) -- building
Release right after Debug during this session's verification silently
overwrote the Debug `.asi` the user was mid-session live-testing with
(F10 menu, `PreDealCheck` logging, all Debug-only). Caught and corrected
by rebuilding Debug again immediately after, but worth remembering
explicitly for any future session: **always redeploy Debug last if a
Release verification build was needed mid-session**, since there's no
separate deploy path to keep them from clobbering each other.

### Build/test status (this addendum)

Both `Debug` and `Release` rebuilt clean (0 warnings/errors) with
RDR2.exe fully closed, both deployed successfully. Order was Debug,
Release (for compile verification only), then Debug again to restore
the user's actual live-testing build -- see the note directly above.
Not yet live-tested whether this actually fixes the eject hang (the user
had not attempted another eject/reinject cycle as of this edit).

### Next live session priority (supersedes the priority list above)

1. **Highest priority given this session's history**: actually try an
   eject/reinject cycle again against the new Debug build and confirm it
   no longer hangs -- this was the whole reason the fix above exists,
   and hasn't been verified yet.
2. Confirm the occupancy-only simplification actually fixes the
   first-sit-down inaccuracy -- stand up and re-sit (cheaper than a full
   game reload) specifically to reproduce a fresh "just occupied, nothing
   else registered yet" moment, and check both the on-screen icon and the
   `PreDealCheck` log line for that round.
3. Watch for "PreDealCheck" MISMATCH lines across several rounds now that
   the gate is occupancy-only -- in particular, does a seat that's
   occupied but doesn't bet (if that's even possible in this minigame)
   ever actually produce one?
4. The two icon positions (reusing `HoleCardIconX/Y`/`NextCardIconBaseX/Y`)
   still haven't been visually confirmed as sane -- first actual look at
   the screen should say whether they need their own dedicated config
   keys and a live-tuning pass.
5. Eventually: if Release's own eject-time risk (see the caveat above)
   ever needs closing off too, the same synchronous-logger swap is a
   one-line change away, at the cost of the async logger's lower
   per-call overhead in the shipped build.
6. Backfill Sessions 7 and 8 into this journal (still not attempted, see
   the earlier note in this same session's entry).

### Fifth finding (same session) -- eject fix confirmed live, then a real correctness bug in the advice engine

The eject fix above was tested live: with the new Debug build deployed,
the user injected then ejected while RDR2.exe stayed open, and a
subsequent `MSBuild` deploy attempt succeeded (`1 file(s) copied` for
both `.asi` and `.pdb`) -- the FIRST time this session an eject actually
released the file lock without closing the game. Confirms the
synchronous-logger fix worked.

Play continued and surfaced two more real issues, addressed in priority
order (the user explicitly flagged the second as more urgent mid-report):

**Issue A (lower priority, addressed second) -- the pre-deal prediction
was showing up while the PREVIOUS round's dealer draw-out was still
visually playing.** Root cause: exactly the same "script state resets
before the animation finishes" mechanism the very first Session 9
finding already traced (func_718 case 0 resets `Table.f_2`/reshuffles
the deck on the FIRST script tick after a round ends), but never
previously connected to a visible symptom -- the real "dealer collects
cards" animation (`func_1049`) plays out over several more real seconds,
completely decoupled from that already-updated state, so
`dealerHand.count` reads 0 well before the table visually finishes
showing the OLD round. `SimulatePreDeal()`, gated purely on that single
tick's read, had no way to distinguish "genuinely between rounds" from
"state already reset, animation still catching up".

Fix: `IsPreDealSettled()` (`BlackjackCheat.cpp`, right after
`ValidatePreDeal()`) requires `dealerHand.count==0` to persist for a
minimum REAL time (`std::chrono::steady_clock`, not a tick count --
frame rate isn't fixed) before the prediction is trusted. New config
field `Config::Values::PreDealSettleDelaySeconds` (Release+Debug,
default 1.5s) -- a heuristic guess at how long that animation typically
takes, not a traced fact, explicitly flagged as likely needing its own
live-tuning pass the same way `HoleCardIconX` did (0.821 -> 0.957).

**Issue B (higher priority, the user's explicit "New issue" interrupt)
-- a hard 9 (5,4 -- mathematically cannot bust on any single card) was
advised Stand.** Traced the exact mechanism in
`BlackjackDeckSim::DetermineCheatAction()`: the dealer showed a hand
that needed to hit, and the very next undrawn deck card would have
busted it. Standing denies the player nothing (the dealer draws that
exact card next and busts, an automatic win regardless of the player's
own low total) -- but hitting would consume that SAME card for the
player instead, letting the dealer draw a safe card afterward and beat
the player's now-higher-but-still-losing total. This is a real,
mathematically correct insight -- but ONLY if nothing else can draw
between this hand and the dealer's turn. It wasn't true that round:
another occupied seat still had to act first, meaning the "dealer's very
next card" the engine assumed was actually going to be consumed by that
OTHER seat's real hits, not handed straight to the dealer. This is
precisely the caveat `BlackjackDeckSim.h`'s own header comment already
documented ("if another occupied seat still has to act before the
dealer, their real hits will shift the cursor by an amount this function
can't predict") -- documented as a risk, never previously checked as a
precondition.

Fix: `DetermineCheatAction()` gained a new required parameter,
`isLastSeatBeforeDealer`. When false, the function no longer trusts ANY
of its own dealer-outcome simulation (not just the specific denial case
above -- every `extraHits` candidate's dealer comparison shares the same
broken premise) and defers entirely to
`BlackjackHandEval::GetBasicStrategyAction()` -- the same textbook
fallback this file already used for Split, now extended to Hit/Stand/
Double too. `BlackjackCheat.cpp`'s `DrawOverlay()` computes this per-tick,
per-seat: turn order is strictly ascending seat 0->1->2->3 then the
dealer (Session 5), so "is mySeat last to act before the dealer" is
simply "is any HIGHER-indexed seat occupied" -- a single scan over
`kSeatOccupiedOffset` for seats above `mySeat`.

New regression test, `TestHardNineDeniedDealerBust`
(`tests/BlackjackDeckSimTests.cpp`), reproduces the exact bug shape:
dealer at 16 with a bust card (6) at the front of a long future-card
array of otherwise-harmless 4s. Asserts BOTH halves of the fix: with
`isLastSeatBeforeDealer=true`, Stand is still the mathematically correct
answer (proves the ORIGINAL insight wasn't wrong, just misapplied); with
`isLastSeatBeforeDealer=false`, the same hand now correctly falls back
to basic strategy's Double (hard 9 vs. dealer up-card 6, `canDouble`
true) or Hit (`canDouble` false) -- never Stand. All 12 checks in the
suite pass (9 previous + 3 new).

### Build/test status (this addendum)

`BlackjackCheat.vcxproj` Debug compiled clean (0 warnings/errors);
deploy correctly failed on the file lock while the mod was still
injected for live testing (expected, not re-tested against the new
build yet). `tests/BlackjackDeckSimTests.vcxproj` rebuilt and re-run:
12/12 pass. `tests/BlackjackHandEvalTests.exe` re-run unchanged: still
31/31 (no header touched). Release not rebuilt this addendum.

### Next live session priority (supersedes the priority list above)

1. **Highest priority**: confirm the hard-9-Stand bug is actually fixed
   live -- watch for a similar low-total hand while another seat is
   still occupied and due to act, and confirm the advice is now Hit/
   Double, never Stand, in that situation specifically.
2. Confirm `IsPreDealSettled()`'s 1.5s guess is in the right ballpark --
   does the pre-deal prediction now wait until the real table has
   visually finished the previous round before appearing? Too short
   still shows it early; too long delays a real feature for no reason.
   Tune `PreDealSettleDelaySeconds` via Reload Config if not.
3. Confirm the occupancy-only pre-deal simplification actually fixes the
   first-sit-down inaccuracy (carried over from before this addendum,
   not yet re-tested).
4. Watch for "PreDealCheck" MISMATCH lines across several rounds.
5. The two icon positions (reusing `HoleCardIconX/Y`/`NextCardIconBaseX/Y`)
   still haven't been visually confirmed as sane.
6. Backfill Sessions 7 and 8 into this journal (still not attempted).

### Sixth finding (same session) -- `IsPreDealSettled()`'s time debounce did NOT fix Issue A; added a real diagnostic instead of guessing again

Deployed the settle-delay build above; the user reported the pre-deal
prediction was STILL showing during the previous round's conclusion.
Rather than blindly try a longer delay, the user asked directly whether
some kind of table state could be checked instead -- the same request
this session's very first trace already answered once (`Table.f_580`,
`func_718`'s own round-phase state variable) but never actually wired
into this codebase as a live-readable field.

Added `kTableStateOffset = 580` (`BlackjackCheat.cpp`) and exposed it in
two places: an always-visible Debug panel line ("Table state (f_580,
diagnostic): N settled=yes/no") and a new log line in
`ProbeTableStruct()`. Explicitly flagged as **STATIC TRACE ONLY, not yet
live-confirmed** -- it reuses the same `kTableSlot` base already
confirmed correct for 3 independent other fields (`f_2`/`f_27`/`f_592`),
which is reasonable evidence by analogy, but the specific relative
offset (580) itself has never been checked against real memory.

**Also flagged, not yet resolved**: per the file header's own Session 9
trace, `func_718`'s switch has NO intermediate "still collecting
cards"/"still animating" state -- case 8 (dealer draw-out) and case 9
both jump straight to state 0 in the same tick the round ends, and state
0 is also where the reshuffle and the waiting-for-next-bet loop both
live. If that trace is right, `f_580` reading 0 won't actually
distinguish "just ended, animation still playing" from "genuinely
waiting for a bet" any better than `dealerHand.count==0` already did --
this diagnostic exists specifically to find out whether that's true, not
because it's assumed to be the fix. This is being added FOR live
observation, not as a claimed solution -- the honest position given two
guesses (the seat-timing race, then a 1.5s debounce) that didn't fully
solve Issue A on their own.

### Build/test status (this addendum)

`BlackjackCheat.vcxproj` Debug compiled clean (0 warnings/errors);
deploy failed on the file lock while the mod was still injected
(expected, not yet redeployed as of this edit).

### Next live session priority (supersedes the priority list above)

1. **Highest priority**: watch the Debug panel's new "Table state
   (f_580)" line across a full round, specifically during the window
   where the pre-deal prediction incorrectly shows -- what value does it
   read? If it's already 0 during that window (as the file header's own
   trace predicts), `f_580` alone won't solve Issue A and a genuinely
   different signal is needed (or the animation-length guess needs
   fixing some other way). If it reads something OTHER than 0 during
   that window, this directly contradicts the existing case-8/9 trace
   and is a real, useful correction to make.
2. Confirm the hard-9-Stand fix and the occupancy-only pre-deal
   simplification, both still not re-tested live as of this addendum.
3. Watch for "PreDealCheck" MISMATCH lines across several rounds.
4. The two icon positions still haven't been visually confirmed as sane.
5. Backfill Sessions 7 and 8 into this journal (still not attempted).

### Seventh finding (same session) -- Issue A actually solved: `f_580` was off by one, the real field is `f_581`

The user watched the new diagnostic live and reported exactly the
answer priority item 1 above asked for: the field reads **1** right
after the round concludes and **0** specifically once genuinely at the
betting phase -- the opposite of what the existing case-8/9 trace
predicted for `f_580` itself. Rather than just trust the correlation,
the user pushed back with the right question: could this be the same
kind of off-by-one this project has hit before, or a genuinely different
field?

Checked properly this time instead of trusting the live correlation
blind:
- `func_1047` (line ~35146) -- confirmed it really does do
  `uParam0->f_580 = iParam1`, exactly as assumed; the switch case labels
  (case 8/9 calling `func_1047(uParam0, 0)`) are unambiguous in the
  source. So the CONTRADICTION was real, not a misreading of the switch.
- `func_1049` (line ~35164, called from case 0 with token `1` -- the
  call this file always described as "kicks off a retrieve-bets/payout
  animation" but never actually opened) -- turned out to write
  **`f_581`**, not `f_580`: `if (iParam1==0) return; f_581 = iParam1;`.
- `func_272` (line ~12696, the outer per-tick driver) only even calls
  `func_718` -- the ENTIRE round-phase state machine -- when
  `f_581 == 0`: `if (uParam0->f_581 == 0) { if (func_718(uParam0)) {...} }`.
  So while `f_581` is nonzero, `f_580` (and everything else func_718
  would otherwise update) stays frozen exactly where case 8/9 left it.
- `func_354` (line ~14657) is the release: it clears `f_581` back to 0
  ONLY if the caller's token matches the CURRENT lock value (a
  token-guarded unlock so one animation's completion can't accidentally
  release a different one's lock).

This is a genuine off-by-one (`kTableStateOffset=580` was reading the
wrong word), same class of bug as `kTableFieldOffset`/`kSeatHandsOffset`
before it -- but a lucky one: `f_581` is a real, explicit "table is
paused for an animation" lock, a BETTER signal for this exact purpose
than the raw switch-case index would even have been. Renamed to
`kTableAnimationLockOffset = 581`, rated CONFIRMED LIVE (the live
behavior the user reported was real; this file's own understanding of
which absolute slot it was reading was wrong).

**Fix applied**: `IsPreDealSettled()`'s 1.5s time-guess (Session 9 fourth
addendum) is REMOVED entirely, replaced by `IsAtBettingPhase()` -- a
direct read of `f_581 == 0`, no timing heuristic at all.
`Config::Values::PreDealSettleDelaySeconds` removed along with it
(`Config.h`/`.cpp`). The Debug panel's diagnostic line now shows
"Table animation lock (f_581): N atBettingPhase=yes/no" instead of the
old "Table state (f_580)" line, and `ProbeTableStruct()`'s log line was
updated the same way.

### Build/test status (this addendum)

`BlackjackCheat.vcxproj` Debug compiled clean (0 warnings/errors);
deploy failed on the file lock while the mod was still injected
(expected, not yet redeployed as of this edit). No pure-math header
touched, test projects not re-run.

### Eighth finding (same session) -- the f_581 theory was wrong; reverted to f_580, empirically

The `f_581` animation-lock explanation (previous finding) was deployed
and immediately falsified live: the user reported it reads a **constant
1**, never toggling, regardless of round phase -- not the real signal at
all, despite the clean-looking `func_1049`/`func_272`/`func_354`
mechanism that seemed to explain it. Reverted `kTableAnimationLockOffset`
back to 580 on the user's explicit direction ("Go back to 580 and just
check for 1"), trusting the ORIGINAL direct live observation (reads 1
while the previous round is still resolving, 0 once genuinely at the
bet-placing phase) over both theories tried so far.

`IsAtBettingPhase()`'s own logic (`ReadInt(...) == 0`) didn't need to
change -- only the offset constant did, plus the header comments that
had confidently narrated the (wrong) `f_581` mechanism. This field's
real identity/mechanism remains genuinely unexplained -- not re-derived
this session. Rather than keep guessing at WHY it works, this file now
explicitly documents it as empirical: gate on the observed 1->0
transition, full stop, same "trust the live probe over the clean-looking
static theory" precedent this project already follows for `kMySeatSlot`.

**A real methodology lesson worth naming plainly**: this offset was
"confirmed" and then un-confirmed TWICE in one session (580 as
switch-case index -- wrong; 581 as animation lock -- also wrong; back to
580 as an unexplained empirical signal -- what actually survived contact
with live testing). Neither wrong theory was a wasted detour exactly --
each was falsifiable and got falsified fast -- but it's a concrete
demonstration of why this project's own confidence-rating discipline
(STATIC TRACE ONLY vs. CONFIRMED LIVE) exists: a theory that reads
cleanly in the decompile is not the same claim as a value a live probe
actually confirms, and this offset still isn't the latter yet either --
it's "empirically works, mechanism unknown," a third, distinct category
worth naming honestly rather than dressing up as either of the other two.

Rebuilt Debug: compiled clean, deployed successfully on the first try
(the file was already unlocked -- no eject needed this time).
`tests/BlackjackDeckSimTests.vcxproj` also rebuilt and re-run against
the externally-updated `BlackjackDeckSim.h` (see that header's own
Session 10 addendum for two more live bug fixes -- a known bust card now
overrides the isLastSeatBeforeDealer fallback, and an already-pat dealer
is trusted regardless of other seats -- made outside this journal entry,
not authored by this session): 16/16 pass (14 previous + 2 new).

### Next live session priority (supersedes the priority list above)

1. **Highest priority**: confirm the reverted `f_580` check actually
   fixes Issue A live -- does the pre-deal prediction now stay hidden
   through the entire previous round's conclusion and only appear once
   genuinely at the bet-placing phase?
2. If it does work but the mechanism still nags at someone: a proper
   re-derivation would mean a differential stack dump (before/after
   round conclusion) scanning a window around slot `kTableSlot+580` for
   what ELSE might explain a clean 1->0 transition -- not attempted this
   session, this fix shipped on trusted live observation alone.
3. Confirm the hard-9-Stand fix (Session 9 fifth finding), the
   Session 10 DeckSim fixes, and the occupancy-only pre-deal
   simplification -- none re-tested live as of this addendum.
4. Watch for "PreDealCheck" MISMATCH lines across several rounds.
5. The two icon positions still haven't been visually confirmed as sane.
6. Backfill Sessions 7 and 8 into this journal (still not attempted).

## Session 16 -- localization: HUD advice/betting/insurance/next-cards labels into RDR2's 13 shipped languages

Ported `../PokerCheat`'s Session 19-21 localization work over to this
project (user request, right after PokerCheat's own localization
branch merged). Same approach, different string set: new
`src/Localization.h/.cpp` hold a `Language` enum matching
`LANGUAGE::_GET_CURRENT_LANGUAGE_ID()`'s own 13-language return-value
mapping exactly (identical enum to PokerCheat's), auto-detected on
first use and re-resolved from the F11 menu's "Reload Config" item
(now `ReloadConfigAndLocalization()` in `script.cpp`, same wrapper
PokerCheat's `script.cpp` already has). `BlackjackCheat.ini`'s new
`[General]` `Language` key ("auto" default) can override it, same
`Config::Values::Language` field/semantics as PokerCheat's.

This mod's actual Release-visible on-screen text turned out to be a
different (smaller) set than poker's verdict/personality tags:

1. `ActionName()` -- the HIT/STAND/DOUBLE/SPLIT advice readout
   (`DrawAdviceStatus()`).
2. `BettingConfidenceLabel()` -- BET LOW/MEDIUM/HIGH
   (`DrawBettingAdviceStatus()`).
3. `InsuranceLabel()` -- "Insurance: YES"/"Insurance: No"
   (`DrawInsuranceStatus()`).
4. `NextCardsLabel()` -- "Next cards:" (`DrawNextCardStatus()`).

All four were previously small local `switch`/ternary functions
directly in `BlackjackCheat.cpp`; those are now removed and their call
sites point at the `Localization::` equivalents. The Debug-only text
panel (`DrawLine()`/`DrawPanel()`, gated `#ifdef _DEBUG`) stays
English-only, same reasoning PokerCheat's `DrawFontTest()`/debug panel
already documents -- a dev diagnostic surface, not something an end
user needs translated.

**No separate font test tool was built here.** PokerCheat's own
Session 20/21 already confirmed, on this exact game build (1491.50),
that `$Font5` (the `UIDEBUG::_BG_DISPLAY_TEXT` pipeline both mods use
verbatim -- see `WrapBgFormatText()`'s header comment in
`BlackjackCheat.cpp`, which already cites PokerCheat's own font
derivation) renders all 13 languages correctly, CJK included, PROVIDED
RDR2's own actual configured language matches what's being tested (see
PokerCheat's `docs/PITFALLS.md` for the "ini override doesn't load
game assets" lesson -- carries over unchanged, not re-litigated here).
Since this mod draws through the identical pipeline/font token with no
poker-specific rendering path, re-running that whole investigation here
would just reconfirm the same game-build fact a second time.

**A real build gap found along the way**: `BlackjackCheat.vcxproj`
had no `/utf-8` `AdditionalOptions` on any configuration, unlike
`PokerCheat.vcxproj` (which already carries it, there for an unrelated
reason -- spdlog's bundled fmt static-asserting on the code page).
This project defines `SPDLOG_USE_STD_FORMAT` (see `Log.h`), which
sidesteps that particular static_assert, so the flag was simply never
needed before. But `Localization.cpp`'s Cyrillic/CJK/Hangul string
literals are non-ASCII regardless of spdlog -- without `/utf-8`, MSVC
reinterprets a UTF-8-saved source file against the current ANSI code
page instead, silently mangling every non-Latin translation into
mojibake (or worse, producing a `C4566` warning and replacing
unrepresentable characters with `?`). Added `/utf-8` to all three
configurations (Release/Debug/Analyze) as part of this change, not
just the two PokerCheat's own build needed. Confirmed post-build: all
three configurations (`Release`, `Debug`, `Analyze`) compile clean,
and the checked-in `Localization.cpp` round-trips as valid UTF-8
containing real Cyrillic/CJK/Hangul codepoints (verified by decoding
the file and pattern-matching each script's Unicode block) -- not yet
confirmed rendering correctly in an actual running game with a non-
English UI language, same "static claim, not yet live-verified" caveat
this project applies to everything else. If a future session sees
tofu/mojibake for a specific language in-game, check the compiled
`.asi`'s string table for the expected bytes before assuming the
translation itself (rather than the encoding pipeline) is at fault.

### Build/test status

`BlackjackCheat.vcxproj` Release, Debug, and Analyze all compiled
clean (0 warnings/errors) and Debug/Release deployed successfully
(RDR2.exe was not running). No pure-math header touched --
`BlackjackHandEvalTests`/`BlackjackCardCountingTests`/
`BlackjackDeckSimTests` not re-run, none of their headers changed.

### Next live-session priority (in addition to the existing struct-offset priority list above)

1. With RDR2's own UI language actually switched away from English
   (Steam Properties -> Language, game relaunched -- NOT just this
   mod's ini override, per the pitfall above) and a hand in progress,
   confirm the advice/betting/insurance/next-cards labels render
   correctly on screen for at least one non-Latin language (Russian or
   Chinese/Japanese/Korean) and one accented-Latin language (French or
   German).
2. Confirm the `[General]` `Language` ini override actually forces a
   different language than the game's own UI setting when set to an
   explicit code (e.g. `fr-FR` while the game itself runs in English).
3. Have a native speaker (or at least a second LLM pass) review the
   translations beyond English -- these are LLM-assisted and unreviewed,
   same caveat PokerCheat's own `kPersonalityLabels`/`kVerdictLabels`
   carry.

### Same-session addendum -- web-search verification pass against real casino glossaries

Immediately after the above, the user asked how confident these
translations actually were and whether anything could be done to raise
that confidence beyond "LLM instinct." Answer: yes -- `WebSearch` each
language's own real blackjack rules/glossary pages and diff the actual
terms found against `kActionLabels`/`kInsuranceLabels`. Ran one search
per language (French/German/Italian/Spanish/Portuguese/Polish/Russian/
Korean/Chinese/Japanese) plus a follow-up disambiguating German's
Stand term specifically.

**Result: most of the original pass held up.** fr-FR, it-IT, es-ES/
es-MX, pt-BR, ko-KR, zh-TW/zh-CN, and ja-JP's HIT/STAND/DOUBLE/SPLIT
words all matched real sources (regles.com, it.blackjackinfo.com,
casino.org/es, pt.pokernews.com, reviewland.net, baike.baidu.com,
ja.wikipedia.org) exactly as originally guessed -- including that
Korean/Japanese casinos really do use the English loanwords
(히트/스탠드/더블/스플릿, ヒット/スタンド/ダブル/スプリット) rather than
native translations, which was a guess rather than a certainty going
in.

**Two real, concrete errors found and fixed:**
- German Stand: `HALTEN` was wrong, `STEHEN` is the term actually used
  (confirmed via a dedicated follow-up search after the first pass came
  back ambiguous between the two).
- Russian Stand: `СТОП` was wrong, `ХВАТИТ` is the term actually used
  (gipsyteam.ru/casino.ru).

**Diacritics restored** (an earlier ASCII-safety instinct, from before
`/utf-8` was confirmed working, had stripped several): fr-FR
`SÉPARER`/`ÉLEVÉE`, de-DE `Nächste`, it-IT `SÌ` (not bare "si", which
is the reflexive pronoun), es-ES/es-MX `SÍ` (not bare "si", the
conditional "if"), pt-BR `Não`/`Média`/`Próximas`, pl-PL `ZAKŁAD`/
`ŚREDNI`/`Następne`, ja-JP `インシュアランス` (the correct
transliteration -- the original `インシュランス` was missing a kana).

**Not source-verified, still a guess**: pl-PL's `PODWÓJ`/`PODZIEL`
(Double/Split) are imperative forms built from confirmed nouns
("podwojenie"/"podział") but not independently confirmed as what a
real Polish-language table actually displays. The `BET LOW/MEDIUM/
HIGH` and `"Next cards:"` tables are this mod's own invented HUD
concepts with no textbook/casino equivalent to verify against, so
those only got a grammar/diacritics pass, not a source-matching one --
their translation quality still rests on LLM instinct alone.

Rebuilt all three configurations (Release/Debug/Analyze) after the
edit -- compiled clean, `Localization.cpp` re-verified as valid UTF-8
decoding to the expected corrected strings (spot-checked
`STEHEN`/`ХВАТИТ` present in the actual table rows, `HALTEN`/`СТОП`
only remaining in the explanatory comments documenting what was wrong
before). Not yet re-tested live in-game.

## Session 17 -- Debug-only injection tracing, card counting leftovers deleted

**Injection tracing.** A user bug report claimed a 100% load-screen crash
(ntdll access violation, no `BlackjackCheat.log` ever created) on a
non-standard setup (a third-party "ScriptHookRDR2 V2" in place of
Alexander Blade's, plus LML). No log at all means the crash, if it's ours,
happens before `Config::Reload()`'s first `Log::Write` in `DllMain`. Added
`Log::Trace` (Debug-only, empty in Release) and traced every injection
step: `DllMain` attach/detach context (load type, pid/tid, asi/exe paths,
working dir, whether ScriptHookRDR2.dll is loaded), each call in
`DllMain`, `Config`'s ini path/open/parse/write, the scrThread-pool AOB
scan (PE headers, match or no-match, RIP resolve), and `ScriptMain`
startup through the first tick. Each line also goes to
`OutputDebugStringA` before the file, so DebugView still shows the last
step reached if opening the log file is what crashes. Not yet run in-game.

**Card counting deleted.** `src/BlackjackCardCounting.h` and
`tests/BlackjackCardCountingTests.*` had been unused since Session 7, left
on disk only because the folder had no version control at the time. It
does now, so they are deleted (plus the stale `ClInclude` in
`BlackjackCheat.vcxproj`), recoverable from git history.

## Session 18 -- code review fixes

A full code review found, and this session fixed (see `docs/CHANGELOG.md`
[Unreleased] and each fix's own "Code-review" comment in the source):

- **Bet offset (`kSeatBetOffset` 4 -> 5), CONFIRMED LIVE** (see the
  live result below).
  `seat.f_4[h]` is a script array, and a YSC array's first word is its
  element count, so `f_4` itself is the size word (2) and `bet[h]` is at
  `f_4 + 1 + h`. Evidence: Session 9's raw dump table logged `f_4[0] = 2`
  for all three seats, including the human seat before it had confirmed a
  bet; and size + 2 bets = `f_4..f_6` ends right before the live-confirmed
  bet-lock flag at `f_7`. The old read made `canDouble` reduce to
  `bankroll >= 2`. **To confirm:** bet something other than $2, run
  F11 -> Probe Seat Hands, and check that `bet(f_4[0])` shows the real bet
  and `betArraySizeWord(f_4)` shows 2.
- **Turn gating relies on `seat.f_3` (static trace only).** Advice now
  requires every occupied lower seat to read `f_3 >= f_59` and my own
  `f_3 < f_59`. If `f_3` doesn't behave as traced, advice will never
  appear, which is easy to spot. The Debug panel now has a
  "Turn f_3/f_59" line showing each seat's values: expect `-1/1` while a
  seat waits (reset at round start), `0/1` while it acts, and `1/1` once
  it's done (`2/2` after a split). "Not done" is `f_3 < f_59`, the same
  test `func_1063` uses for the table's case-4 "next seat to play" scan
  (Session 5), so every seat the table plays -- naturals included --
  reaches `f_3 == f_59` before the dealer's turn.
- The insurance window is inferred from the deck cursor still sitting
  exactly at the end of the initial deal (2 cards per dealt seat + 2 for
  the dealer), and by my seat's `f_3` still reading -1: insurance is
  state 2 of the table state machine, before state 4 sets the acting
  seat's `f_3` from -1 to 0 (the first version of this check required
  `f_3 == 0` and so could never fire). If insurance ever fails to show
  at the real prompt, check the cursor at that moment with Probe Table
  Struct.
- `tests/BlackjackHandEvalTests.cpp`'s "hard 11 with 3 cards" case had
  been passing `count=3` with a 2-element array (out of bounds), which
  passed under MSVC by luck and failed under g++. Fixed with a real 3-card
  hand.
- Considered and rejected: preferring the fewest hits among winning
  candidates in `DetermineCheatAction()`. See `BlackjackDeckSim.h`'s
  header comment (code-review addendum, item 2).

**Live result (after the fixes).** ProbeSeatHands with a $250 bet:
`seat 1 hand 0 ... bet(f_4[0], slot 863)=250 betArraySizeWord(f_4)=2`
(NPC seat 0: bet 4, size word 2). The bet offset is confirmed. The same
log had `mySeat (ped-array)=-1, f_9=1` with the human at seat 1: `f_9`
is right and `FindMySeatByPed()` is wrong (unknown why -- stale ped
array offset or handle encoding). `DrawOverlay()` already reads `f_9`
first and only falls back to the ped array when `f_9` is out of range,
so advice picked the right seat; the Probe labels now say which one to
trust.

## Session 19 -- AI seat model, no double after split, round-log replay

The first round log (`BlackjackCheat_rounds.jsonl`, 5 rounds, me at
seat 0 with AI seats after me every round) showed the deck engine
trusted in only 2 of 5 rounds: every decision line had
`isLastBeforeDealer:false`, so whenever the dealer still had to draw,
play advice fell back to basic strategy and betting advice to the
textbook estimate. Round 5 (18 vs 8,6) was a sure loss shown as Medium.

- **AI seats modeled (`BlackjackDeckSim.h`).** `func_1002` -> `func_623`
  is now ported: `func_623`'s table (dealer up card 2..14 x hand total,
  plus the pair branch) was transcribed with a script into
  `kAiHardTable`/`kAiPairTable`, with `func_1002`'s overrides (Aces always
  split; Double drops to Hit on 3+ cards, a short bankroll, or after a
  split). `SeatsAfter` replaces the `isLastSeatBeforeDealer` bool: the
  seats still to act after my hand are played before the dealer draws.
  Bool overloads remain for old tests/fixtures. Pre-deal betting now
  plays every seat (before and after mine) off the deck.
- **The AI keys on the dealer's VISIBLE card (`dealerRanks[1]`)**, even
  though `func_1002` reads `Table.f_2[0]`. Rounds 4 and 5 only replay
  that way (round 5: seat 1 stood on 16, which the table only does vs a
  6, the up card; the hole card was an 8). Likely the dealer-hand copy
  the mod reads and `Table.f_2` differ in order -- not chased further.
- **No double after split.** The Double prompt (`func_600`) needs
  `func_998`, which requires `f_59 == 1`; `func_1237` alone would allow
  it, which is where the old assumption came from. `CanDouble()`/
  `CanSplit()` mirror `func_998`/`func_997`, and split hands inside
  `EvaluateSplit()` never double. User report: Double advised after a
  split. Two tests that asserted doubling within split hands were
  corrected.
- **Round log `dealerPredicted`** is now `ReplayDealer()`: deal the deck,
  AI seats by the model, my seat as the cards it actually drew
  (`myCardsDrawn`), then the dealer. The old value was the deal-time
  "nobody draws" snapshot and mismatched whenever anyone hit. All 5
  logged rounds replay to the real dealer hand (fixture `expectDealer`).
  Decision lines now log `seatsAfter*` instead of `isLastBeforeDealer`.
- `liveLastDealer` never shows the dealer's draws. That's expected: the
  draw-out happens in the same tick as the live reset.

**To confirm live:** play rounds with AI seats and check every round
line has `"dealerPredictionMatch":true`. A split by an AI seat hasn't
been seen yet: the split dealing order is assumed to match mine.
