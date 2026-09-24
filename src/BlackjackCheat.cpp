/*
	Blackjack advisor module for the "bjack_sp" single-player minigame.

	STATUS: as of Session 6, kTableFieldOffset/kSeatHandsOffset/
	kHandCountOffset/kHandValueOffset are LIVE-CONFIRMED (see Session 6
	addendum below for the full before/after-diff methodology and
	evidence) -- the first constants in this file to graduate out of
	static-trace-only. Everything else below the "Struct layout" section
	is still a STATIC TRACE ONLY -- derived purely by reading the
	decompiled script
	(D:\Backup\Stuff\RDR2 Shit\Scripts\rdr2-scripts-decompiled\1491.50\script_rel\bjack_sp.ysc.c),
	the same way PokerCheat's struct layout was first derived, and not yet
	checked against a running game. Every constant not explicitly marked
	"CONFIRMED LIVE" in its own comment is still a plausible candidate,
	not a fact -- the Session 6 corrections are proof this genuinely
	matters: the ORIGINAL static trace for these same four constants was
	wrong (off by one word on the table offset, two words on the seat
	hand offset, one word each on count/value), exactly the kind of error
	PokerCheat's own JOURNAL.md documents needing many rounds of live
	probing to catch.

	Card encoding: assumed identical to poker_sp's own (rank 2-14, 11=J,
	12=Q, 13=K, 14=A; suit 0-3) -- bjack_sp's own deck-init function
	(func_458, line ~18168) builds its 52-card array with the exact same
	nested-loop shape as poker_sp's func_589 (suit 0..3 outer, rank 2..14
	inner, `num` holding rank directly and `num.f_1` holding suit), so the
	{rank, suit} 2-field layout itself is a solid structural match, not a
	guess. The specific suit-int-to-real-suit mapping (0=Hearts, 1=Diamonds,
	2=Spades, 3=Clubs) is carried over from poker_sp's own confirmed finding
	on the assumption both minigames share the same underlying card-texture
	convention (func_697, line ~25174, buds into a "card_set_N" texture
	dictionary here too, same as poker's func_925) -- NOT independently
	confirmed for blackjack specifically. Doesn't affect hand scoring
	either way (BlackjackHandEval.h only ever looks at rank).

	Rank->blackjack-value mapping (2-10 face, J/Q/K=10, A=11) is no longer
	just an assumption borrowed from a generic chart: bjack_sp's own
	func_1010 (line ~33066) is an exact case-by-case match of
	BlackjackHandEval.h's CardValue(), and its own hand-total function
	func_645 (line ~23356: sum values, count Aces, `while (total > 21 &&
	aces > 0) { total -= 10; aces--; }`) is a byte-for-byte structural
	match of EvaluateHand()'s ace-demotion loop. See docs/JOURNAL.md
	Session 2 for the full citation trail and what this let us derive
	about the dealer's soft-17 behavior and the real double/split rules
	(BlackjackHandEval.h's own header comment carries the rule-level
	findings; not repeated here since none of it changes a struct offset).

	Struct layout (all slots relative to bjack_sp's own uLocal_14, absolute
	local slot 14; all line numbers refer to the decompile above):
	  - uScriptParam_0 (LaunchArgs) at absolute slot 3624 -- counted the
	    same way PokerCheat's kLaunchArgsSlot was: last declared local is
	    uLocal_3623 (3624 slots, 0-3623), uScriptParam_0 immediately
	    follows. HIGH confidence (pure counting, not inference), but the
	    field(s) inside LaunchArgs worth cross-checking (a stakes-tier hash
	    the way poker's kKnownStakesHashes worked) haven't been identified
	    for blackjack -- ProbeTableStruct() doesn't attempt that check here.
	  - main() (line 3662) calls func_3(&(uLocal_14.f_3497), &uScriptParam_0,
	    ...) -- f_3497 is the generic scene-sequencer framework struct
	    (same role as poker's uLocal_14.f_1), NOT the game-state struct.
	    Not used by this file at all.
	  - Table = uLocal_14 ITSELF, not a nested sub-struct the way poker's
	    Table lived under uLocal_14.f_114.f_287. Traced via func_85(&uLocal_14)
	    (line 3961, called directly from func_4) -- func_85's own body
	    (line 4983) does `*uParam0 = 0; ... uParam0->f_9 = -1;
	    func_218(&(uParam0->f_17)); ... uParam0->f_1532 = 0; ...
	    func_222(&(uParam0->f_756));` i.e. resets fields directly on its
	    uParam0, and every one of those field names (f_9, f_17, f_1532)
	    independently matches a DIRECT `uLocal_14.f_N` use found elsewhere
	    in the file (e.g. line 4255 `uLocal_14.f_9`, line 4229
	    `uLocal_14.f_1532`) -- so func_85's uParam0 IS uLocal_14, confirming
	    Table's fields hang directly off uLocal_14 with no wrapper offset.
	    MEDIUM-HIGH confidence (same call-site-tracing method that nailed
	    poker's Table location, but only traced once here, not
	    cross-checked a second way).
	  - Table.f_756 = the actual per-round game-state struct passed to
	    func_222 (line 5048, `func_222(&(uParam0->f_756))`, called from
	    within func_85) -- func_222's own body (line 7605) is a Table-reset
	    function: zeroes 4-element seat-shaped arrays, resets a deck via
	    func_458+func_933, etc. Also calls
	    `MINIGAME::_0x6480723D3BE535B6(-1150372370)` -- the SAME constant
	    (-1150372370) that appears in poker_sp's kKnownStakesHashes, which
	    is a real (if indirect) cross-game confirmation that this is the
	    right kind of function (a per-round minigame-table reset) and that
	    the addressing model (flat slot arithmetic, no pointer indirection)
	    still applies to this build. Originally computed as kTableSlot =
	    14 + 756 = 770 -- Session 6 found this off by one word via live
	    memory (see Session 6 addendum below): the CONFIRMED value is
	    kTableSlot = 14 + 757 = 771.
	  - Table.f_2 = the DEALER's hand struct (see "Hand struct" below).
	    Traced via func_1236 (line 40750, `func_1236(uParam0, uParam1)`
	    just does `func_1188(&(uParam0->f_2)); func_1241(&(uParam0->f_2),
	    uParam1);`) called once per round from the same dealing function
	    that deals every SEAT's hand in a loop first (line 35268-35286) --
	    the one deal call OUTSIDE that seat loop is the dealer's own,
	    landing on f_2. Independently corroborated by the dealer's own
	    stand-on-17 loop (line 26058: `while (uParam0->f_2.f_24 < 17)`,
	    `.f_24` being the hand-total field -- see below) and by simple
	    struct-size arithmetic: a 25-word hand struct (below) starting at
	    offset 2 ends at offset 27, exactly where the seats array (f_27,
	    next paragraph) begins. MEDIUM-HIGH confidence.
	  - Table.f_27 = seats array, 4 seats (0-3), 60 words/seat. Occupancy
	    marker at seat_base+0, `!= -1` means occupied -- confirmed via
	    func_116 (line 5789: `uParam0->f_27[iParam1 (stride 60)] != -1`), same
	    "-1 = empty" convention poker used. HIGH confidence as of Session
	    2: the seat COUNT (4) is no longer just inferred from `for (i=0;
	    i<4; i++)` loops -- func_280 (line 12838:
	    `if (iParam1 >= 0 && iParam1 < 4 && ...)`) is a direct bounds
	    check on the seat index against the literal 4, the same kind of
	    hard evidence poker's own kSeatsHeaderOffset had (a real read of
	    the array's actual capacity, not just an incidental loop count).
	  - Per seat: f_8 = up to kMaxHandsPerSeat hand structs (25 words each,
	    for split hands -- blackjack lets one hand become two), f_59 = how
	    many of those are currently in use. Traced via func_1235 (line
	    40739: reads `uParam0->f_27[iParam1].f_59` as the next free hand
	    index, increments it, then resets+fills
	    `uParam0->f_27[iParam1].f_8[num (stride 25)]`) -- this is the actual
	    "deal a new hand into this seat" function. kMaxHandsPerSeat=2 is
	    now HIGH confidence as of Session 2, upgraded from an inferred cap
	    to a directly-read legality gate: func_1237 (the actual
	    hit/double/split/stand action-legality check, line ~40757, case 6
	    = split) returns false outright when `uParam1->f_59 > 1` (line
	    ~40785) -- i.e. the game itself refuses to split a hand that's
	    already been split once, a real enforced cap of exactly 2 hands
	    per seat, not just "every f_8 access this file happened to see
	    stayed under 2". Offset 8 was ORIGINALLY reasoned to be HIGH
	    confidence via 8 + 25*2 = 58, one spare word, then f_59 at 59 --
	    Session 6 found the real per-seat hands offset is 10, not 8 (see
	    Session 6 addendum), which breaks this specific corroboration:
	    with hands starting at 10, slot 59 lands inside hand[1]'s own
	    trailing spare word (10 + 25 + 24 = 59), not past both hands.
	    seat.f_59 (kSeatHandCountOffset) itself was NOT re-verified this
	    session -- demoted from HIGH to MEDIUM pending a live check; it
	    may need its own correction, or may simply be a coincidental
	    overlap that still happens to read correctly (an unused/always-
	    zero spare word colliding with a real field's expected value).
	  - Hand struct (25 words, used for BOTH Table.f_2 and each
	    seat.f_10[h] -- CORRECTED from seat.f_8[h], see Session 6
	    addendum): cards at hand_base+0..+20 (11 slots, 2 words each,
	    {rank,suit} -- NO leading header word, unlike poker's board/seat
	    arrays; confirmed no-header via func_1072 at line 35512 indexing
	    `uParam1->[uParam1->f_23 (stride 2)]` starting from count=0 i.e. offset
	    0 directly), count at hand_base+22 and total VALUE at hand_base+23
	    -- CORRECTED from the originally-traced +23/+24 (Session 6,
	    live-confirmed against all 4 real seats simultaneously, see
	    addendum below). Static trace for +23/+24 originally cited
	    func_1072's own count-field usage, func_1188's reset
	    (`uParam0->f_23 = 0`, line 39781), and func_1069's bust check
	    (`uParam0->f_24 > 21`, line 35494) -- these f_23/f_24 NAMES in the
	    decompile are still real fields, just apparently not at the flat
	    hand_base+23/+24 slots this file assumed (a nested-offset
	    subtlety the flat f_N=offset+N convention doesn't capture cleanly
	    here); the live-confirmed +22/+23 are what this file actually
	    reads now.
	    UNKNOWN whether f_24 is a plain best-total int (what this file
	    assumes, and recomputes independently via BlackjackHandEval.h
	    anyway rather than trusting) or something else pre-adjusted for a
	    "soft" flag -- doesn't matter for THIS file since hand totals are
	    computed fresh from the raw cards, never read out of f_24 directly.
	  - Deck = Table.f_592 (`func_458(&(uParam0->f_592))`,
	    `func_933(&(uParam0->f_592))`, both called from func_222/func_459 on
	    the same Table struct) -- 52-card array with NO header word (same
	    as the hand struct, confirmed via func_458's own build loop writing
	    directly to `uParam0->[num2 (stride 2)]` from index 0), cursor at
	    deck_base+104 (f_104), count at deck_base+105 (f_105) -- CORRECTED
	    from the originally-traced +105/+106 (Session 7 addendum, see
	    below): the cards occupy offsets 0-103 with NO spare word before
	    cursor, not one as originally guessed. Shuffle
	    (func_933, line 31274) is a byte-for-byte match of poker_sp's own
	    5-pass Fisher-Yates (func_1195) -- same structure, same
	    GET_RANDOM_INT_IN_RANGE(0, count) swap, same 5 outer passes.
	    HIGH confidence on the offsets (multiple independent call sites);
	    same "fully deterministic once shuffled" property poker_sp had
	    (the shoe is shuffled once, dealt sequentially by cursor, no burn
	    cards, no per-draw reshuffling) SHOULD carry over structurally, but
	    has NOT been independently re-confirmed here the way poker's
	    equivalent claim eventually needed a real deck-mismatch bug hunt
	    to nail down (see PokerCheat's docs/JOURNAL.md, "Root cause found:
	    the real deck lives on Candidate B, not Candidate A" -- there is no
	    "Candidate B"-style second copy found for blackjack's deck in this
	    trace, but that absence itself hasn't been verified either, only
	    not-yet-found). This file does not currently read/predict future
	    deck cards at all (first-pass scope is advisor-only on ALREADY
	    dealt cards) -- kDeckSlot/kDeckCursorOffset/kDeckCountOffset are
	    logged by ProbeTableStruct() for future use, not read by OnTick().
	  - "Your seat" candidate: uLocal_14.f_9 (absolute slot 14+9=23),
	    reset to -1 by func_85 (`uParam0->f_9 = -1`) -- same reset-to-(-1)
	    convention poker's kF114SeatIndexSlot used for its own "my seat"
	    field. MEDIUM confidence as of Session 2 (upgraded from
	    LOW-MEDIUM): two more plausible-but-not-conclusive corroborating
	    sites found -- line 4255-4256 feeds `uLocal_14.f_9` through
	    func_113/func_114/func_115 in what looks like a camera-focus/ped
	    lookup (consistent with "point the camera at MY seat"), and line
	    20114 gates a per-seat stat lookup (`f_568[uParam0->f_9].f_6`) on
	    it. Neither is poker's kind of smoking-gun (a predicate function
	    that independently tests the field against a second, differently-
	    derived value) -- caution: f_9 is heavily overloaded across
	    UNRELATED structs throughout this file (UI widget handles, task
	    data, etc. all also happen to use field index 9), so any new
	    "f_9" call site found later must be checked for which struct it's
	    actually operating on before treating it as corroboration.

	Session 2 addendum -- seat.f_1 (bankroll) and seat.f_4[handIndex] (bet
	amount per hand): traced via func_1237's own double/split funds gate
	(line ~40775: `if (uParam1->f_1 < uParam1->f_4[num]) return false`,
	`num` being the seat's current hand index) -- MEDIUM confidence (a
	real, direct read of both fields together in a live legality check,
	but only one call site, and neither field's absolute meaning -- which
	is bankroll vs. which is bet -- independently cross-checked a second
	way). Not read/used by OnTick() or any Probe* function yet -- logged
	now by ProbeSeatHands() (Session 2) purely so a future live session
	can sanity-check them (a bankroll value should look like a plausible
	dollar amount) alongside everything else, still no bet/bankroll UI or
	advice built on top of them. Also found seat.f_2 = insurance bet, paid
	out 2:1 (`seat.f_1 += 2 * seat.f_2; seat.f_2 = 0`, line ~26052) when
	the dealer's up card is an Ace and the dealer has blackjack.

	Session 3 additions:

	- The deck is a SINGLE 52-card deck, rebuilt+reshuffled every round,
	  NOT a 4-8 deck shoe -- func_458 (line ~18168) is an unambiguous
	  4-suit x 13-rank nested loop (`for (i=0;i<4;i++) for (j=2;j<15;j++)
	  ...`, `f_106 = num2` always lands on 52), and func_718 case 0 (line
	  ~25823-25827) calls func_459 -- which rebuilds AND reshuffles that
	  same array via func_458+func_933 -- at the start of every single
	  round. HIGH confidence (exact, unambiguous loop bounds; a real call
	  site tying the rebuild to every round, not just game startup). This
	  made card counting near-worthless here (since removed, see the
	  Session 7 FIFTH addendum below) and means BlackjackHandEval.h's basic-strategy chart, which
	  still assumes a 4-8 deck shoe, has NOT been corrected for
	  single-deck play -- a known, flagged gap, not a silent assumption.
	- A NEW, more reliable candidate for "my seat" than uLocal_14.f_9:
	  uLocal_14.f_1724 is a SIBLING struct to Table (like poker_sp's own
	  f_3310 "scene" struct -- a separate camera/ped/animation layer, not
	  the core game-logic Table at f_756), traced via func_44's teardown
	  code (line ~4267-4306, which passes `&(uLocal_14.f_1724)` to
	  func_117/func_118/func_121/func_122/func_123/func_125, all
	  consistently seat-indexed 0-3) and func_236/func_239 (lines
	  ~9656-9789, a per-seat ped animation-task state machine that
	  likewise threads `&(uParam0->f_1724)` through the same helper
	  functions). Its f_946[seat] (stride 46) is the seat's live Ped
	  HANDLE -- func_117 (line 5797: `uParam0->f_946[iParam1] != 0`) and
	  func_118 (line 5806: `return uParam0->f_946[iParam1];`) both use the
	  array element directly as a scalar Ped value with no further field
	  access, the exact same "array[i] alone means offset+0 of the
	  stride" convention already confirmed for Table.f_27's own occupancy
	  marker (func_116, line 5791) -- so f_946[seat]+0 is the ped handle.
	  kPedSceneFieldOffset=1724/kSeatPedArrayOffset=946/kSeatPedStride=46:
	  HIGH confidence on the offsets themselves (multiple independent call
	  sites, consistent seat indexing, consistent scalar-access
	  convention). This lets mySeat be determined WITHOUT trusting the
	  ambiguous f_9 field at all: read each seat's ped handle and compare
	  it against PLAYER::PLAYER_PED_ID() via a real native call (see
	  FindMySeatByPed() below) -- a fundamentally more reliable mechanism
	  than a bare scalar-field guess, since it only depends on confirming
	  f_946 holds a real ped handle (which the game's OWN code already
	  treats it as, feeding it straight into PED:: natives at line ~4280
	  and ~9679), not on guessing what an opaque int means. kMySeatSlot
	  (f_9) is kept as a SECONDARY logged candidate for comparison only --
	  its confidence is unchanged at MEDIUM (still no smoking-gun second
	  independent site; new sites found this session were themselves on
	  OTHER unrelated structs, e.g. a per-seat ped-task struct storing its
	  OWN seat index at its own f_9, reinforcing rather than resolving the
	  known field-index-9 overloading problem already flagged in Session 2).
	- Split Aces DO get the standard "one card each, then forced stand"
	  restriction after all -- see BlackjackHandEval.h's own header
	  comment for the full f_699 trace; this REVERSES Session 2's "no
	  ace-specific restriction found" conclusion. GetBasicStrategyAction()
	  now takes an explicit isSplitAceHand flag; DrawOverlay() below
	  detects it (a seat with 2 hands whose hand's first card is an Ace --
	  see that call site's own comment for why that's a valid signal).

	Session 4 addition -- deterministic deck-ahead prediction:

	The user pointed out (correctly) that since this mod already traces the
	deck/shoe struct offsets, it should read the deck directly for
	deterministic predicted future cards the way PokerCheat's
	BuildPredictedBoard() does, rather than only ever showing already-dealt
	cards. This is reinforced by Session 3's single-deck finding: with the
	WHOLE round dealt from one fixed, already-shuffled 52-card deck, the
	dealer's hole card is already-dealt real data (just not shown on
	screen) and the dealer's own forced stand-on-17 draw-out is simulatable
	ahead of time straight off the deck array. See SimulateDealerOutcome()/
	PredictedHand/UpdateDeckPrediction()/DrawDealerHoleCardStatus()/
	ProbeDeckPrediction() below -- this is now the PRIMARY feature (Release
	+Debug), with card counting demoted to secondary/Debug-only (later
	removed entirely, see the Session 7 FIFTH addendum below), since direct reading is strictly better
	information than estimating from a count once the deck itself is
	readable. UpdateDeckPrediction() also self-validates the dealer
	draw-out prediction automatically every round via a "PredictionCheck"
	log line (Debug-only), the same technique PokerCheat's own predicted
	board used -- this is the most valuable live-testing signal for
	confirming kDeckSlot/kDeckCursorOffset are actually right, and needs no
	F11 interaction at all, just normal play with the Debug build running.

	Session 5 addition -- turn order traced, NPC seats confirmed real AND
	deterministic, kMySeatSlot upgraded, deck-ahead prediction made
	self-correcting:

	- Turn order is CONFIRMED strictly ascending, one seat fully resolved
	  before the next starts, dealer strictly last. func_718 (the Table's
	  own round-phase state machine, line 25809) case 4 (line 25947) scans
	  `for (i=0;i<4;i++)` and picks the first seat where func_1063 (line
	  35344: `return func_280(uParam0,iParam1) && uParam0->f_27[iParam1].f_3
	  < uParam0->f_27[iParam1].f_59;`) is true -- i.e. the first OCCUPIED
	  seat that still has unresolved hands (f_3, its "current hand index",
	  hasn't caught up to f_59, its hand count). That seat plays out
	  entirely (cases 5/6/7, including the f_699 forced multi-hand walk --
	  see BlackjackHandEval.h's own header comment) before case 4 runs
	  again and picks the NEXT such seat. Once no seat qualifies (num2==-1),
	  case 4 transitions straight to case 8 -- the dealer's own draw-out
	  loop (`while (f_2.f_24 < 17) hit`) already relied on by
	  SimulateDealerOutcome(). So: seat 0, then 1, then 2, then 3 (skipping
	  unoccupied/already-done ones), then the dealer, strictly in that
	  order, sharing the one deck cursor the whole way -- HIGH confidence
	  (the scan order and the termination condition are both unambiguous).
	- NPC (non-player) seats are REAL -- any occupied seat plays, not just
	  the player's -- AND their actions are FULLY DETERMINISTIC, no
	  randomness in the decision itself. Traced end to end: the per-seat
	  "get this seat's queued action" consumer (func_1065/func_1066, line
	  35356/35374) branches on `iParam1 == uParam0->f_582` -- a player-seat
	  action QUEUE (f_570/f_579, push/pop semantics) vs. a single-slot
	  per-seat mailbox (f_561[seat]) for every other seat. The PRODUCER
	  side that fills whichever one applies is func_1077 (line 35593),
	  reached (for the seat currently up, `*uParam0`/Table+0, a "current
	  turn seat" tracker case 4 writes and cases 5-7 read back -- informal,
	  not added as a read field since nothing below needs it) via a UI
	  state machine (func_227-shaped, case 19 around line 8744) that
	  branches on func_597 (line 20684: `return uParam0->f_9 == uParam1;`
	  -- see kMySeatSlot below) -- real player seat: read actual pad/button
	  input (func_600); any OTHER seat: call func_602 -> func_1002 (line
	  32949), a pure function of the hand's total (f_24) and the dealer's
	  up card RANK (Table.f_2[0], no suit/soft info at all) that looks up
	  an action via func_623 (line 21123, a ~1860-line fully-unrolled
	  table keyed on dealer-upcard-rank x hand-total, split into a
	  pair-decision branch when the hand is exactly 2 cards of matching
	  rank and a hit/stand/double branch otherwise), with two hardcoded
	  overrides: a pair of Aces (rank 14+14) always returns Split
	  regardless of the table, and a table result of Double gets demoted
	  to Hit when the hand already has more than 2 cards, funds are
	  short, or the seat has already split once (`f_59 > 1` -- the AI
	  chooses not to double post-split even though func_1237 itself would
	  legally allow it, per Session 2 -- an AI policy choice, not a game
	  rule). A handful of spot-checked entries (pair totals 4/6/8/10 vs.
	  dealer 2/3/4 all matched known-correct never/always-split
	  conventions) look like genuine, sane strategy, not junk data. The
	  per-seat random 1-6s "thinking delay" timer (f_1711[seat],
	  MISC::GET_RANDOM_FLOAT_IN_RANGE) gates only WHEN an NPC's queued
	  decision gets submitted, never WHAT it decides -- the decision
	  itself is 100% reproducible from (total, dealer up rank, card count,
	  hands-in-use) alone. HIGH confidence on the mechanism (multiple
	  independent, clean call sites); the full func_623 table itself was
	  spot-checked, not exhaustively transcribed (~1860 lines) -- not
	  needed, see the self-correcting fix below.
	- Because of the above, `SimulateDealerOutcome()`/`UpdateDeckPrediction()`
	  no longer snapshot ONCE at round start (which only ever produced a
	  "what if nobody else draws" guess) -- they now RE-SIMULATE from the
	  LIVE deck cursor every tick the dealer has cards. This is
	  self-correcting without needing to replicate func_623 at all: the
	  cursor only ever advances as real draws happen, so re-running "what
	  would the dealer draw starting from the CURRENT cursor" every tick
	  means the last simulation computed right before the dealer's real
	  turn begins is automatically exact -- by then every occupied seat
	  ahead of the dealer has already finished consuming its share of the
	  cursor for real, and nothing simulated is left to guess at. No
	  explicit "wait until all seats are done" trigger needed, and no risk
	  of a subtly-wrong hand-written port of the AI's own table.
	- `kMySeatSlot` (`uLocal_14.f_9`): MEDIUM -> HIGH. func_597 (line
	  20684, quoted above) is exactly the kind of independent, second-
	  derivation smoking gun PokerCheat's own confirmed mySeat field had
	  and this project's f_9 candidate was previously missing -- the game
	  itself uses `f_9 == seat` specifically to decide "is this the real
	  human player" for the single highest-stakes purpose there is (whether
	  to wait for real button input or auto-play a seat). Still a STATIC
	  trace, not a live memory read -- kept at HIGH (static-trace) per this
	  project's confidence-rating discipline, not promoted to "confirmed".
	  `FindMySeatByPed()` (Session 3, ped-handle matching) remains the
	  PRIMARY method in code since it doesn't depend on f_9 at all, but the
	  two candidates now have real, comparable justification instead of
	  one solid and one merely-plausible.
	- New field: seat.f_3, "current hand index" -- how far this seat has
	  progressed through its own (possibly-split, up to kMaxHandsPerSeat)
	  hands, compared against f_59 by func_1063 (quoted above) to decide
	  if the seat still has unresolved hands. HIGH confidence purely by
	  the established f_N=offset+N convention already load-bearing
	  throughout this exact struct (f_0/f_1/f_2/f_4/f_8/f_59 all matched
	  this pattern already) -- not consumed by OnTick() (the self-
	  correcting prediction fix above doesn't need it), logged by
	  ProbeSeatHands() as a new sanity-check field only.
	- New rule found, NOT yet incorporated into BlackjackHandEval.h: a
	  "7-card Charlie" auto-win. The per-hand turn-resolution loop (case 7,
	  line 26027) treats `uParam0->f_27[num3].f_8[j].f_23 >= 7` (7+ cards)
	  the same as an already-made 21 -- both trigger func_1062's payout
	  path (a real win, 2.5x for the natural-blackjack branch, otherwise
	  2x) rather than continuing to prompt for hit/stand/double. The
	  dealer's own draw-out (case 8's `while (f_2.f_24 < 17)` loop) has NO
	  equivalent card-count cap, so this does NOT affect
	  SimulateDealerOutcome() -- only a player/NPC hand can hit this.
	  Doesn't change GetBasicStrategyAction()'s hit/stand thresholds
	  either (it only ever recommends hitting when that's already the
	  right call on total alone) but IS a real edge case basic textbook
	  strategy doesn't cover -- e.g. hitting a 6-card hand specifically to
	  try to lock in an auto-win at 7 cards can occasionally be correct
	  even when standing on the current total would otherwise be strategy-
	  chart-optimal. Flagged as a known, explicit gap, not implemented
	  this session (would need genuine game-theoretic work, not a quick
	  patch).

	Session 6 addition -- FIRST LIVE MEMORY CONFIRMATION of this project
	(everything above this point was static-trace-only; this is the
	turning point, same milestone PokerCheat's own JOURNAL.md documents
	hitting many sessions in):

	- Built a generic diagnostic (GamePointers::DumpLocalStackJsonl(),
	  wired to a new F11 "Dump Full Stack JSONL" item) that dumps EVERY
	  script-local slot of bjack_sp's thread to a timestamped JSONL file
	  -- no theory about what any slot means, just every plausible
	  interpretation (i32/u32/i64/f32/hex) of its raw 8 bytes. Built
	  specifically because the original kTableFieldOffset=756 guess
	  turned out wrong (the dealer's hole card read as garbage in live
	  testing -- see below) and one-probe-at-a-time re-guessing wasn't
	  converging.
	- Took two dumps a few seconds apart around a real dealer hit (the
	  user watched the dealer draw a live-identified Queen of Diamonds)
	  and grepped both for the known real cards. Found the dealer's
	  cards at absolute slot 773 in BOTH dumps, correctly extended by
	  exactly the real drawn card between them (9H,4S -> 9H,4S,QD) --
	  ruling out coincidence (multiple OTHER slots also briefly looked
	  plausible from a single static snapshot but turned out to be
	  frozen/stale scratch copies that did NOT update between the two
	  dumps the way the real struct must).
	- 773 = 14 (uLocal_14) + kDealerHandOffset(2) + 757 -- i.e.
	  kTableFieldOffset is 757, not 756: a plain off-by-one in the
	  original static trace. kTableSlot is now 771, not 770.
	- Cross-checked against a SECOND, independent real-world fact: the
	  user's own 4-card hand (2S,4H,4C,AD, value 21) was live at the
	  table at the same moment. Using the corrected kTableSlot=771 and
	  the ALREADY-assumed kSeatsBase(27)/kSeatStride(60), all 4 seats'
	  hands were pulled from the dump at once and EVERY ONE produced a
	  card list whose sum matched its own logged "value" field exactly
	  (19=8+3+8, 18=2+9+7, 21=A+Q, 21=2+4+4+A) -- overwhelming
	  corroboration for kTableSlot=771 from a completely independent
	  direction (seats, not the dealer), not just one lucky offset.
	- Getting there needed two more corrections beyond kTableFieldOffset,
	  found by direct trial against this same live data:
	    - kSeatHandsOffset: 8 -> 10 (a seat's hand array actually starts
	      2 words later than originally traced; the dealer's own hand,
	      living directly on Table.f_2 rather than inside a seat struct,
	      did NOT need this correction -- cards there were already
	      right at offset+0, so this is specific to the per-seat path,
	      not a universal rule).
	    - kHandCountOffset: 23 -> 22, kHandValueOffset: 24 -> 23 (the
	      11-card array occupies offsets 0-21, so count/value naturally
	      follow at 22/23, not 23/24 -- the original trace had counted
	      one slot too many somewhere in the 11*2=22-word card region).
	  All three corrections were required together and cross-validated
	  against 4 independent real seats simultaneously -- not a single
	  lucky match.
	- STILL OPEN: applying the same corrected kHandCountOffset(22)/
	  kHandValueOffset(23) to the dealer's own hand (base 773) read 0/0
	  in this one live sample, even though the dealer's CARDS at that
	  same base were confirmed correct twice. Either this particular
	  copy of the dealer hand is itself a stale/secondary copy for just
	  those two trailing fields (cards got copied correctly but
	  count/value didn't, which would be strange for a single struct),
	  or there's a timing/read-race explanation, or a dealer-specific
	  struct quirk not yet understood. Doesn't block anything currently
	  read by OnTick() (which recomputes hand totals itself from raw
	  cards via BlackjackHandEval.h rather than trusting f_22/f_23
	  directly, same "don't trust the field, recompute" policy already
	  in place before this session) but flagged here as a genuine open
	  question for the next live session, not silently ignored.
	- Methodology note for future sessions: DumpLocalStackJsonl's biggest
	  practical lesson was that a SINGLE dump is not enough to trust a
	  match on its own -- the stack is full of transient scratch copies
	  of real values (e.g. function arguments to a card-texture-drawing
	  routine) that coincidentally look exactly like real hand data from
	  one snapshot alone. A before/after DIFF across a known, real state
	  change (here: a dealer hit) is what actually separates the one
	  persistent struct from its many short-lived look-alikes.

	Session 7 addition -- three real, user-caught bugs fixed from actual
	live play (not new tracing -- the user just played normally and
	reported what looked wrong):

	- ReadHand() no longer trusts hand.f_22 (kHandCountOffset) to decide
	  how many cards to read -- it scans the raw card array itself for
	  the first invalid/padding entry (rank outside 2-14) instead. Cause:
	  a live probe showed the dealer's OWN hand had 5 real cards sitting
	  in memory while its f_22/f_23 both read 0 (the Session 6 "STILL
	  OPEN" anomaly) -- since dealerHasCards/haveAdvice/the hole-card
	  line/the new next-card line were all gated on hand.count being
	  nonzero, this silently blanked out most of the HUD whenever it hit.
	  The card array's -1 padding convention was already independently
	  confirmed structurally (Session 6), so scanning for it is not a new
	  assumption, just a more robust way to use an already-trusted fact.
	- mySeat priority FLIPPED: f_9 is now tried first, with
	  FindMySeatByPed() (the old PRIMARY) as a fallback only if f_9 is
	  out of range. Cause: a live probe showed FindMySeatByPed()
	  returning -1 (no ped match at all) while f_9 read 3 -- and seat 3
	  is independently corroborated by Session 6's own differential-dump
	  find (the user's real hand that session was at seat 3). f_9 was
	  never wrong here, just distrusted for lack of a second derivation;
	  it now has one (agreement with an unrelated session's independent
	  finding). kPedSceneSlot/kSeatPedArrayOffset/kSeatPedStride remain
	  unconfirmed and clearly a real problem (a -1 result live, not just
	  static-trace uncertainty) -- worth its own re-derivation later, not
	  attempted this session.
	- **Dealt-card order is [hole, up], not [up, hole]** -- the user
	  directly observed on screen that the card this file was calling
	  "Dealer hole" was actually showing face-up, meaning ranks[0] is the
	  hidden card and ranks[1] is the real up card, backwards from every
	  prior session's assumption. This was NOT just a display bug: it
	  meant GetBasicStrategyAction() was being fed the HIDDEN card as the
	  dealer's up-card rank the entire time (Sessions 1-6), a genuine
	  correctness bug in the advice engine itself, not merely a label.
	  Also wrong: the insurance trigger (checked ranks[0]==14 instead of
	  the real up card) and UpdateCardCounting()'s "count the up card
	  immediately, defer the rest" logic (was counting the actual hidden
	  card immediately and deferring the real up card -- backwards for a
	  count meant to mirror what a real player could legitimately see).
	  All four call sites fixed: DrawDealerHoleCardStatus() now reads
	  ranks[0]/suits[0], GetBasicStrategyAction()'s dealer-upcard
	  parameter and the insurance check now read ranks[1],
	  UpdateCardCounting() now counts ranks[1] immediately and defers
	  index 0 plus any hits (2+) to round end. SimulateDealerOutcome()'s
	  own math was NEVER affected by this (it treats ranks[0..count-1] as
	  an undifferentiated set of "already known" cards for total/bust
	  purposes, with no up/hole distinction baked into the simulation
	  itself) -- only the labeling and the strategy/insurance/counting
	  call sites that specifically needed to know WHICH card is the real
	  up card were wrong.
	- None of these three are struct-offset errors -- Session 6's four
	  corrected offsets are untouched and still stand. These are bugs in
	  how this file's OWN code interpreted otherwise-correctly-read data.

	Session 7 addendum -- kDeckCursorOffset/kDeckCountOffset corrected
	(the actual root cause of "Next card (if you Hit)" never appearing,
	and of the garbled-suit predicted draws in early PredictionCheck log
	lines): BlackjackCheat.log showed ProbeTableStruct/ProbeDeckPrediction
	consistently reading an implausible deckCursor=52 alongside a garbage
	deckCount (-1 or 0) every single time, at both between-round and
	mid-round moments -- a real deck cursor should vary with game state
	and a real count should be the constant 52 (Session 3's single-deck
	finding), so the two fields were clearly swapped-and-shifted, not just
	occasionally wrong. Confirmed by scanning 3 real stack dumps taken
	this session/a prior one (BlackjackCheat_stackdump_baseline.jsonl and
	two more 2 seconds apart) for the 52-card {rank 2-14, suit 0-3}
	pattern: it landed at exactly kTableSlot+592 in all three (kDeckOffset
	itself needed NO correction), but the word right after the 52 cards
	(relative +104) was the one that actually CHANGED between dumps (0,
	0, 8 -- consistent with a live deal cursor), while relative +105 was a
	rock-steady 52 in all three (consistent with count), and +106 was
	garbage in every dump (past the end of the real struct). This is the
	same off-by-one shape as Session 6's kTableFieldOffset fix: the deck's
	52 cards occupy relative offsets 0-103 with NO spare word before the
	cursor, so cursor is at +104 and count at +105, not +105/+106 as
	originally statically traced. With the fix, DrawNextCardStatus()'s
	`deckCursor >= 0 && deckCursor < deckCount` guard (previously
	comparing a real count against garbage and failing almost every tick)
	and SimulateDealerOutcome()'s own cursor-driven draw-out loop both now
	read the real, live-varying cursor. Confirmed fixed by the second
	addendum below's live test, which also caught a second, bigger bug
	that was independently blocking "Next card" from ever appearing.

	Session 7 THIRD addendum -- ReadHand()'s raw-array-scan approach
	(introduced earlier in Session 7 to fix the dealer's f_22/f_23 reading
	0/0, see the file's original ReadHand() comment history) reverted:
	live testing right after the deck-offset fix above surfaced a real bug
	report -- the user's own hand showed as "JC QD [+something]" when
	their real hand was "8D 4D", and the dealer showed more cards than
	were actually on the table. A fresh stack dump
	(BlackjackCheat_stackdump_20260911_132438.jsonl) explained why: unused
	trailing card slots are NOT reliably -1-padded the way this file
	assumed -- they can hold fully self-consistent STALE data from a
	PREVIOUS deal (real cards AND matching f_22/f_23 fields, just old)
	that a "scan until invalid" approach cannot distinguish from a
	genuinely longer current hand. Concretely: the dealer's slot showed
	3D,7C,AH before real -1 padding, but f_22/f_23 read 2/10 -- exactly
	the real 3D+7C hand, with AH a stale leftover; the user's own seat's
	hand 0 showed 8D,4D,8H,KD before padding, but f_22/f_23 read 2/12 --
	exactly their real 8D+4D hand, with 8H/KD stale leftovers; and that
	same seat's hand 1 held a FULLY self-consistent old hand (JC,QD,AS,
	count=3, value=21 all agreeing with each other) even though the seat
	hadn't split this round, proving the whole per-hand struct just isn't
	cleared between hands/rounds. This directly disproves Session 6's
	"dealer f_22/f_23 read 0/0" finding as a permanent trait -- the SAME
	dump that caught the scan-based overrun also showed f_22/f_23 reading
	correctly for the dealer, so that earlier 0/0 sample is now believed
	to have been a one-off transient/timing read (it coincided with
	dealerHand.count also reading 0, i.e. a between-rounds reset moment).
	ReadHand() now trusts hand.f_22/f_23 directly again (clamped
	defensively), and the same reasoning applies one level up: a same-
	session detour that started trusting a raw scan of hand 1 instead of
	seat.f_59 to decide "did this seat split" was ALSO wrong for the exact
	same reason (a stale hand-1 struct scans as "real") and has been
	reverted back to trusting seat.f_59 directly -- which a live dump
	confirmed reads 1 (correct, matching the user's real single hand)
	right where the stale hand-1 data sat. Net effect: both the deck-
	offset fix and this reversion were needed together for "Next card"
	and correct hand values to actually work -- confirmed against the
	live report that triggered this addendum, not yet re-confirmed on a
	subsequent independent round.

	Session 7 FIFTH addendum -- card counting removed entirely, advice
	replaced with a fully deck-derived "pure cheat" engine, at the user's
	explicit direction ("remove this card counting crap, just have it do
	pure cheating -- all logic should be derived based on reading the
	cards"). BlackjackCardCounting.h is no longer included or called from
	this file: UpdateCardCounting() and its running-count state are
	deleted, the Debug-only "Count (secondary)" panel line and
	Config::Values::ShowCardCount toggle are gone, and ProbeTableStruct()
	no longer logs a count line. The now-unused header/tests project
	(BlackjackCardCounting.h, tests/BlackjackCardCountingTests.vcxproj)
	were initially left on disk, then deleted outright once the project
	was under git (user request) -- recoverable from git history if ever
	needed.
	  - Hit/Stand/Double advice (previously BlackjackHandEval::
	    GetBasicStrategyAction(), pure textbook probability, blind to the
	    actual next card by design) is replaced by DetermineCheatAction()
	    (BlackjackCheat.cpp, right after SimulateDealerOutcome()) -- see
	    that function's own header comment for the full mechanism and its
	    one open caveat (exact once this is the last seat left to act
	    before the dealer, a provisional guess otherwise, same as the
	    HUD's own "Predicted dealer draws" line already was). This was the
	    direct fix for the reported bug: standing on a hard 18 when the
	    known next card was a 3 (making 21).
	  - Split is the one action NOT yet deck-derived -- DetermineCheatAction()
	    still asks GetBasicStrategyAction() purely for its pair-chart
	    Split/no-Split call (textbook, not deck-simulated) before doing
	    its own thing for everything else. A genuinely deck-derived split
	    decision would need to simulate both resulting hands' own
	    draw-outs plus the dealer's, which is real additional work not
	    attempted this pass -- flagged as a known scope limit.
	  - Insurance is now a CERTAINTY, not a probability-based deviation:
	    the dealer's real hole card is already read every tick regardless
	    (DrawDealerHoleCardStatus() has shown it since Session 4), so
	    "will the dealer have blackjack" is directly answerable --
	    DrawOverlay() now recommends insurance iff
	    BlackjackHandEval::CardValue(dealerHand.ranks[0]) == 10, the exact
	    condition for dealer blackjack when the up card (ranks[1]) is an
	    Ace. No live re-confirmation yet on either the new advice engine
	    or the new insurance logic -- next session should watch a few
	    hands where hitting/standing/doubling against a known next card
	    is unambiguous (e.g. this bug report's own 18-vs-known-3 case) and
	    confirm the HUD now says Hit/Double correctly.

	Session 9 addition -- deck is fixed BEFORE the bet, and a new pre-bet
	deal prediction feature built on that fact:

	- The user asked directly: "when does the script determine what the
	  deck will be? After or before the bet?" Tracing func_718 (line
	  25809) answered it unambiguously: case 0 (the round-transition
	  state, entered the instant the PREVIOUS round's dealer draw-out
	  finishes -- case 8/9 both call func_1047(uParam0, 0)) calls func_459
	  (the rebuild+reshuffle from the Session 3 deck finding)
	  UNCONDITIONALLY on its very first tick, BEFORE the state machine
	  even starts waiting for the next round's bets. That wait is
	  func_1055/func_1056 (line 35235/35248) -- func_1056 specifically
	  requires EVERY occupied seat's seat.f_7 to be nonzero before state 0
	  can hand off to state 1 (func_1057, the real initial-deal function,
	  line 35261). So the entire round's deck order, dealt sequentially
	  off the same cursor everything else in this file already relies on,
	  is completely fixed in memory before a single bet is placed -- the
	  bet has ZERO causal influence on the shuffle. HIGH confidence
	  (unambiguous call order, exact same func_459/func_718 case 0 call
	  site the Session 3 deck finding already cited, just read for
	  ordering this time instead of contents).
	- New field found while tracing this: seat.f_7 (kSeatBetConfirmedOffset)
	  -- func_759 (line 27401) reads exactly `seat.f_7`, and it's the SAME
	  field both func_1056 (the state-0-exit gate above) and func_1057
	  (the real per-seat deal gate, alongside seat.f_4[0]/func_492) check.
	  HIGH confidence: a real field independently corroborated by being
	  load-bearing in two different real gating functions, not an
	  inferred/guessed offset.
	- SimulatePreDeal() (BlackjackCheat.cpp, right after FindMySeatByPed())
	  replays func_1057's own algorithm off the raw, not-yet-touched deck
	  array: for seat 0->1->2->3, any seat with BOTH f_7 and f_4[0]>0 gets
	  the next 2 undrawn deck cards, then the dealer gets the final 2 --
	  giving an exact preview of every hand before it's dealt, whenever
	  the deck is caught in its untouched post-reshuffle state (cursor==0,
	  count==kDeckSize==52) with dealerHand.count still 0. Wired into
	  DrawOverlay() as new "Predicted dealer (before deal)"/"Predicted
	  your hand (before deal)"/"Next after deal" Debug panel lines, plus
	  new Release+Debug card-face icons (DrawPredictedHandIcons(), reusing
	  the existing DrawDealerHoleCardIcon/DrawNextCardIcons screen slots
	  since the two states are mutually exclusive in time -- see that
	  function's own header comment).
	- Self-correcting the exact same way SimulateDealerOutcome() already
	  is (re-simulated from live state every tick, not a frozen snapshot):
	  early in the "waiting for bets" phase a seat's f_7/f_4[0] can still
	  change while that seat is mid-adjustment on its own bet slider, so
	  this is only a "if dealing happened right now" guess until then --
	  but func_1056 already REQUIRES every occupied seat to be confirmed
	  before state 0 can exit, so the LAST call made right before that
	  transition is exact by construction, not a guess, at the exact
	  moment it matters (immediately before func_1057 runs the real deal).
	Session 9 LIVE CONFIRMATION addendum (same session, immediately
	after): the user paused right at the bet prompt and took a raw
	DumpFullStackJsonl() dump (cursor=0, count=52 -- confirmed the exact
	"untouched deck" window this feature targets), then a second dump
	after betting and dealing. Manually replaying SimulatePreDeal()'s own
	algorithm against the FIRST dump's raw slots predicted: seat 0 (the
	human, mySeat via f_9 read 0 in both dumps) gets deck[0..1], seat 1
	(NPC, f_7 already 1 pre-human-confirm) gets deck[2..3], seat 2 was
	unoccupied (marker -1, skipped), seat 3 (NPC, f_7 also already 1)
	gets deck[4..5], and the dealer gets deck[6..7]. The SECOND dump's
	real dealt hands matched EVERY one of those exactly: seat 0 = 7H/9C
	(user-confirmed against the real screen), seat 1 = KH/4C, seat 3 =
	8C/5C, dealer = KD(hole)/2H(up) (also user-confirmed against the real
	screen) -- and the deck cursor read exactly 8 in the second dump (4
	pairs consumed), with the raw 52-card array itself byte-identical
	between both dumps. Also incidentally confirmed: the user's own
	bankroll dropped exactly 400->398 for a $2 bet (kSeatBankrollOffset/
	kSeatBetOffset both correct together), and kSeatBetConfirmedOffset
	(f_7) read 0->1 for the human seat specifically between the two
	dumps while staying 1 throughout for both already-confirmed NPC
	seats -- the exact func_1056 mechanism this field was traced from,
	caught live. Every offset this feature touches was correct on the
	FIRST live test, no corrections needed -- see docs/JOURNAL.md's
	Session 9 addendum for the full slot-by-slot evidence. STILL open:
	the two new Release+Debug icon positions (reusing HoleCardIconX/Y and
	NextCardIconBaseX/Y verbatim) were not visually confirmed this pass
	(the user read real screen cards and F11 dumps, not the on-screen
	icon layout) -- may still need the same kind of live Reload-Config
	retuning HoleCardIconX itself went through (0.821 -> 0.957).

	Session 9 SECOND live-testing addendum (same session, next round) --
	two real bugs found from actual play, both fixed, plus a new
	ShowCardsBeforeBet toggle (user request):

	- **The predicted own-hand icon never appeared at all** -- confirmed
	  the STILL-open item just above was in fact a real bug, not merely
	  unconfirmed. Root cause: SimulatePreDeal() required seat.f_7 (bet
	  CONFIRMED) for every seat including the human's own, matching
	  func_1057's real gate exactly -- but func_1056 also requires the
	  human's own f_7 before the table can leave state 0 to deal, so the
	  real window where "my own f_7 just flipped to 1 AND dealerHand.count
	  is still 0" both hold is at most one script tick, often zero visible
	  frames. The dealer's predicted icon showed fine because it doesn't
	  depend on mySeat at all -- the FIRST live addendum above already
	  showed both NPC seats confirm their bets before the human even sees
	  the bet prompt, so `preDeal.valid` was true for the entire time the
	  human was deciding. Fix: SimulatePreDeal() now takes an explicit
	  `mySeat` parameter and treats that one seat as "will play" the
	  moment its bet AMOUNT (f_4[0]) is nonzero, without waiting for f_7 --
	  live data already showed this field reads nonzero well before
	  confirming (the bet slider's current value). Every OTHER seat still
	  requires the real f_7, unchanged. This is a strictly more
	  provisional guess for mySeat specifically (could show a hand that
	  never gets dealt if the human backs their bet down to 0 without
	  confirming) -- accepted tradeoff, since showing an occasionally
	  premature preview during a window that otherwise showed NOTHING at
	  all is the point of ShowCardsBeforeBet.
	- **A real mismatch caught on the very next round**: predicted dealer
	  read 9S/10C, actual dealer ended up with 2D/5D. Root cause
	  understood, NOT fully solvable in general: unlike the dealer
	  draw-out prediction (SimulateDealerOutcome(), where the cursor only
	  ever advances forward, so self-correction is monotonic), THIS
	  prediction's seat-to-deck-index mapping can shift non-monotonically
	  any time another seat's bet confirms during the waiting phase --
	  every OTHER occupied seat (not just mine) could in principle confirm
	  right up against the same "last tick before dealing" edge the mySeat
	  fix above addresses, and this file has no way to distinguish "no
	  more seats are joining" from "one more seat is about to confirm"
	  ahead of time. Added ValidatePreDeal() (right after SimulatePreDeal())
	  as an instrument, not a fix -- the same "PredictionCheck every round,
	  no F11 needed" self-validation UpdateDeckPrediction() already does
	  for the dealer draw-out, logging a per-seat + dealer MATCH/MISMATCH
	  ("PreDealCheck" lines) every round from now on so future sessions can
	  characterize how often/why this actually happens instead of relying
	  on a user noticing by eye.
	- New Config::Values::ShowCardsBeforeBet toggle (Release+Debug,
	  default true) -- user request, so this specific (more provisional)
	  feature can be turned off independently of ShowDeckPrediction, which
	  now only ever governs the post-deal dealer-hole-card icon/"Next
	  cards" row. All of SimulatePreDeal()'s HUD output (both the Debug
	  panel text lines and the Release+Debug icons) now gates on this
	  instead.
	- Confirmed (re-read, not re-derived) that the actual on-screen
	  rendering technique here is architecturally identical to
	  PokerCheat's own opponent/community card icons
	  (DrawCommunityCardIcons()/DrawSeatCardIcons() in PokerCheat.cpp --
	  same DRAW_SPRITE + BuildCardTextureName() + card_set_N streamed-dict
	  technique, no header/config difference worth porting) -- the bug was
	  never in HOW cards get drawn, only in WHEN this file decided a hand
	  was safe to draw.

	Session 13 addition -- Betting Advice (user request): a new Low/
	Medium/High bet-sizing readout, its own Config::Values::
	ShowBettingAdvice toggle (default true), drawn ABOVE the ordinary
	hit/stand/double/split line (DrawBettingAdviceStatus(), positioned at
	AdviceY minus the same fixed offset DrawInsuranceStatus already uses
	to sit just below it). Weighting algorithm lives in two places, same
	"deck-derived when trustworthy, textbook fallback otherwise"
	convention as everything else in this file since Session 7:
	BlackjackDeckSim::EvaluateBettingConfidence() plays the current hand
	out via PlayHandOut() (the same "what actually happens" helper
	EvaluateSplit() already uses) and compares the real result against
	the dealer's own simulated final hand -- High for an immediate
	natural blackjack or a win reached by simply standing pat, Medium for
	a win that only materializes by hitting/doubling into it, Low for a
	push or loss. When that isn't trustworthy (same
	isLastSeatBeforeDealer-or-dealer-already-17+ precondition
	DetermineCheatAction()/EvaluateSplit() need), DetermineBettingAdvice()
	falls back to BlackjackHandEval::EstimateBettingConfidence(), a rough
	textbook-strength heuristic (hand total vs. the dealer's single
	visible up card, same "2-6 weak/9-Ace strong" convention the standard
	basic-strategy chart already uses). Both are pure, tested functions
	(see tests/BlackjackHandEvalTests.cpp/BlackjackDeckSimTests.cpp) --
	this file's own DetermineBettingAdvice()/DrawBettingAdviceStatus() are
	thin wrappers, same separation-of-concerns discipline as
	DetermineAdvice()/DrawAdviceStatus(). NOT yet live-tested against a
	real hand.

	Not yet done: blackjack payout ratio (3:2 vs 6:5 -- one plausible-
	looking `1.5f` constant was found near line 7246 in Session 2 but
	turned out to belong to an unrelated card-prop/caddy setup function,
	not payout math -- still genuinely untraced; the 7-card Charlie payout
	above IS now known, 2x/2.5x, but that's a side-pot special case, not
	the base payout), the 7-card Charlie strategy refinement above,
	correcting BlackjackHandEval.h's basic-strategy chart for single-deck
	play (see the deck-size finding above) -- func_623's own pair-split
	table (Session 5's finding) is actual evidence of what a
	single-deck-correct chart could look like, but wasn't transcribed
	wholesale (see above) -- and the Session 6 dealer count/value anomaly
	above.
*/

#include "BlackjackCheat.h"
#include "BlackjackHandEval.h"
#include "BlackjackDeckSim.h"
#include "Log.h"
#include "GamePointers.h"
#include "Config.h"
#include "Localization.h"
#include "script.h"

#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>

namespace BlackjackCheat
{
	bool Enabled = false;

	void Toggle()
	{
		Enabled = !Enabled;
		Log::Write("BlackjackCheat::Toggle -> {}", Enabled ? "ON" : "OFF");
	}

	void SetEnabled(bool enabled)
	{
		Enabled = enabled;
		Log::Write("BlackjackCheat::SetEnabled -> {}", Enabled ? "ON" : "OFF");
	}

	// ------------------------------------------------------------------
	// UNCONFIRMED struct layout -- see file header comment above for the
	// full derivation/confidence notes on every one of these.
	// ------------------------------------------------------------------
	constexpr std::uint32_t kLocalStructIndex = 14;   // uLocal_14
	constexpr std::uint32_t kLaunchArgsSlot = 3624;   // uScriptParam_0 -- HIGH confidence (pure slot counting)

	constexpr std::uint32_t kTableFieldOffset = 757;  // uLocal_14.f_757 -- CONFIRMED LIVE (Session 6, off-by-one fix from the original 756 static-trace guess -- see file header comment and docs/JOURNAL.md)
	constexpr std::uint32_t kTableSlot = kLocalStructIndex + kTableFieldOffset;

	constexpr std::uint32_t kMySeatSlot = kLocalStructIndex + 9; // uLocal_14.f_9 -- HIGH confidence (Session 5, via func_597's real "is this the human seat" predicate -- see file header comment), CONFIRMED LIVE (Session 18: read 1 with the human at seat 1, while FindMySeatByPed() returned -1 -- the ped-array candidate is wrong/stale, so f_9 is the one to trust; DrawOverlay() already reads it first)

	// uLocal_14.f_1724 -- sibling "ped/scene" struct to Table, same role
	// as poker_sp's own f_3310 -- HIGH confidence (Session 3, see file
	// header comment). f_946[seat] (stride 46) is that seat's live Ped
	// handle, offset+0 of the stride.
	constexpr std::uint32_t kPedSceneFieldOffset = 1724;
	constexpr std::uint32_t kPedSceneSlot = kLocalStructIndex + kPedSceneFieldOffset;
	constexpr std::uint32_t kSeatPedArrayOffset = 946;
	constexpr std::uint32_t kSeatPedStride = 46;

	constexpr std::uint32_t kDealerHandOffset = 2;    // Table.f_2 -- MEDIUM-HIGH confidence

	// Table.f_579 -- CONFIRMED LIVE as func_718's own round-state switch
	// variable (bjack_sp.ysc.c:25809's `switch (uParam0->f_580)`, set by
	// func_1047 via `uParam0->f_580 = iParam1`), off by exactly one word
	// from the decompiled source's own field number -- the same shape as
	// every other struct correction this project has needed
	// (kTableFieldOffset 756->757, deck cursor/count, seat hands offset).
	// A user-directed live investigation (7 F11 "Dump Full Stack JSONL"
	// snapshots across one full round -- sat down / waiting to bet / bet
	// placed / an NPC's turn / my own hit-or-stand decision / the dealer
	// flipping his cards / end of round -- diffed programmatically
	// against every other Table field already confirmed, to rule out
	// coincidental matches inside the dealer hand/seats/deck arrays)
	// found exactly one slot matching func_718's own case values end to
	// end: table+579 read 0/0 (sat down, waiting to bet), 5/5/5 (bet
	// placed through both NPC and my own turn -- func_718 case 5,
	// "waiting on the current seat's action", covers either), then 0/0
	// again (dealer flip, end of round). ONLY EVER 0 OR 5 IN THIS DATA --
	// use it for "is any seat currently mid-decision", nothing finer.
	// It does NOT distinguish "genuinely idle, ready for the next bet"
	// from "the previous round's payout/reveal animation is still
	// playing" -- both read 0 here, since func_718's own case 0 resets
	// the state number immediately, well before the real on-screen
	// animation catches up (case 8 calls func_1075/increments f_701/
	// resets to case 0 in one script tick; the animation takes several
	// more real seconds). For THAT distinction, see kRoundResolvingOffset
	// below -- the ORIGINAL offset this file used for IsAtBettingPhase()
	// before this session's investigation, which turned out to be a
	// separate, real, still-correct field, not an off-by-one error after
	// all. Kept confirmed but currently unused by any caller -- a future
	// need for "is a hand actively being decided" (independent of the
	// resolving-animation question) should reach for this, not
	// kRoundResolvingOffset.
	constexpr std::uint32_t kRoundStateOffset = 579;

	// Table.f_580 -- the ORIGINAL offset this file used before this
	// session (previously named kTableAnimationLockOffset), confirmed by
	// the SAME 7-dump investigation that found kRoundStateOffset above:
	// table+580 read 1/0/0/0/0/1/1 across sat down / waiting to bet / bet
	// placed / NPC's turn / my turn / dealer flipping / end of round --
	// i.e. 1 SPECIFICALLY while the previous round's payout/reveal
	// animation is still playing on screen (even after the script has
	// already reset dealerHand.count to 0 and reshuffled), 0 the rest of
	// the time (both genuinely idle AND actively mid-hand -- dealerHand
	// data itself already distinguishes those two). This is NOT
	// func_718's own state number (off by one from it, at least in
	// behavior) -- it's a separate animation-sequencer lock, most likely
	// what func_477(&(uParam0->f_583))/func_1049 (case 0's own
	// still-busy check, see func_718's decompile) actually reflect. Was
	// briefly mis-diagnosed this session as simply the wrong offset for
	// kRoundStateOffset and nearly replaced by it -- reverted once the
	// same dump data showed the two fields answer genuinely different
	// questions and IsAtBettingPhase() specifically needs THIS one (its
	// whole job is staying false during exactly this lingering-animation
	// window, matching the "1 while still resolving, 0 once genuinely
	// ready" behavior this offset was originally, correctly, found to
	// have).
	constexpr std::uint32_t kRoundResolvingOffset = 580;

	// Table.f_701 (EMPIRICAL -- NOT confirmed to be the same f_701 the
	// decompile names in func_718's case 8/9, which only increments once
	// per COMPLETED round; this reads a distinct value at each of several
	// points WITHIN a single round, so it's very likely an unrelated
	// field the flat-offset arithmetic happens to land on, same trap as
	// the false table+644 lead earlier this same investigation -- treat
	// the number as a raw slot, not a named decompiled field, until a
	// cross-reference proves otherwise). Found via the SAME 7-dump
	// investigation, this time searching for a slot taking on >=4 DISTINCT
	// small clean-int values across the 7 snapshots (kRoundStateOffset
	// and kRoundResolvingOffset above only ever gave 2 each): table+701
	// read 1 (just sat down), 2 (waiting to bet), 3 (bet placed / cards
	// being dealt), 7 (an occupied seat -- NPC or mine, held identically
	// across both) is deciding hit/stand/double/split, then 8 (dealer's
	// forced draw-out + payout + reveal, held steady through end of
	// round). A genuine monotonically-increasing per-phase enum, not a
	// tick/frame counter -- confirmed by npc_turn and my_turn (10 real
	// seconds apart) both reading exactly 7, and dealer_flip/end_round
	// (6 real seconds apart) both reading exactly 8; a raw counter would
	// have kept climbing across either gap. This is the field to use for
	// "which part of the round am I in" -- NOT YET CONFIRMED across a
	// second round in the same sitting (does it reset to 2/3/7/8 for the
	// next round, or keep climbing to 9/10/11/12? -- 579/580 above are
	// each independently confirmed to reset every round, so a genuine
	// per-round reset here would be the expected, consistent answer, but
	// only a second round's dumps can actually prove it).
	constexpr std::uint32_t kRoundPhaseOffset = 701;

	constexpr std::uint32_t kSeatsBase = 27;          // Table.f_27 -- MEDIUM-HIGH confidence
	constexpr std::uint32_t kSeatStride = 60;
	constexpr std::uint32_t kSeatCount = 4;           // CONFIRMED via func_280's direct `iParam1 < 4` bounds check -- HIGH confidence (Session 2)
	constexpr std::uint32_t kSeatOccupiedOffset = 0;  // != -1 means occupied -- confirmed (func_116)
	constexpr std::uint32_t kSeatHandsOffset = 10;    // seat.f_10[hand] (stride 25) -- CONFIRMED LIVE (Session 6, corrected from the original 8 static-trace guess -- verified against all 4 real seats' cards simultaneously, see docs/JOURNAL.md)
	constexpr std::uint32_t kSeatHandCountOffset = 59; // seat.f_59 -- HIGH confidence again (Session 7 second addendum): re-confirmed live via a raw stack dump matching the real screen (read 1 for a genuinely unsplit hand while a stale/leftover hand-1 struct sat right next to it) -- see docs/JOURNAL.md. Briefly distrusted and replaced with a raw-scan-derived count earlier in Session 7; that replacement was itself wrong (see kHandCountOffset's comment below) and has been reverted.
	constexpr std::uint32_t kMaxHandsPerSeat = 2;     // CONFIRMED cap via func_1237 case 6 (`f_59 > 1` blocks split) -- HIGH confidence (Session 2)
	constexpr std::uint32_t kSeatBankrollOffset = 1;  // seat.f_1 -- MEDIUM confidence (Session 2). Session 10: now also read live by OnTick()'s advice loop (see the canDouble computation below) -- a live-reported bug had advice recommend Double with insufficient bankroll, since canDouble previously only checked card count, never the game's own bankroll-vs-bet legality gate (`f_1 >= f_4[handIndex]`, BlackjackHandEval.h's own header comment).
	// seat.f_4[handIndex] is a script ARRAY, and YSC arrays carry their
	// element count in the first word -- so seat.f_4 itself is the array
	// size (always kMaxHandsPerSeat = 2) and bet[h] lives at f_4 + 1 + h.
	// This used to read f_4 directly (offset 4), i.e. the constant 2, for
	// every hand: Session 9's raw dump (docs/JOURNAL.md, the seat table
	// under "Replaying SimulatePreDeal()") logged f_4[0] = 2 for all three
	// seats, INCLUDING the human seat before it had confirmed any bet --
	// the size word, not a bet. The layout fits exactly: size + 2 bets =
	// f_4..f_6, then the live-confirmed bet-lock flag at f_7. Effect of
	// the old read: canDouble reduced to `bankroll >= 2`, so the Session
	// 10 "don't advise Double without the bankroll" fix never actually
	// gated anything. CONFIRMED LIVE (Session 18): with a $250 bet on
	// the human seat, ProbeSeatHands() logged bet(f_4[0], +5)=250 and
	// the size word (+4)=2 (an NPC seat read bet=4, size word 2).
	constexpr std::uint32_t kSeatBetArrayOffset = 4;   // seat.f_4 -- the array's size word (expect 2)
	constexpr std::uint32_t kSeatBetOffset = kSeatBetArrayOffset + 1; // seat.f_4[0] -- bet[h] is at +5+h. Session 10: read live by OnTick()'s advice loop for the canDouble/canSplit bankroll checks.
	constexpr std::uint32_t kSeatCurrentHandIndexOffset = 3; // seat.f_3 -- HIGH confidence (Session 5, f_N=offset+N convention + func_1063's direct f_3<f_59 comparison), static trace only. Read by DrawOverlay() to pick which split hand gets advice, with a first-live-hand fallback if it reads out of range
	constexpr std::uint32_t kSeatBetConfirmedOffset = 7; // seat.f_7 -- CONFIRMED LIVE (Session 9 live addendum): a before/after dump pair caught it reading 0 for the human seat pre-confirm and 1 post-confirm, while both NPC seats already read 1 in BOTH dumps (they lock in instantly; the table visibly waits on the human) -- exactly the func_1056 mechanism this was traced from. func_759 (line ~27401) reads exactly `seat.f_7`, and that same field is what func_1056 requires nonzero on EVERY occupied seat before the table leaves state 0 for the next round, and what func_1057 (the actual initial-deal function) checks per-seat before dealing into it -- i.e. this is the real "this seat's bet is locked in" flag, not merely "a bet amount is set" (that's kSeatBetOffset/f_4[0], checked separately by both of those same functions). Not read by OnTick(), Probe-only as of Session 9's occupancy-only simplification (see SimulatePreDeal()'s own header comment) -- still a real, confirmed field, just no longer this file's gate for who's about to be dealt in.

	constexpr std::uint32_t kHandStride = 25;         // words per hand struct (dealer's and every seat hand's)
	constexpr std::uint32_t kHandCardsOffset = 0;      // 11 slots, 2 words each, NO header word
	constexpr std::uint32_t kHandMaxCards = 11;
	constexpr std::uint32_t kHandCountOffset = 22;     // hand.f_22 -- CONFIRMED LIVE for BOTH seat hands (Session 6) and the dealer's own hand (Session 7 second addendum: a later dump read 2 here for the dealer, matching a real 2-card hand, resolving Session 6's earlier 0/0 sample as a one-off transient rather than a permanent quirk -- see docs/JOURNAL.md). ReadHand() trusts this field directly again after a brief, actively-wrong detour through raw-array-scanning earlier in Session 7 -- see ReadHand()'s own header comment for why scanning was unsafe (stale, never-cleared trailing cards from a previous deal can look exactly like real ones).
	constexpr std::uint32_t kHandValueOffset = 23;     // hand.f_23 -- CONFIRMED LIVE for both seat hands and the dealer's hand, same Session 7 second-addendum re-confirmation as kHandCountOffset above.

	constexpr std::uint32_t kDeckOffset = 592;         // Table.f_592 -- CONFIRMED LIVE (Session 7 addendum): the 52-card {rank,suit} pattern was located by scanning 3 real stack dumps and landed exactly at kTableSlot+592, matching this offset with no correction needed.
	constexpr std::uint32_t kDeckSlot = kTableSlot + kDeckOffset;
	constexpr std::uint32_t kDeckCursorOffset = 104;   // deck.f_104 -- CONFIRMED LIVE (Session 7 addendum, corrected from the original 105 static-trace guess: across 3 real stack dumps, offset+104 was the one that actually varied between game states (0, 0, 8) while +105 was a constant 52 -- i.e. the cards (offsets 0-103) are immediately followed by cursor with NO spare word, not cursor-then-count as originally guessed). This bug silently broke "Next card (if you Hit)" and desynced SimulateDealerOutcome(): the old +105/+106 pair read (realCount, garbage) instead of (realCursor, realCount), so the deckCursor<deckCount sanity guard was false almost always.
	constexpr std::uint32_t kDeckCountOffset = 105;    // deck.f_105 -- CONFIRMED LIVE (Session 7 addendum, corrected from the original 106 static-trace guess -- see kDeckCursorOffset's comment above); reads a steady 52 across all 3 dumps, matching the single-52-card-deck-per-round finding (Session 3).
	constexpr std::uint32_t kDeckCardsBaseOffset = 0;  // no header word (confirmed via func_458's build loop)
	constexpr std::int32_t kDeckSize = 52;             // Session 3 finding -- a freshly-shuffled deck is always exactly 52 cards (single deck, no shoe)

	namespace
	{
		std::int32_t ReadInt(rage::scrThread* thread, std::uint32_t slot)
		{
			void* raw = GamePointers::ReadScriptLocal(thread, slot);
			return static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(raw));
		}

		// The deck's live card count, clamped to the one real deck size.
		// Every advice/prediction loop bounds its reads by this value, so
		// a garbage read (wrong script, mid-teardown, or an offset that's
		// drifted on a new game build) must never walk those loops past
		// the 52-card array into unrelated table fields and feed their
		// values in as card ranks. Probe*() functions still log the raw
		// field -- they exist to show exactly what memory says.
		std::int32_t ReadDeckCount(rage::scrThread* thread)
		{
			std::int32_t count = ReadInt(thread, kDeckSlot + kDeckCountOffset);
			if (count < 0)
				return 0;
			return count > kDeckSize ? kDeckSize : count;
		}

		// RankName/SuitLetter are used by both configs as of Session 4 --
		// Release now draws the dealer's real hole card via
		// DrawDealerHoleCardStatus() (the same UIDEBUG::_BG_DISPLAY_TEXT
		// pipeline DrawAdviceStatus/DrawInsuranceStatus already use in
		// Release), not just Debug's raw-data text panel/Probe* logging.
		const char* RankName(std::int32_t rank)
		{
			switch (rank)
			{
				case 2: return "2";
				case 3: return "3";
				case 4: return "4";
				case 5: return "5";
				case 6: return "6";
				case 7: return "7";
				case 8: return "8";
				case 9: return "9";
				case 10: return "10";
				case 11: return "J";
				case 12: return "Q";
				case 13: return "K";
				case 14: return "A";
				default: return "?";
			}
		}

		// Suit-letter mapping carried over from poker_sp's own confirmed
		// finding (0=Hearts, 1=Diamonds, 2=Spades, 3=Clubs) -- NOT
		// independently confirmed for blackjack, see file header comment.
		// Cosmetic only -- BlackjackHandEval.h never looks at suit.
		char SuitLetter(std::int32_t suit)
		{
			switch (suit)
			{
				case 0: return 'H';
				case 1: return 'D';
				case 2: return 'S';
				case 3: return 'C';
				default: return '?';
			}
		}

		// Card FACE texture naming, ported straight from PokerCheat.cpp's
		// BuildCardTextureName()/FindLoadedCardSetDict() -- func_697 (line
		// ~25174, see file header comment) builds a texture NAME as
		// "<SUIT>_<RANK>" inside a "card_set_N" texture dictionary here
		// too, same as poker's func_925/func_1599, on the same
		// not-independently-confirmed-for-blackjack assumption SuitLetter()
		// above already carries. std::string throughout, not PokerCheat's
		// original fixed char[]/sprintf_s version -- same reasoning as
		// FormatCard/FormatCardRun below: no manual buffer size to get
		// wrong. const_cast<char*>(...c_str()) is used only at the actual
		// native call boundary below (DRAW_SPRITE/
		// HAS_STREAMED_TEXTURE_DICT_LOADED require char*, not const
		// std::string&).
		std::string BuildCardTextureName(std::int32_t rank, std::int32_t suit)
		{
			const char* suitName;
			switch (suit)
			{
				case 0: suitName = "HEARTS_"; break;
				case 1: suitName = "DIAMONDS_"; break;
				case 2: suitName = "SPADES_"; break;
				case 3: suitName = "CLUBS_"; break;
				default: suitName = ""; break;
			}

			return std::string(suitName) + RankName(rank);
		}

		// The real card_set_N number depends on which table/location skin
		// is active -- instead of reimplementing that selection logic,
		// this probes which card_set_N dictionary is ALREADY streamed in,
		// since the game itself must have already loaded the correct one
		// to be showing its own cards right now. Falls back to requesting
		// card_set_1 if none are found loaded yet.
		constexpr int kCardSetProbeLo = 1;
		constexpr int kCardSetProbeHi = 8;

		bool FindLoadedCardSetDict(std::string& outDict)
		{
			for (int n = kCardSetProbeLo; n <= kCardSetProbeHi; n++)
			{
				std::string candidate = "card_set_" + std::to_string(n);
				if (TEXTURE::HAS_STREAMED_TEXTURE_DICT_LOADED(const_cast<char*>(candidate.c_str())))
				{
					outDict = candidate;
					return true;
				}
			}

			return false;
		}

		// Type-safe replacements for the old fixed-size char[] + sprintf_s/
		// strcat_s "buffer too small" trap (a real live crash hit while
		// extending DrawNextCardStatus() to show more than one card) --
		// std::string grows as needed, so there is no manual size to get
		// wrong. Used everywhere this file previously built a card list or
		// hand string by hand.
		std::string FormatCard(std::int32_t rank, std::int32_t suit)
		{
			return std::string(RankName(rank)) + SuitLetter(suit);
		}

		// Formats ranks/suits[from, to) as "RS RS RS " (space-separated,
		// trailing space, matching the old sprintf_s("%s%c ", ...) convention
		// callers already expect in their surrounding text).
		std::string FormatCardRun(const std::int32_t* ranks, const std::int32_t* suits, std::int32_t from, std::int32_t to)
		{
			std::string out;
			for (std::int32_t i = from; i < to; i++)
			{
				out += FormatCard(ranks[i], suits[i]);
				out += ' ';
			}
			return out;
		}

		// HIT/STAND/DOUBLE/SPLIT wording now lives in Localization.cpp's
		// kActionLabels (one row per supported language) -- see that
		// file for the full table. Callers use
		// Localization::ActionName(action) directly.

		struct HandCards
		{
			std::int32_t ranks[kHandMaxCards];
			std::int32_t suits[kHandMaxCards];
			std::int32_t count;
		};

		std::string FormatHandCards(const HandCards& hand)
		{
			return FormatCardRun(hand.ranks, hand.suits, 0, hand.count);
		}

		// The UIDEBUG::_BG_DISPLAY_TEXT pipeline's common TEXTFORMAT/FONT
		// wrapper, shared by every Draw*Status function below -- another
		// spot the old code rebuilt by hand into a fixed char[192]/[256]
		// buffer via sprintf_s each time.
		//
		// Builds into one reused buffer, so the per-frame HUD text does no heap
		// allocation once its capacity has grown. The returned pointer is valid
		// until the next call.
		const char* WrapBgFormatText(std::string_view label, int fontSize)
		{
			static std::string buffer;
			std::array<char, 12> digits{};
			const auto sizeEnd = std::to_chars(digits.data(), digits.data() + digits.size(), fontSize).ptr;

			buffer.assign("<TEXTFORMAT RIGHTMARGIN='0'><P ALIGN='Left'><FONT FACE='$Font5' LETTERSPACING='0' SIZE='");
			buffer.append(digits.data(), sizeEnd);
			buffer.append("'>~s~");
			buffer.append(label);
			buffer.append("</FONT></P><TEXTFORMAT>");
			return buffer.c_str();
		}

		// Reads one hand struct (dealer's Table.f_2, or a seat's
		// f_8[handIndex]) at absolute slot `handSlot`.
		// Session 7 SECOND addendum -- REVERTS the session's own earlier
		// "scan for the first invalid entry" fix and goes back to trusting
		// hand.f_22 (kHandCountOffset)/f_23 (kHandValueOffset) directly,
		// clamped to a plausible range. The scan-based approach (added
		// earlier this session to work around Session 6's one-off finding
		// that the dealer's copy of f_22/f_23 read 0/0 despite real cards)
		// turned out to be actively wrong: a live stack dump this session
		// (see docs/JOURNAL.md) caught it walking PAST the real hand into
		// STALE leftover cards from a previous deal that were never
		// cleared and happen to still decode as plausible-looking
		// {rank,suit} pairs (e.g. a seat's real 2-card hand 8D/4D followed
		// by leftover 8H/KD from an earlier round, inflating the read to
		// a phantom 4-card bust) -- unused hand slots are NOT reliably
		// -1-padded the way this file assumed, they can hold fully
		// self-consistent old data (matching cards AND matching f_22/f_23)
		// that's simply stale. The SAME dump showed f_22/f_23 reading
		// correctly for both the dealer (2/10, matching a real 3D+7C
		// hand) and the seat (2/12, matching the real 8D+4D hand) at the
		// exact moment the scan-based read was wrong -- i.e. the fields
		// are the reliable source here, not the raw array. Session 6's
		// dealer 0/0 anomaly is now believed to have been a one-off
		// transient/timing read (probably a probe landing mid-reset,
		// consistent with dealerHand.count also reading 0 at that same
		// moment) rather than a permanent trait -- if a future live
		// session finds it recurring, that needs its own investigation,
		// but blanket-distrusting the field for everyone was the wrong
		// fix and is why "Next card"/advice broke down for a real,
		// unsplit hand. kHandMaxCards clamp kept purely as a defensive
		// bound against a genuinely out-of-range field read.
		HandCards ReadHand(rage::scrThread* thread, std::uint32_t handSlot)
		{
			HandCards hand{};

			std::int32_t count = ReadInt(thread, handSlot + kHandCountOffset);
			if (count < 0)
				count = 0;
			if (count > static_cast<std::int32_t>(kHandMaxCards))
				count = static_cast<std::int32_t>(kHandMaxCards);

			for (std::int32_t i = 0; i < count; i++)
			{
				hand.ranks[i] = ReadInt(thread, handSlot + kHandCardsOffset + static_cast<std::uint32_t>(i) * 2);
				hand.suits[i] = ReadInt(thread, handSlot + kHandCardsOffset + static_cast<std::uint32_t>(i) * 2 + 1);
			}
			hand.count = count;

			return hand;
		}

		// Primary "my seat" determination (Session 3) -- reads each
		// seat's live Ped handle off the Table-sibling scene struct
		// (kPedSceneSlot, see file header comment) and asks the game
		// itself, via a real native call, whether it's the local player.
		// Returns -1 if no seat's ped matches (e.g. bjack_sp isn't
		// running, or between rounds). Deliberately independent of the
		// ambiguous kMySeatSlot (f_9) field -- see that constant's own
		// comment.
		std::int32_t FindMySeatByPed(rage::scrThread* thread)
		{
			std::int32_t myPed = static_cast<std::int32_t>(PLAYER::PLAYER_PED_ID());

			for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
			{
				std::uint32_t pedSlot = kPedSceneSlot + kSeatPedArrayOffset + seat * kSeatPedStride;
				std::int32_t pedHandle = ReadInt(thread, pedSlot);
				if (pedHandle != 0 && pedHandle == myPed)
					return static_cast<std::int32_t>(seat);
			}

			return -1;
		}

		// Session 9 addition -- PRE-BET deal prediction. Answering the
		// user's own question ("when does the script determine what the
		// deck will be, after or before the bet?") required tracing
		// func_718 case 0 (the round-transition state, line ~25823): it
		// calls func_459 (the same rebuild+reshuffle from the file header's
		// Session 3 deck finding) UNCONDITIONALLY on the very first tick
		// after the previous round ends, BEFORE the state machine even
		// starts waiting for the next round's bets (that wait is
		// func_1055/func_1056, line ~35235 -- func_1056 specifically
		// requires every occupied seat's seat.f_7 to be nonzero before
		// state 0 hands off to state 1, the actual dealing state). So the
		// entire round's deck order is already fixed in memory before a
		// single bet is placed -- betting has ZERO influence on the
		// shuffle. This function reads that already-fixed order directly
		// and replays func_1057's own dealing algorithm (line ~35261: for
		// seat 0->1->2->3, deal 2 cards from the deck, then the dealer's
		// own 2 cards last) to predict every hand before it's dealt.
		//
		// func_1057 itself only deals a seat in once BOTH seat.f_7 (bet
		// confirmed -- func_759) and seat.f_4[0] (bet amount -- func_492)
		// are set, and reading those live was this function's FIRST
		// version -- but live testing (docs/JOURNAL.md, Session 9's two
		// live-testing addenda) caught two separate races this created:
		// the human's own f_7 flips essentially the same instant the
		// table actually deals (func_1056 requires it before state 0 can
		// even exit), leaving no visible frame to ever show the human's
		// own predicted hand; and ANY occupied seat -- not just the
		// human's -- confirming late enough could still shift the
		// dealer's predicted index out from under a guess that had
		// already been shown on screen, since the seat-to-deck-index
		// mapping isn't monotonic the way the dealer draw-out cursor is.
		// At the user's explicit direction ("Assume everyone at the table
		// will be betting"), this function now gates purely on
		// OCCUPANCY (kSeatOccupiedOffset) for every seat, human and NPC
		// alike, dropping the bet-confirmed/bet-amount checks entirely --
		// in this minigame every seated player realistically does bet
		// every round, so treating "occupied" as "will play" removes both
		// races at once (occupancy registers as soon as a seat is taken,
		// well before any bet field is even meaningful) at the cost of a
		// known, accepted inaccuracy if a seat is ever occupied but
		// genuinely sits a round out -- ValidatePreDeal()'s own
		// "PreDealCheck" log line (below) will surface that as a real
		// MISMATCH if it ever actually happens, rather than this file
		// silently assuming it never does.
		//
		// Only meaningful against an untouched, freshly-shuffled deck
		// (cursor==0, count==kDeckSize) -- the call site below only invokes
		// this while dealerHand.count==0 (nothing dealt yet this round),
		// and an untouched cursor/count is what "the reshuffle already
		// happened, nothing has been drawn from it yet" looks like from
		// outside the state machine.
		struct PredictedDeal
		{
			bool valid = false;
			bool seatWillPlay[kSeatCount] = {};
			HandCards seatHands[kSeatCount] = {};
			HandCards dealerHand{};
			std::int32_t cursorAfterDeal = 0;
		};

		PredictedDeal SimulatePreDeal(rage::scrThread* thread, std::int32_t deckCursor, std::int32_t deckCount)
		{
			PredictedDeal result{};
			if (deckCursor != 0 || deckCount != kDeckSize)
				return result; // not a freshly-shuffled, untouched deck -- nothing safe to predict from yet

			std::int32_t cursor = 0;
			for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
			{
				std::uint32_t seatBase = kTableSlot + kSeatsBase + seat * kSeatStride;
				std::int32_t occupiedMarker = ReadInt(thread, seatBase + kSeatOccupiedOffset);

				if (occupiedMarker == -1)
					continue; // assumes every occupied seat bets every round -- see this function's own header comment above

				result.seatWillPlay[seat] = true;
				HandCards& hand = result.seatHands[seat];
				hand.ranks[0] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor) * 2);
				hand.suits[0] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor) * 2 + 1);
				hand.ranks[1] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor + 1) * 2);
				hand.suits[1] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor + 1) * 2 + 1);
				hand.count = 2;
				cursor += 2;
			}

			result.dealerHand.ranks[0] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor) * 2);
			result.dealerHand.suits[0] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor) * 2 + 1);
			result.dealerHand.ranks[1] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor + 1) * 2);
			result.dealerHand.suits[1] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(cursor + 1) * 2 + 1);
			result.dealerHand.count = 2;
			cursor += 2;

			result.cursorAfterDeal = cursor;
			result.valid = true;
			return result;
		}

		PredictedDeal g_lastPreDeal{};
		bool g_havePreDealBaseline = false;

		// Session 9 live-testing addendum -- self-validates SimulatePreDeal()
		// the same way UpdateDeckPrediction() already self-validates the
		// dealer draw-out prediction (a "PredictionCheck" log line every
		// round, no F11 needed): added specifically because live testing
		// caught a real MISMATCH (predicted dealer 9S/10C, actual dealer
		// 2D/5D -- see docs/JOURNAL.md). Root cause understood but NOT
		// fully solvable in general (see SimulatePreDeal()'s own header
		// comment): unlike the dealer draw-out prediction, where the
		// cursor only ever moves forward and self-correction is therefore
		// monotonic, THIS prediction's seat-to-deck-index mapping can
		// shift non-monotonically any time ANOTHER seat's bet confirms
		// during the waiting phase -- if that happens in the same tick
		// (or the tick immediately before) the table actually deals,
		// whatever this file last managed to show can still be stale
		// relative to the real deal, with no further tick left to
		// self-correct in. This log line exists to characterize how
		// often/why that happens going forward without needing a manual
		// F11 dump every time -- not a fix, an instrument.
		void ValidatePreDeal(rage::scrThread* thread, bool dealerHasCards, const PredictedDeal& preDeal)
		{
			if (!dealerHasCards)
			{
				if (preDeal.valid)
				{
					g_lastPreDeal = preDeal;
					g_havePreDealBaseline = true;
				}
				return;
			}

			if (!g_havePreDealBaseline)
				return; // never saw a valid pre-deal snapshot this round (e.g. the mod was toggled on mid-round) -- nothing to check

#ifdef _DEBUG
			HandCards actualDealerHand = ReadHand(thread, kTableSlot + kDealerHandOffset);
			std::string predictedDealerStr = FormatHandCards(g_lastPreDeal.dealerHand);
			std::string actualDealerStr = FormatHandCards(actualDealerHand);

			bool dealerMatch = actualDealerHand.count >= 2
				&& g_lastPreDeal.dealerHand.ranks[0] == actualDealerHand.ranks[0] && g_lastPreDeal.dealerHand.suits[0] == actualDealerHand.suits[0]
				&& g_lastPreDeal.dealerHand.ranks[1] == actualDealerHand.ranks[1] && g_lastPreDeal.dealerHand.suits[1] == actualDealerHand.suits[1];

			Log::Write("PreDealCheck: dealer predicted=[ {}] actual=[ {}] {}",
				predictedDealerStr, actualDealerStr,
				dealerMatch ? "MATCH" : "MISMATCH (another seat's bet likely confirmed too late for this file to re-predict before the real deal -- see SimulatePreDeal()'s header comment)");

			for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
			{
				if (!g_lastPreDeal.seatWillPlay[seat])
					continue;

				std::uint32_t seatBase = kTableSlot + kSeatsBase + seat * kSeatStride;
				HandCards actualSeatHand = ReadHand(thread, seatBase + kSeatHandsOffset);
				std::string predictedSeatStr = FormatHandCards(g_lastPreDeal.seatHands[seat]);
				std::string actualSeatStr = FormatHandCards(actualSeatHand);

				bool seatMatch = actualSeatHand.count >= 2
					&& g_lastPreDeal.seatHands[seat].ranks[0] == actualSeatHand.ranks[0] && g_lastPreDeal.seatHands[seat].suits[0] == actualSeatHand.suits[0]
					&& g_lastPreDeal.seatHands[seat].ranks[1] == actualSeatHand.ranks[1] && g_lastPreDeal.seatHands[seat].suits[1] == actualSeatHand.suits[1];

				Log::Write("PreDealCheck: seat {} predicted=[ {}] actual=[ {}] {}",
					seat, predictedSeatStr, actualSeatStr, seatMatch ? "MATCH" : "MISMATCH");
			}
#endif

			g_havePreDealBaseline = false;
			g_lastPreDeal = PredictedDeal{};
		}

		// Session 9 THIRD live bug report -- "it's showing the [pre-deal]
		// prediction while the round concludes (dealer draws his cards)".
		// Root cause: func_718's own state machine (file header, Session 9
		// addendum) resets Table.f_2 (the dealer's hand, read as
		// dealerHand.count here) and reshuffles the deck on the FIRST
		// script tick after the previous round ends -- but the real
		// table's "dealer collects cards"/payout ANIMATION keeps playing
		// for several more real seconds, decoupled from that already-
		// updated script state. An earlier version of this fix (Session 9
		// fourth/fifth addenda) guessed at a fixed real-time delay
		// (`IsPreDealSettled()`, 1.5s); that was replaced by reading
		// Table.f_580 directly once live testing showed it toggling
		// exactly in sync with the real phase transition (1 while still
		// resolving, 0 once genuinely ready for bets). Superseded again by
		// kRoundPhaseOffset (see its own comment) once that field's finer
		// granularity was found. User-confirmed live: phase 1 (just sat
		// down, before a round has even started) must NOT count as
		// betting phase either -- only phase 2 (genuinely waiting for a
		// bet) is safe to draw predictions on. Excludes phase 8 (dealer
		// resolving/lingering reveal animation) too, which is exactly the
		// distinction this function exists for.
		bool IsAtBettingPhase(rage::scrThread* thread, bool dealerHasCards)
		{
			if (dealerHasCards)
				return false;

			return ReadInt(thread, kTableSlot + kRoundPhaseOffset) == 2;
		}

		// Deterministic deck-ahead prediction (Session 4) -- the blackjack
		// equivalent of PokerCheat's BuildPredictedBoard(). bjack_sp deals
		// from a single 52-card deck, fully shuffled and fixed for the
		// whole round before a single card is drawn (Session 3, see file
		// header comment) -- so anything not yet drawn is not a guess, it's
		// already sitting in memory at a known cursor position, exactly the
		// property PokerCheat's own predicted board relies on for
		// poker_sp's deck. Two things follow directly from that:
		//   1. The dealer's hole card is ALREADY dealt the moment the round
		//      starts (dealerHand.count reaches 2 immediately, same as
		//      every seat) -- it's just not shown on screen until the round
		//      resolves. Reading dealerHand.ranks[0] is exact, real data,
		//      not a prediction -- same category of thing as PokerCheat
		//      showing every opponent's real hole cards, not a probabilistic
		//      guess at all. (Session 7: index 0 is the hole card and index
		//      1 is the real up card -- the OPPOSITE of what this file
		//      originally assumed -- live-observed directly by the user,
		//      who saw the card this file was calling "hole" was actually
		//      showing face-up on screen. Fixed everywhere this mattered:
		//      here, DrawDealerHoleCardStatus()'s call site, the dealer
		//      up-card rank fed to GetBasicStrategyAction()/insurance, and
		//      UpdateCardCounting() above.)
		//   2. The dealer's OWN forced draws (while total < 17, keep
		//      hitting -- see BlackjackHandEval.h's header comment for the
		//      f_24-based derivation of this rule) can be simulated ahead
		//      of time by reading the next undrawn deck cards in order and
		//      re-running the same stand-on-17 test EvaluateHand() already
		//      implements.
		struct PredictedHand
		{
			std::int32_t ranks[kHandMaxCards];
			std::int32_t suits[kHandMaxCards];
			std::int32_t knownCount;  // cards already actually dealt (real data, e.g. up card + hole card)
			std::int32_t totalCount;  // knownCount + predicted future draws
		};

		// Session 5: bjack_sp's deck cursor IS shared by every seat's hits
		// AND the dealer's own draws (turn order confirmed strictly
		// ascending seat 0->1->2->3, dealer last -- see file header
		// comment), so a single snapshot taken at round start was only
		// ever a "what if nobody else draws" guess. The fix isn't to
		// replicate every other seat's AI decision logic (func_1002/
		// func_623, also traced this session) -- it's simpler than that:
		// UpdateDeckPrediction() now calls this every tick using the LIVE
		// cursor rather than a frozen one. The cursor only ever advances
		// as real draws happen, so re-running "what would the dealer draw
		// starting from HERE" every tick means the last call made right
		// before the dealer's real turn begins is automatically exact --
		// by construction, nothing is left to guess at by then. Earlier in
		// the round this still reads as "if no one else draws from this
		// point", same as before; it just keeps re-grounding itself in
		// reality instead of committing to a guess once and sticking with
		// it. See UpdateDeckPrediction()'s round-end "PredictionCheck" log
		// line and ProbeDeckPrediction() for how a live session can verify
		// this actually converges the way this reasoning predicts.
		PredictedHand SimulateDealerOutcome(rage::scrThread* thread, const HandCards& dealerHand, std::int32_t deckCursor, std::int32_t deckCount)
		{
			PredictedHand result{};
			result.knownCount = dealerHand.count;
			if (result.knownCount > static_cast<std::int32_t>(kHandMaxCards))
				result.knownCount = kHandMaxCards;

			for (std::int32_t i = 0; i < result.knownCount; i++)
			{
				result.ranks[i] = dealerHand.ranks[i];
				result.suits[i] = dealerHand.suits[i];
			}
			result.totalCount = result.knownCount;

			std::int32_t simCursor = deckCursor;
			BlackjackHandEval::HandValue value = BlackjackHandEval::EvaluateHand(result.ranks, result.totalCount);

			while (value.total < 17 && result.totalCount < static_cast<std::int32_t>(kHandMaxCards) && simCursor >= 0 && simCursor < deckCount)
			{
				result.ranks[result.totalCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(simCursor) * 2);
				result.suits[result.totalCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(simCursor) * 2 + 1);
				result.totalCount++;
				simCursor++;

				value = BlackjackHandEval::EvaluateHand(result.ranks, result.totalCount);
			}

			return result;
		}

		// Session 7 FOURTH addendum -- replaces GetBasicStrategyAction() as
		// the source of hit/stand/double advice with a fully deck-derived
		// simulation, at the user's explicit request ("remove this card
		// counting crap, just have it do pure cheating -- all logic should
		// be derived based on reading the cards"). Textbook basic strategy
		// (still used ONLY for the split/no-split decision below) is blind
		// to the actual next card by design -- it's the best play against
		// an UNKNOWN card. This mod already reads the exact deck, so
		// blindly standing on a hard 18 when the known next card is a 3
		// (making 21) was leaving free information on the table -- the
		// bug report that prompted this.
		//
		// Session 7 SIXTH addendum -- the actual simulation now lives in
		// BlackjackDeckSim.h (BlackjackDeckSim::DetermineCheatAction()),
		// not inline here. Reason: the first version of this lived
		// entirely in this file, reading straight from game memory via
		// `thread`, which meant it could ONLY ever be eyeballed in-game --
		// exactly the trap docs/JOURNAL.md already flags for PokerCheat's
		// original hand-eval code (unverified for 9 sessions for lack of
		// an automated check). That first version had a real bug (see
		// BlackjackDeckSim.h's own header comment for the full story: it
		// ALWAYS recommended Stand, even on a bust-proof hard 11) that
		// went live specifically because it wasn't unit-testable. This
		// wrapper's only job now is reading the live deck into a plain
		// rank array and handing it to the pure, tested function.
		//
		// Caveat this inherits from BlackjackDeckSim::SimulateDealerFromRanks():
		// if another occupied seat still has to act between this hand and
		// the dealer's turn, their real hits will shift the cursor by an
		// amount this function can't know in advance (this project
		// deliberately never ported func_623's own ~1860-line AI decision
		// table, see the file header's Session 5 addendum) -- so the
		// outcome this simulates is exact once this is the last seat left
		// to act before the dealer, and unreliable otherwise. Session 9
		// SECOND live bug report caught what "unreliable" actually meant
		// in practice: a bust-proof hard 9 was advised Stand, because the
		// simulation correctly noticed that standing would let the dealer
		// draw a card that happened to bust them -- true only if nothing
		// else drew first, which wasn't the case that round (see
		// BlackjackDeckSim.h's own Session 9 addendum for the full
		// mechanism). `isLastSeatBeforeDealer` (computed by the caller,
		// see DrawOverlay()'s seat loop) is now an explicit, checked
		// precondition instead of a silent assumption -- when false,
		// DetermineCheatAction() defers entirely to
		// BlackjackHandEval::GetBasicStrategyAction() rather than trusting
		// a dealer simulation built on a future that was never going to
		// happen.
		//
		// Session 11 addendum -- Split is now ALSO deck-derived
		// (BlackjackDeckSim::EvaluateSplit()) instead of unconditionally
		// asking the blind textbook pair chart. Live bug report: J,J (a
		// pair basic strategy never splits) with known upcoming cards of
		// Ace then 2 then 7 was advised Stand, even though splitting
		// would have produced a 21 (J,A) and a 19 (J,2,7) -- worth more
		// than standing pat on a single 20. EvaluateSplit() falls back to
		// the textbook chart itself (via its own `trustworthy` flag) when
		// the same dealer-outcome precondition DetermineCheatAction()
		// uses doesn't hold, so the textbook path below is still reached,
		// just no longer unconditionally.
		constexpr std::int32_t kFutureLookahead = 32; // generous bound: worst case is a split's two hands' own draws plus the dealer's, each capped at kHandMaxCards

		BlackjackHandEval::Action DetermineAdvice(rage::scrThread* thread, const HandCards& playerHand, const HandCards& dealerHand,
			std::int32_t deckCursor, std::int32_t deckCount, bool canDouble, bool canSplit, bool isSplitAceHand, bool isLastSeatBeforeDealer)
		{
			if (isSplitAceHand)
				return BlackjackHandEval::Action::Stand; // forced by the game itself, see BlackjackHandEval.h's own header comment

			std::int32_t futureRanks[kFutureLookahead];
			std::int32_t futureCount = 0;
			for (; futureCount < kFutureLookahead; futureCount++)
			{
				std::int32_t idx = deckCursor + futureCount;
				if (idx < 0 || idx >= deckCount)
					break;
				futureRanks[futureCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
			}

			if (canSplit && playerHand.count == 2 && playerHand.ranks[0] == playerHand.ranks[1])
			{
				BlackjackDeckSim::SplitDecision splitDecision = BlackjackDeckSim::EvaluateSplit(
					playerHand.ranks, playerHand.count, dealerHand.ranks, dealerHand.count,
					futureRanks, futureCount, canDouble, isLastSeatBeforeDealer);

				if (splitDecision.trustworthy)
				{
					if (splitDecision.shouldSplit)
						return BlackjackHandEval::Action::Split;
					// else: deck-derived already answered "don't split" exactly -- fall through to the normal Hit/Stand/Double evaluation below, not the blind pair chart
				}
				else
				{
					BlackjackHandEval::Action basicSuggestion = BlackjackHandEval::GetBasicStrategyAction(
						playerHand.ranks, playerHand.count, dealerHand.ranks[1], canDouble, canSplit, false);
					if (basicSuggestion == BlackjackHandEval::Action::Split)
						return BlackjackHandEval::Action::Split;
				}
			}

			return BlackjackDeckSim::DetermineCheatAction(playerHand.ranks, playerHand.count, dealerHand.ranks, dealerHand.count,
				futureRanks, futureCount, canDouble, isSplitAceHand, isLastSeatBeforeDealer);
		}

		// Session 13 addition -- Betting Advice (user request: a Low/
		// Medium/High bet-sizing readout, shown above the ordinary
		// hit/stand/double/split line). Same futureRanks-gathering
		// pattern as DetermineAdvice() above -- reads the live deck into
		// a plain rank array and hands it to the pure, tested
		// BlackjackDeckSim::EvaluateBettingConfidence(). When that isn't
		// trustworthy (another occupied seat still has to act before the
		// dealer, and the dealer's own hand isn't already 17+), falls
		// back to BlackjackHandEval::EstimateBettingConfidence() -- a
		// rough, non-deck-derived heuristic, same "textbook chart when
		// deck simulation isn't trustworthy" convention DetermineAdvice()
		// itself already uses for Split.
		//
		// ownDrawsExact (code-review fix): the deck-derived path assumes
		// this hand's own hits come straight off deckCursor. Pre-deal,
		// that only holds when no occupied seat acts BEFORE this one --
		// a lower seat's hits shift the cursor first, so the deck-derived
		// playout would hand this hand cards it will never actually get
		// (isLastSeatBeforeDealer only covers seats AFTER it, and the
		// "dealer already 17+" shortcut doesn't help either, since it's
		// this hand's own cards that are wrong). A natural is still exact
		// either way -- it needs no draws on either side.
		BlackjackHandEval::BettingConfidence DetermineBettingAdvice(rage::scrThread* thread, const HandCards& playerHand, const HandCards& dealerHand,
			std::int32_t deckCursor, std::int32_t deckCount, bool canDouble, bool isLastSeatBeforeDealer, bool ownDrawsExact)
		{
			if (!ownDrawsExact)
			{
				BlackjackHandEval::HandValue playerNow = BlackjackHandEval::EvaluateHand(playerHand.ranks, playerHand.count);
				if (playerNow.blackjack)
				{
					BlackjackHandEval::HandValue dealerNow = BlackjackHandEval::EvaluateHand(dealerHand.ranks, dealerHand.count);
					return dealerNow.blackjack ? BlackjackHandEval::BettingConfidence::Low : BlackjackHandEval::BettingConfidence::High;
				}
				return BlackjackHandEval::EstimateBettingConfidence(playerHand.ranks, playerHand.count, dealerHand.ranks[1]);
			}

			std::int32_t futureRanks[kFutureLookahead];
			std::int32_t futureCount = 0;
			for (; futureCount < kFutureLookahead; futureCount++)
			{
				std::int32_t idx = deckCursor + futureCount;
				if (idx < 0 || idx >= deckCount)
					break;
				futureRanks[futureCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
			}

			BlackjackDeckSim::BettingAdvice advice = BlackjackDeckSim::EvaluateBettingConfidence(
				playerHand.ranks, playerHand.count, dealerHand.ranks, dealerHand.count,
				futureRanks, futureCount, canDouble, isLastSeatBeforeDealer);

			if (advice.trustworthy)
				return advice.confidence;

			return BlackjackHandEval::EstimateBettingConfidence(playerHand.ranks, playerHand.count, dealerHand.ranks[1]);
		}

		PredictedHand g_predictedDealerOutcome{};   // LIVE -- refreshed every tick, for the HUD (always the current best guess)
		PredictedHand g_predictionBaseline{};       // FROZEN at round start -- for PredictionCheck validation only (see below)
		bool g_predictionRoundActive = false;
		HandCards g_predictionLastDealerHand{};

		// Two different jobs need two different snapshots (Session 5):
		// the HUD wants the freshest possible guess (g_predictedDealerOutcome,
		// re-simulated from the LIVE cursor every tick -- see
		// SimulateDealerOutcome()'s own header comment for why that's
		// self-correcting), but VALIDATING that guess against reality only
		// means something if the guess being checked was made BEFORE the
		// real cards existed -- re-simulating right up to round end would
		// just compare the dealer's real final hand against itself,
		// trivially "matching" every time and proving nothing. So
		// g_predictionBaseline is captured ONCE, right when each round
		// starts (dealerHand.count's 0->positive edge, the same trigger
		// this function used before Session 5), frozen there, and it's
		// THAT snapshot the round-end "PredictionCheck" log line compares
		// against the dealer's real final hand -- runs every tick
		// regardless of HUD toggles, same convention as
		// UpdateCardCounting(), so the log line fires automatically during
		// normal play with no F11 interaction needed. This is the single
		// most useful piece of live evidence a future session can gather
		// about whether kDeckSlot/kDeckCursorOffset and the turn-order
		// trace above are actually right, the same role PokerCheat's own
		// "PredictionCheck ... MATCH" line played for poker_sp's deck (see
		// that project's docs/JOURNAL.md).
		void UpdateDeckPrediction(rage::scrThread* thread, const HandCards& dealerHand, bool dealerHasCards)
		{
			if (dealerHasCards)
			{
				std::int32_t deckCursor = ReadInt(thread, kDeckSlot + kDeckCursorOffset);
				std::int32_t deckCount = ReadDeckCount(thread);
				g_predictedDealerOutcome = SimulateDealerOutcome(thread, dealerHand, deckCursor, deckCount);

				if (!g_predictionRoundActive)
					g_predictionBaseline = g_predictedDealerOutcome;

				g_predictionRoundActive = true;
				g_predictionLastDealerHand = dealerHand;
			}

			if (!dealerHasCards && g_predictionRoundActive)
			{
#ifdef _DEBUG
				std::string predictedStr = FormatCardRun(g_predictionBaseline.ranks, g_predictionBaseline.suits,
					g_predictionBaseline.knownCount, g_predictionBaseline.totalCount);
				std::string actualStr = FormatCardRun(g_predictionLastDealerHand.ranks, g_predictionLastDealerHand.suits,
					g_predictionBaseline.knownCount, g_predictionLastDealerHand.count);

				bool match = (g_predictionBaseline.totalCount == g_predictionLastDealerHand.count);
				if (match)
				{
					for (std::int32_t i = g_predictionBaseline.knownCount; i < g_predictionBaseline.totalCount; i++)
					{
						if (g_predictionBaseline.ranks[i] != g_predictionLastDealerHand.ranks[i])
						{
							match = false;
							break;
						}
					}
				}

				Log::Write("PredictionCheck: dealer draw-out predicted-at-round-start=[ {}] actual=[ {}] {}",
					predictedStr, actualStr,
					match ? "MATCH" : "MISMATCH (expected -- this baseline is deliberately the OLD round-start-only guess for validation purposes; the HUD's live prediction self-corrects independently, see SimulateDealerOutcome()'s header comment)");
#endif
				g_predictionRoundActive = false;
				g_predictionBaseline = PredictedHand{};
				g_predictedDealerOutcome = PredictedHand{};
				g_predictionLastDealerHand = HandCards{};
			}
		}

#ifdef _DEBUG
		constexpr int kPanelR = 22, kPanelG = 18, kPanelB = 14, kPanelA = 205;
		constexpr int kTextR = 235, kTextG = 222, kTextB = 194, kTextA = 235;
		constexpr int kTitleR = 255, kTitleG = 238, kTitleB = 180, kTitleA = 255;

		// Text panel -- Debug-only, same reasoning as PokerCheat's DrawLine/
		// DrawPanel: this raw-data dump is a dev surface, not something an
		// end user needs, and RDR2's legacy UI::DRAW_TEXT path has no real
		// in-game font available to it regardless (see PokerCheat.cpp's
		// DrawFontTest() header comment for the full derivation -- not
		// re-litigated here, same game build, same conclusion).
		void DrawLine(float x, float y, const std::string& text, bool title = false)
		{
			const Config::Values& cfg = Config::Get();
			float textScale = title ? cfg.TitleTextScale : cfg.TextScale;
			UI::SET_TEXT_SCALE(0.0f, textScale);
			if (title)
				UI::SET_TEXT_COLOR_RGBA(kTitleR, kTitleG, kTitleB, kTitleA);
			else
				UI::SET_TEXT_COLOR_RGBA(kTextR, kTextG, kTextB, kTextA);
			UI::SET_TEXT_CENTRE(0);
			UI::SET_TEXT_DROPSHADOW(1, 0, 0, 0, 200);
			UI::DRAW_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(text.c_str())), x, y);
		}

		void DrawPanel(float x, float y, float width, float height)
		{
			GRAPHICS::DRAW_RECT(x + width * 0.5f, y + height * 0.5f, width, height, kPanelR, kPanelG, kPanelB, kPanelA, 0, 0);
		}
#endif

		// Standalone advice readout ("HIT"/"STAND"/"DOUBLE"/"SPLIT"), drawn
		// via the UIDEBUG::_BG_DISPLAY_TEXT/$Font5 pipeline -- the ONLY
		// text pipeline confirmed to actually render on this game build
		// (1491.50); UI::DRAW_TEXT/SET_TEXT_COLOR_RGBA are nullsub here
		// (see PokerCheat.cpp's DrawFontTest() header comment for the full
		// derivation -- a game-build fact, not poker-specific, carried
		// over verbatim). Shown in both Debug and Release, same as
		// PokerCheat's DrawWinPredictionStatus.
#ifndef _DEBUG
		constexpr float kReleaseAdviceX = 0.4f;
		constexpr float kReleaseAdviceY = 0.5f;
#endif

		void DrawAdviceStatus(BlackjackHandEval::Action action)
		{
#ifdef _DEBUG
			const Config::Values& cfg = Config::Get();
			float adviceX = cfg.AdviceX;
			float adviceY = cfg.AdviceY;
#else
			float adviceX = kReleaseAdviceX;
			float adviceY = kReleaseAdviceY;
#endif
			int r = 140, g = 255, b = 140;
			switch (action)
			{
				case BlackjackHandEval::Action::Hit: r = 255; g = 220; b = 140; break;
				case BlackjackHandEval::Action::Stand: r = 140; g = 220; b = 255; break;
				case BlackjackHandEval::Action::Double: r = 255; g = 180; b = 255; break;
				case BlackjackHandEval::Action::Split: r = 180; g = 255; b = 180; break;
			}

			const char* formatText = WrapBgFormatText(Localization::ActionName(action), 40);

			UIDEBUG::_BG_SET_TEXT_COLOR(r, g, b, 255);
			UIDEBUG::_BG_DISPLAY_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(formatText)), adviceX, adviceY);
		}

		// Session 13 addition -- Betting Advice, same pipeline/convention
		// as DrawAdviceStatus but positioned just ABOVE it (user request:
		// "Display it above ShowAdvice"), same offset-from-AdviceY
		// technique DrawInsuranceStatus below uses to sit just below it.
		// BET LOW/MEDIUM/HIGH wording now lives in Localization.cpp's
		// kBettingConfidenceLabels (one row per supported language) --
		// see that file for the full table. Callers use
		// Localization::BettingConfidenceLabel(confidence) directly.

#ifndef _DEBUG
		constexpr float kReleaseBettingAdviceYOffset = -0.045f;
#endif

		void DrawBettingAdviceStatus(BlackjackHandEval::BettingConfidence confidence)
		{
#ifdef _DEBUG
			const Config::Values& cfg = Config::Get();
			float x = cfg.AdviceX;
			float y = cfg.AdviceY - 0.045f;
#else
			float x = kReleaseAdviceX;
			float y = kReleaseAdviceY + kReleaseBettingAdviceYOffset;
#endif
			int r = 255, g = 140, b = 140;
			switch (confidence)
			{
				case BlackjackHandEval::BettingConfidence::Low: r = 255; g = 140; b = 140; break;
				case BlackjackHandEval::BettingConfidence::Medium: r = 255; g = 220; b = 140; break;
				case BlackjackHandEval::BettingConfidence::High: r = 140; g = 255; b = 140; break;
			}

			const char* formatText = WrapBgFormatText(Localization::BettingConfidenceLabel(confidence), 32);

			UIDEBUG::_BG_SET_TEXT_COLOR(r, g, b, 255);
			UIDEBUG::_BG_DISPLAY_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(formatText)), x, y);
		}

		// Same pipeline/convention as DrawAdviceStatus -- positioned just
		// below it (see kReleaseInsuranceY/cfg.AdviceY+offset) so both can
		// be visible at once when the dealer shows an Ace.
#ifndef _DEBUG
		constexpr float kReleaseInsuranceYOffset = 0.045f;
#endif

		void DrawInsuranceStatus(bool takeInsurance)
		{
#ifdef _DEBUG
			const Config::Values& cfg = Config::Get();
			float x = cfg.AdviceX;
			float y = cfg.AdviceY + 0.045f;
#else
			float x = kReleaseAdviceX;
			float y = kReleaseAdviceY + kReleaseInsuranceYOffset;
#endif
			const std::string_view label = Localization::InsuranceLabel(takeInsurance);
			int r = takeInsurance ? 180 : 200, g = takeInsurance ? 255 : 200, b = takeInsurance ? 180 : 200;

			const char* formatText = WrapBgFormatText(label, 26);

			UIDEBUG::_BG_SET_TEXT_COLOR(r, g, b, 255);
			UIDEBUG::_BG_DISPLAY_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(formatText)), x, y);
		}

		// Dealer's real hole card, PRIMARY feature as of Session 4 -- exact
		// data (see PredictedHand's header comment above), not a
		// probability. Session 8 (user request): drawn as an actual
		// card-face icon in the top-right corner -- the same calibrated
		// spot and DRAW_SPRITE/card_set_N technique PokerCheat's own
		// DrawCommunityCardIcons() uses for its community-card strip --
		// instead of the text line this used to be (DrawDealerHoleCardStatus,
		// now removed). Ghosted (reduced alpha), same convention
		// PokerCheat uses for a predicted-but-not-yet-revealed community
		// card: this card IS already dealt/known with certainty, but the
		// real table still shows it face down, so the alpha marks "we
		// know this, the game hasn't shown it yet" rather than genuine
		// uncertainty.
		constexpr int kHoleCardIconAlpha = 140;

#ifndef _DEBUG
		// Named (not just local to DrawDealerHoleCardIcon) so DrawOverlay()'s
		// Session 9 predicted-dealer-hand call site below can anchor off the
		// exact same spot without duplicating these literals.
		constexpr float kReleaseHoleCardIconX = 0.957f; // user-confirmed via live Reload Config tuning (0.821 initial guess -> 0.957)
		constexpr float kReleaseHoleCardIconY = 0.078f;
		constexpr float kReleaseHoleCardIconWidth = 0.03f;
		constexpr float kReleaseHoleCardIconHeight = 0.075f;
#endif

		void DrawDealerHoleCardIcon(std::int32_t rank, std::int32_t suit)
		{
			if (rank < 2)
				return;

			std::string cardSetDict;
			if (!FindLoadedCardSetDict(cardSetDict))
			{
				TEXTURE::REQUEST_STREAMED_TEXTURE_DICT(const_cast<char*>("card_set_1"), false);
				return;
			}

#ifdef _DEBUG
			const Config::Values& cfg = Config::Get();
			float x = cfg.HoleCardIconX;
			float y = cfg.HoleCardIconY;
			float width = cfg.HoleCardIconWidth;
			float height = cfg.HoleCardIconHeight;
#else
			constexpr float x = kReleaseHoleCardIconX;
			constexpr float y = kReleaseHoleCardIconY;
			constexpr float width = kReleaseHoleCardIconWidth;
			constexpr float height = kReleaseHoleCardIconHeight;
#endif

			std::string textureName = BuildCardTextureName(rank, suit);

			GRAPHICS::DRAW_SPRITE(const_cast<char*>(cardSetDict.c_str()), const_cast<char*>(textureName.c_str()),
				x, y, width, height, 0.0f, 255, 255, 255, kHoleCardIconAlpha, 0);
		}

		// Session 9 -- draws a predicted (not-yet-dealt) 2-card hand as a
		// pair of card-face icons, reusing the same DRAW_SPRITE/card_set_N
		// technique and ghosted alpha as DrawDealerHoleCardIcon() above
		// ("we know this, it hasn't shown on screen yet" rather than
		// genuine uncertainty -- see that function's own header comment).
		// Generic over position/spacing so it can draw both the dealer's
		// predicted hand (growing LEFT from a right-edge anchor, negative
		// spacingX) and the player's own predicted hand (growing RIGHT,
		// positive spacingX) with one function -- see the two call sites
		// in DrawOverlay() below.
		void DrawPredictedHandIcons(const HandCards& hand, float baseX, float baseY, float spacingX, float width, float height)
		{
			if (hand.count < 2)
				return;

			std::string cardSetDict;
			if (!FindLoadedCardSetDict(cardSetDict))
			{
				TEXTURE::REQUEST_STREAMED_TEXTURE_DICT(const_cast<char*>("card_set_1"), false);
				return;
			}

			for (std::int32_t i = 0; i < hand.count; i++)
			{
				if (hand.ranks[i] < 2)
					continue;

				std::string textureName = BuildCardTextureName(hand.ranks[i], hand.suits[i]);
				GRAPHICS::DRAW_SPRITE(const_cast<char*>(cardSetDict.c_str()), const_cast<char*>(textureName.c_str()),
					baseX + static_cast<float>(i) * spacingX, baseY, width, height, 0.0f, 255, 255, 255, kHoleCardIconAlpha, 0);
			}
		}

		// Whatever cards are sitting at and after the deck's current
		// cursor, shown only while it's genuinely your turn (haveAdvice
		// true, see the call site) -- Session 5 confirmed turn order is
		// strictly ascending seat 0->1->2->3 then the dealer, ALL sharing
		// this one cursor, so if it's your decision point right now,
		// nothing else has drawn since, and the card at the cursor IS
		// exactly what a Hit gives you right now -- deterministic, not a
		// guess, same "single fixed deck" property SimulateDealerOutcome()
		// already relies on. The two cards AFTER that are exactly as
		// deterministic (same fixed deck, same cursor) but are NOT
		// necessarily what you personally draw next -- another seat's hit,
		// your own second hit, or the dealer's draw-out can consume them
		// first depending on how play proceeds from here, so the label
		// only promises "if you Hit" for cursor+0. Positioned below
		// Insurance, which is below Advice (the dealer hole-card text line
		// that used to sit between them is now the top-right icon above,
		// see DrawDealerHoleCardIcon()).
#ifndef _DEBUG
		constexpr float kReleaseNextCardYOffset = 0.09f;
#endif

		// Session 8 (user request): the "next card(s) if you Hit" cards
		// themselves are now drawn as card-face icons next to a short
		// "Next cards:" label, same DRAW_SPRITE/card_set_N technique as
		// DrawDealerHoleCardIcon() above, instead of spelling each card
		// out as text (RankName+SuitLetter) inline in the label.
		constexpr int kNextCardIconMaxCount = 3;

#ifndef _DEBUG
		// Named (not just local to DrawNextCardIcons) so DrawOverlay()'s
		// betting-phase "next cards after the deal" call site below can
		// anchor off the exact same spot without duplicating these
		// literals. This slot used to also host the pre-deal predicted-
		// own-hand icons (Session 9); that moved to kReleaseMyHandIcon*
		// below (user request) specifically to free this spot up so the
		// betting-phase view could show a same-position "next cards"
		// preview matching the post-deal one.
		constexpr float kReleaseNextCardIconBaseX = 0.48f;
		constexpr float kReleaseNextCardIconY = 0.59f;
		constexpr float kReleaseNextCardIconSpacingX = 0.03f;
		constexpr float kReleaseNextCardIconWidth = 0.025f;
		constexpr float kReleaseNextCardIconHeight = 0.06f;

		// Player's own predicted hand, pre-deal/betting-phase only (see
		// SimulatePreDeal()) -- drawn near the player's own on-screen
		// avatar instead of the NextCardIcon* slot above. User-confirmed
		// via live Reload Config tuning (0.2/0.2 initial guess -> these),
		// same process NextCardIconBaseX/HoleCardIconX already went
		// through (see Config.h's MyHandIconX/Y for the Debug-tunable
		// equivalent).
		constexpr float kReleaseMyHandIconX = 0.14f;
		constexpr float kReleaseMyHandIconY = 0.925f;
		constexpr float kReleaseMyHandIconSpacingX = 0.02f;
		constexpr float kReleaseMyHandIconWidth = 0.02f;
		constexpr float kReleaseMyHandIconHeight = 0.04f;
#endif

		void DrawNextCardIcons(const std::int32_t* ranks, const std::int32_t* suits, std::int32_t count)
		{
			std::string cardSetDict;
			if (!FindLoadedCardSetDict(cardSetDict))
			{
				TEXTURE::REQUEST_STREAMED_TEXTURE_DICT(const_cast<char*>("card_set_1"), false);
				return;
			}

#ifdef _DEBUG
			const Config::Values& cfg = Config::Get();
			float baseX = cfg.NextCardIconBaseX;
			float y = cfg.NextCardIconY;
			float spacingX = cfg.NextCardIconSpacingX;
			float width = cfg.NextCardIconWidth;
			float height = cfg.NextCardIconHeight;
#else
			constexpr float baseX = kReleaseNextCardIconBaseX;
			constexpr float y = kReleaseNextCardIconY;
			constexpr float spacingX = kReleaseNextCardIconSpacingX;
			constexpr float width = kReleaseNextCardIconWidth;
			constexpr float height = kReleaseNextCardIconHeight;
#endif

			for (std::int32_t i = 0; i < count && i < kNextCardIconMaxCount; i++)
			{
				if (ranks[i] < 2)
					continue;

				std::string textureName = BuildCardTextureName(ranks[i], suits[i]);

				GRAPHICS::DRAW_SPRITE(const_cast<char*>(cardSetDict.c_str()), const_cast<char*>(textureName.c_str()),
					baseX + static_cast<float>(i) * spacingX, y, width, height, 0.0f, 255, 255, 255, 255, 0);
			}
		}

		void DrawNextCardStatus(const std::int32_t* ranks, const std::int32_t* suits, std::int32_t count)
		{
#ifdef _DEBUG
			const Config::Values& cfg = Config::Get();
			float x = cfg.AdviceX;
			float y = cfg.AdviceY + 0.09f;
#else
			float x = kReleaseAdviceX;
			float y = kReleaseAdviceY + kReleaseNextCardYOffset;
#endif
			const char* formatText = WrapBgFormatText(Localization::NextCardsLabel(), 26);

			UIDEBUG::_BG_SET_TEXT_COLOR(180, 255, 220, 255);
			UIDEBUG::_BG_DISPLAY_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(formatText)), x, y);

			DrawNextCardIcons(ranks, suits, count);
		}

		void DrawOverlay()
		{
			auto thread = GamePointers::FindScriptThread(rage::Joaat("bjack_sp"));
			if (!thread)
				return;

			// mySeat determination -- Session 7: FLIPPED which candidate
			// is trusted here. FindMySeatByPed() (Session 3's PRIMARY) was
			// live-observed returning -1 (no match at all) in this user's
			// session, while f_9 read 3 -- and 3 is independently
			// corroborated by Session 6's own differential-dump find
			// (the user's real hand that session showed up at seat 3).
			// f_9 is now trusted first; FindMySeatByPed() kept as a
			// logged fallback/comparison only until its own -1 result is
			// understood (kPedSceneSlot/kSeatPedArrayOffset/kSeatPedStride
			// were never live-confirmed, unlike the Session 6 offsets).
			std::int32_t mySeat = ReadInt(thread, kMySeatSlot);
			if (mySeat < 0 || mySeat >= static_cast<std::int32_t>(kSeatCount))
				mySeat = FindMySeatByPed(thread);

			// Session 9 SECOND live bug report -- turn order is strictly
			// ascending seat 0->1->2->3 then the dealer (Session 5), so
			// "is mySeat the last seat left to act before the dealer" is
			// simply "is any HIGHER-indexed seat occupied" -- if so, that
			// seat's own hits will consume some of the cursor before the
			// dealer's real turn ever begins, breaking
			// DetermineCheatAction()'s dealer-simulation premise (see that
			// function's own header comment for the real bug this fixes:
			// a bust-proof hard 9 was advised Stand because the engine
			// assumed the very next undrawn card goes straight to the
			// dealer, when in fact another occupied seat was due to draw
			// it first). Computed up here (not just below, where it used
			// to live) since Session 14's pre-deal betting-advice call
			// needs it too, and occupancy is already meaningful before
			// the deal happens -- see this same field's use in
			// SimulatePreDeal()'s own seatWillPlay derivation.
			bool isMySeatLastBeforeDealer = true;
			if (mySeat >= 0 && mySeat < static_cast<std::int32_t>(kSeatCount))
			{
				for (std::uint32_t higherSeat = static_cast<std::uint32_t>(mySeat) + 1; higherSeat < kSeatCount; higherSeat++)
				{
					std::uint32_t higherSeatBase = kTableSlot + kSeatsBase + higherSeat * kSeatStride;
					if (ReadInt(thread, higherSeatBase + kSeatOccupiedOffset) != -1)
					{
						isMySeatLastBeforeDealer = false;
						break;
					}
				}
			}

			// Code-review fix: the mirror image of the above for seats that
			// act BEFORE mine. While any occupied lower seat is still
			// playing, it's not my turn -- its hits come off the cursor
			// first, so "the next card is mine" (which every piece of
			// advice below assumes) is false. seat.f_3 is the seat's
			// current-hand index: -1 while it waits (reset at round
			// start), 0.. once the table's case 4 starts its turn, and
			// f_59 (its hand count) once the turn-advance loop has
			// resolved all of its hands (see kSeatCurrentHandIndexOffset).
			// "Not done" below is f_3 < f_59 -- the same test func_1063
			// uses for case 4's own "next seat to play" scan (Session 5),
			// so it can't disagree with the game about whose turn it is.
			// A seat with no hands (0) isn't playing this round. Pre-deal, hasOccupiedLowerSeat alone is
			// what matters (every occupied lower seat is going to act
			// first).
			bool hasOccupiedLowerSeat = false;
			bool lowerSeatsDone = true;
			if (mySeat >= 0 && mySeat < static_cast<std::int32_t>(kSeatCount))
			{
				for (std::uint32_t lowerSeat = 0; lowerSeat < static_cast<std::uint32_t>(mySeat); lowerSeat++)
				{
					std::uint32_t lowerSeatBase = kTableSlot + kSeatsBase + lowerSeat * kSeatStride;
					if (ReadInt(thread, lowerSeatBase + kSeatOccupiedOffset) == -1)
						continue;

					hasOccupiedLowerSeat = true;
					std::int32_t lowerHandCount = ReadInt(thread, lowerSeatBase + kSeatHandCountOffset);
					std::int32_t lowerCurrentHand = ReadInt(thread, lowerSeatBase + kSeatCurrentHandIndexOffset);
					if (lowerHandCount > 0 && lowerCurrentHand < lowerHandCount)
						lowerSeatsDone = false;
				}
			}

			HandCards dealerHand = ReadHand(thread, kTableSlot + kDealerHandOffset);
			bool dealerHasCards = dealerHand.count > 0;

			// User request: the dealer's hole-card icon (see the
			// DrawDealerHoleCardIcon() call site below) should stay up
			// from the deal all the way through the dealer's own reveal,
			// not just while dealerHand.count>=2 -- the round resets
			// dealerHand.count to 0 in the same script tick it starts
			// resolving, while the real on-screen "dealer flips his
			// cards" animation keeps playing for several more real
			// seconds. kRoundPhaseOffset's confirmed value 8 covers
			// exactly the dealer's-turn-through-end-of-round window (see
			// its own comment for the live evidence) -- read once here
			// and reused below for both this and IsAtBettingPhase's own
			// (separate) read.
			std::int32_t roundPhase = ReadInt(thread, kTableSlot + kRoundPhaseOffset);
			bool roundResolving = (roundPhase == 8);

			UpdateDeckPrediction(thread, dealerHand, dealerHasCards);

			// Read once, reused by the cheat-action simulation, the "Next
			// card" line, and insurance below -- all three need the exact
			// same live cursor/count snapshot to stay consistent with each
			// other within a single tick.
			std::int32_t liveDeckCursor = ReadInt(thread, kDeckSlot + kDeckCursorOffset);
			std::int32_t liveDeckCount = ReadDeckCount(thread);

			const Config::Values& cfg = Config::Get();

			// Session 9: only worth attempting while nothing has been
			// dealt yet this round (dealerHasCards false) AND Table.f_580
			// reads 0 (CONFIRMED LIVE to mean "genuinely ready for bets" --
			// see kRoundResolvingOffset's own comment for the full
			// derivation) -- see IsAtBettingPhase()'s own header comment
			// for why dealerHand.count==0 alone isn't enough (a real live
			// bug this fixes). See SimulatePreDeal()'s own header comment
			// for the "deck is fixed before the bet" derivation and the
			// self-correcting-guess caveat this is layered on top of.
			bool atBettingPhase = IsAtBettingPhase(thread, dealerHasCards);
			PredictedDeal preDeal = atBettingPhase ? SimulatePreDeal(thread, liveDeckCursor, liveDeckCount) : PredictedDeal{};
			ValidatePreDeal(thread, dealerHasCards, preDeal); // Session 9 live-testing addendum -- see that function's own header comment

#ifdef _DEBUG
			float x = cfg.PanelX;
			float y = cfg.PanelY;
			constexpr float kLineHeight = 0.028f;
			constexpr float kPanelPadding = 0.012f;
			constexpr int kMaxLines = 11; // title + phase + turn + dealer + predicted draws + pre-deal lines, generously

			DrawPanel(x - kPanelPadding, y - kPanelPadding,
				0.36f + kPanelPadding * 2.0f,
				kMaxLines * kLineHeight + kPanelPadding * 2.0f);

			DrawLine(x, y, "BlackjackCheat (UNCONFIRMED offsets -- see docs/JOURNAL.md)", true);
			y += kLineHeight;

			// Session 9 sixth live bug report -- diagnostic, see
			// kRoundPhaseOffset's own comment above (1=sat down,
			// 2=waiting for bet, 3=bet placed/dealing, 7=a seat deciding,
			// 8=dealer resolving/end of round -- NOT YET CONFIRMED across
			// a second round in the same sitting). Always shown (not
			// gated by any toggle) so a live session can watch it change
			// across a round transition without needing to flip anything
			// on first.
			DrawLine(x, y, "Round phase f_701=" + std::to_string(roundPhase) + " atBettingPhase=" + (atBettingPhase ? "yes" : "no"));
			y += kLineHeight;

			// Code-review addition: advice is now gated on seat.f_3
			// (current-hand index) vs seat.f_59 (hand count) for my seat
			// AND every occupied lower seat -- still a static trace only,
			// so show the raw values live. Expect each seat to read
			// f_3=-1 while it waits (reset at round start), 0.. while it
			// acts (the turn loop sets -1 -> 0 when the seat's turn
			// begins) and f_3=f_59 once it's done.
			{
				std::string turnLine = "Turn f_3/f_59:";
				for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
				{
					std::uint32_t seatBase = kTableSlot + kSeatsBase + seat * kSeatStride;
					if (ReadInt(thread, seatBase + kSeatOccupiedOffset) == -1)
						continue;
					turnLine += " s" + std::to_string(seat) + (static_cast<std::int32_t>(seat) == mySeat ? "(me)=" : "=")
						+ std::to_string(ReadInt(thread, seatBase + kSeatCurrentHandIndexOffset)) + "/"
						+ std::to_string(ReadInt(thread, seatBase + kSeatHandCountOffset));
				}
				turnLine += std::string(" lowerDone=") + (lowerSeatsDone ? "yes" : "no");
				DrawLine(x, y, turnLine);
				y += kLineHeight;
			}

			if (cfg.ShowDealerHand && dealerHasCards)
			{
				BlackjackHandEval::HandValue dealerValue = BlackjackHandEval::EvaluateHand(dealerHand.ranks, dealerHand.count);
				std::string cardsStr = FormatHandCards(dealerHand);

				std::string line = "Dealer: " + cardsStr + "- " + std::to_string(dealerValue.total)
					+ (dealerValue.soft ? " (soft)" : "") + (dealerValue.bust ? " BUST" : "");
				DrawLine(x, y, line);
				y += kLineHeight;
			}

			// PRIMARY feature as of Session 4 -- deterministic, not a guess
			// (see PredictedHand's/SimulateDealerOutcome()'s header comments
			// above for the "single fixed deck" reasoning and the turn-order
			// caveat this has that PokerCheat's own board prediction didn't).
			if (cfg.ShowDeckPrediction && dealerHasCards)
			{
				std::string predStr = FormatCardRun(g_predictedDealerOutcome.ranks, g_predictedDealerOutcome.suits,
					g_predictedDealerOutcome.knownCount, g_predictedDealerOutcome.totalCount);
				BlackjackHandEval::HandValue predValue = BlackjackHandEval::EvaluateHand(g_predictedDealerOutcome.ranks, g_predictedDealerOutcome.totalCount);

				std::string line;
				if (g_predictedDealerOutcome.totalCount > g_predictedDealerOutcome.knownCount)
					line = "Predicted dealer draws: " + predStr + "- final " + std::to_string(predValue.total)
						+ (predValue.bust ? " BUST" : "") + " (see PredictionCheck log at round end)";
				else
					line = "Predicted dealer draws: none, already at " + std::to_string(predValue.total);
				DrawLine(x, y, line);
				y += kLineHeight;
			}

			// Session 9: pre-bet deal prediction -- see SimulatePreDeal()'s
			// own header comment. Only ever shows while dealerHasCards is
			// false (preDeal.valid is forced false otherwise, see the call
			// site above), so this and the two blocks above it never both
			// draw in the same tick. Own toggle (ShowCardsBeforeBet), not
			// ShowDeckPrediction -- this is a distinctly more provisional
			// guess (see ValidatePreDeal()'s header comment for a real,
			// live-caught mismatch) and the user may want it off on its
			// own.
			if (cfg.ShowCardsBeforeBet && preDeal.valid)
			{
				std::string dealerStr = FormatHandCards(preDeal.dealerHand);
				DrawLine(x, y, "Predicted dealer (before deal): " + dealerStr);
				y += kLineHeight;

				if (mySeat >= 0 && mySeat < static_cast<std::int32_t>(kSeatCount) && preDeal.seatWillPlay[mySeat])
				{
					std::string myStr = FormatHandCards(preDeal.seatHands[mySeat]);
					DrawLine(x, y, "Predicted your hand (before deal): " + myStr);
					y += kLineHeight;
				}

				// User request: show the next few cards after the deal
				// (not just one), same kNextCardPreviewCount=3 depth as the
				// post-deal "Next cards" preview below -- these are exactly
				// as deterministic as that preview (same fixed deck), just
				// read from cursorAfterDeal instead of the live cursor.
				{
					std::int32_t nextRanks[kNextCardIconMaxCount];
					std::int32_t nextSuits[kNextCardIconMaxCount];
					std::int32_t nextCount = 0;
					for (std::int32_t i = 0; i < kNextCardIconMaxCount; i++)
					{
						std::int32_t idx = preDeal.cursorAfterDeal + i;
						if (idx < 0 || idx >= liveDeckCount)
							break;
						nextRanks[nextCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
						nextSuits[nextCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2 + 1);
						nextCount++;
					}
					if (nextCount > 0)
					{
						DrawLine(x, y, "Next after deal: " + FormatCardRun(nextRanks, nextSuits, 0, nextCount));
						y += kLineHeight;
					}
				}
			}

#endif

			// Release+Debug: the dealer's real hole card, exact data read
			// straight from the already-dealt hand struct (see
			// PredictedHand's header comment) -- drawn as a card-face icon
			// top-right, see DrawDealerHoleCardIcon()'s own header comment.
			// `|| roundResolving` (see its own computation above) extends
			// this through the dealer's own reveal: dealerHand.count>=2
			// alone drops to false the instant func_718 resets for the
			// next round, one script tick before the real on-screen flip
			// animation is done, and ranks[0]/suits[0] are still the
			// correct just-concluded values at that point (count resets
			// before the card data itself is overwritten by the next
			// deal) -- so this is exactly the same data DrawDealerHoleCardIcon()
			// always drew, just kept visible a little longer.
			if (cfg.ShowDeckPrediction && (dealerHand.count >= 2 || roundResolving))
				DrawDealerHoleCardIcon(dealerHand.ranks[0], dealerHand.suits[0]); // Session 7: index 0 is the real hole card, not index 1 -- see PredictedHand's header comment above

			// Session 9: pre-bet deal prediction, Release+Debug -- both of
			// the dealer's predicted cards (growing LEFT from the same
			// top-right anchor DrawDealerHoleCardIcon uses -- unlike the
			// post-deal case, NEITHER dealer card is visible on the real
			// table yet, so both need a stand-in icon here, not just one),
			// and the player's own predicted hand, drawn near the
			// player's own avatar instead of the post-deal "Next cards"
			// slot (user request -- that slot is freed up below to show
			// an actual "next cards after the deal" preview during
			// betting phase, matching the post-deal layout). Own toggle
			// (ShowCardsBeforeBet), see the Debug panel block above for
			// why this isn't folded into ShowDeckPrediction.
			if (cfg.ShowCardsBeforeBet && preDeal.valid)
			{
#ifdef _DEBUG
				DrawPredictedHandIcons(preDeal.dealerHand, cfg.HoleCardIconX, cfg.HoleCardIconY, -cfg.HoleCardIconWidth * 1.1f, cfg.HoleCardIconWidth, cfg.HoleCardIconHeight);
				if (mySeat >= 0 && mySeat < static_cast<std::int32_t>(kSeatCount) && preDeal.seatWillPlay[mySeat])
					DrawPredictedHandIcons(preDeal.seatHands[mySeat], cfg.MyHandIconX, cfg.MyHandIconY, cfg.MyHandIconSpacingX, cfg.MyHandIconWidth, cfg.MyHandIconHeight);
#else
				DrawPredictedHandIcons(preDeal.dealerHand, kReleaseHoleCardIconX, kReleaseHoleCardIconY, -kReleaseHoleCardIconWidth * 1.1f, kReleaseHoleCardIconWidth, kReleaseHoleCardIconHeight);
				if (mySeat >= 0 && mySeat < static_cast<std::int32_t>(kSeatCount) && preDeal.seatWillPlay[mySeat])
					DrawPredictedHandIcons(preDeal.seatHands[mySeat], kReleaseMyHandIconX, kReleaseMyHandIconY, kReleaseMyHandIconSpacingX, kReleaseMyHandIconWidth, kReleaseMyHandIconHeight);
#endif

				// User request: show the next 3 cards during the
				// betting-phase prediction too, same
				// label+icon-strip presentation DrawNextCardStatus
				// already gives the post-deal "if you Hit" preview
				// below -- read from cursorAfterDeal (the card right
				// after the predicted deal) instead of the live
				// cursor. Reuses that same NextCardIcon*/Advice screen
				// slot, which preDeal.valid guarantees is otherwise
				// idle this tick (the post-deal block below only ever
				// draws once dealerHasCards/haveAdvice, i.e. after the
				// real deal has happened).
				constexpr std::int32_t kPreDealNextCardPreviewCount = kNextCardIconMaxCount;
				std::int32_t preDealNextRanks[kPreDealNextCardPreviewCount];
				std::int32_t preDealNextSuits[kPreDealNextCardPreviewCount];
				std::int32_t preDealNextCount = 0;
				for (std::int32_t i = 0; i < kPreDealNextCardPreviewCount; i++)
				{
					std::int32_t idx = preDeal.cursorAfterDeal + i;
					if (idx < 0 || idx >= liveDeckCount)
						break;
					preDealNextRanks[preDealNextCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
					preDealNextSuits[preDealNextCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2 + 1);
					preDealNextCount++;
				}
				if (preDealNextCount > 0)
					DrawNextCardStatus(preDealNextRanks, preDealNextSuits, preDealNextCount);
			}

			// Session 14 addition -- Betting Advice, betting-phase ONLY
			// (user request). Session 13's original wiring ran this
			// post-deal (dealerHasCards required) -- by then the bet is
			// already locked in, so the "how should I size this bet"
			// readout was both too late to act on AND kept showing after
			// the phase it's named for had already ended. Session 14
			// moved the only call site here, gated on preDeal.valid,
			// which SimulatePreDeal()/the call site above already forces
			// false outside the true betting phase (Table.f_580==0 and an
			// untouched, freshly-shuffled deck -- see IsAtBettingPhase()'s
			// own header comment) -- so this can never fire post-deal.
			// Reuses the exact same deterministic preDeal data
			// ShowCardsBeforeBet's icon preview above already reads.
			// canDouble is hardcoded true here: the real bankroll>=bet
			// gate the post-deal Hit/Stand loop below computes isn't
			// meaningful yet at this phase since no bet has been placed
			// for the hand that hasn't been dealt.
			if (cfg.ShowBettingAdvice && preDeal.valid && mySeat >= 0 && mySeat < static_cast<std::int32_t>(kSeatCount) && preDeal.seatWillPlay[mySeat])
			{
				BlackjackHandEval::BettingConfidence preDealBettingConfidence = DetermineBettingAdvice(
					thread, preDeal.seatHands[mySeat], preDeal.dealerHand,
					preDeal.cursorAfterDeal, liveDeckCount, /*canDouble=*/true, isMySeatLastBeforeDealer,
					/*ownDrawsExact=*/!hasOccupiedLowerSeat);
				DrawBettingAdviceStatus(preDealBettingConfidence);
			}

			BlackjackHandEval::Action bestAction = BlackjackHandEval::Action::Stand;
			bool haveAdvice = false;
			// Session 15 (user bug report): the "Next cards" deck-ahead
			// preview was wrongly gated on `haveAdvice`, which only ever
			// got set inside the `cfg.ShowAdvice`-guarded loop below -- so
			// turning ShowAdvice off (wanting to hide ONLY the hit/stand/
			// double/split readout, per the user's own request to keep
			// making betting decisions unaided) silently killed the deck
			// prediction too, even though ShowDeckPrediction was still on.
			// haveValidHand tracks the cheap "is there a live, non-bust,
			// sub-21 hand to predict off of" fact unconditionally, so
			// ShowDeckPrediction's gate below no longer depends on whether
			// advice is being displayed at all -- only DetermineAdvice()
			// itself (the actual per-hand strategy computation) stays
			// behind cfg.ShowAdvice.
			bool haveValidHand = false;

			// Filled in by the loop below for my own seat (the insurance
			// window check after it needs them); -1 = not read this tick.
			std::int32_t myHandCount = -1;
			std::int32_t myCurrentHandIndex = -1;
			std::int32_t myFirstHandCardCount = -1;

			for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
			{
				std::uint32_t seatBase = kTableSlot + kSeatsBase + seat * kSeatStride;
				std::int32_t occupiedMarker = ReadInt(thread, seatBase + kSeatOccupiedOffset);
				if (occupiedMarker == -1)
					continue;

				std::int32_t handCount = ReadInt(thread, seatBase + kSeatHandCountOffset);
				if (handCount <= 0)
					continue;
				if (handCount > static_cast<std::int32_t>(kMaxHandsPerSeat))
					handCount = static_cast<std::int32_t>(kMaxHandsPerSeat); // defensive -- unconfirmed cap

				bool isMe = (static_cast<std::int32_t>(seat) == mySeat);
				if (!isMe || !dealerHasCards)
					continue;

				// After a split, only the hand actually being played gets
				// advice. The previous version advised every live hand in turn
				// and showed whichever came LAST -- i.e. hand 1's advice while
				// you were still playing hand 0. seat.f_3 is the game's own
				// "current hand" index (func_1063 compares it against f_59).
				//
				// Code-review fix: advice is shown ONLY while it's actually
				// my turn -- every occupied lower seat is done (see
				// lowerSeatsDone above) and f_3 points at one of my hands.
				// Before, f_3 == handCount (the "all my hands are done"
				// state) failed the range check and fell back to advising
				// the first live hand, so a hand I'd already stood on kept
				// showing advice while later seats drew; and while a lower
				// seat was still playing, advice assumed the next card was
				// mine when that seat was about to take it.
				std::int32_t currentHandIndex = ReadInt(thread, seatBase + kSeatCurrentHandIndexOffset);
				myCurrentHandIndex = currentHandIndex;
				myHandCount = handCount;
				const bool isMyTurn = lowerSeatsDone && currentHandIndex >= 0 && currentHandIndex < handCount;

				for (std::int32_t h = 0; h < handCount; h++)
				{
					std::uint32_t handSlot = seatBase + kSeatHandsOffset + static_cast<std::uint32_t>(h) * kHandStride;
					HandCards hand = ReadHand(thread, handSlot);
					if (hand.count <= 0)
						continue;

					if (h == 0)
						myFirstHandCardCount = hand.count;

					BlackjackHandEval::HandValue value = BlackjackHandEval::EvaluateHand(hand.ranks, hand.count);
					if (value.bust || value.total >= 21)
						continue;

					// "Next cards" is useful whenever I still hold a live
					// hand (it shows what's coming off the deck, whoever
					// draws it), so it isn't gated on whose turn it is --
					// only the per-hand advice below is.
					haveValidHand = true;

					if (!isMyTurn || h != currentHandIndex)
						continue;

					// A later split hand of MINE still draws from the deck
					// before the dealer does, exactly like a higher occupied
					// seat -- so the dealer simulation only holds for the
					// last of my hands (BlackjackDeckSim::EvaluateSplit()
					// already models its own first hand this way).
					const bool isLastBeforeDealer = isMySeatLastBeforeDealer && h == handCount - 1;

					// Session 10 live bug fix: canDouble previously
					// only checked card count, so advice would
					// recommend Double even when the player
					// couldn't actually afford it -- the game's own
					// legality gate also requires bankroll >= bet
					// (BlackjackHandEval.h's own header comment,
					// func_1237 case 4: `f_1 >= f_4[handIndex]`).
					// Both DetermineCheatAction() and
					// GetBasicStrategyAction() already demote
					// Double to Hit/Stand on their own once
					// canDouble is false. (Code-review fix: bet[h] is
					// now read past the array's size word -- see
					// kSeatBetOffset.)
					//
					// Code-review fix: Split puts up a second bet
					// equal to the first, so it needs the same
					// bankroll >= bet check -- canSplit used to check
					// only card count and the one-split cap.
					std::int32_t bankroll = ReadInt(thread, seatBase + kSeatBankrollOffset);
					std::int32_t bet = ReadInt(thread, seatBase + kSeatBetOffset + static_cast<std::uint32_t>(h));
					const bool canAffordSecondBet = bankroll >= bet;
					bool canDouble = (hand.count == 2) && canAffordSecondBet;
					bool canSplit = (hand.count == 2 && handCount < static_cast<std::int32_t>(kMaxHandsPerSeat)) && canAffordSecondBet;

					// A seat with 2 hands can only have gotten
					// there via exactly one split (kMaxHandsPerSeat
					// caps it there -- see file header comment).
					// Since split requires the original pair to
					// share the same RANK, if THIS hand's first
					// (non-drawn) card is an Ace, the pair that
					// was split must have been a pair of Aces --
					// see GetBasicStrategyAction()'s own header
					// comment for why that's enough to identify a
					// split-Ace hand without any dedicated
					// "how did this hand originate" flag existing
					// in the struct itself.
					bool isSplitAceHand = (handCount == static_cast<std::int32_t>(kMaxHandsPerSeat)) && hand.ranks[0] == 14;

					if (cfg.ShowAdvice && !haveAdvice)
					{
						bestAction = DetermineAdvice(thread, hand, dealerHand, liveDeckCursor, liveDeckCount, canDouble, canSplit, isSplitAceHand, isLastBeforeDealer); // Session 7 fourth/sixth addendum: deck-derived simulation (BlackjackDeckSim.h), not blind basic strategy -- see that function's own header comment. isMySeatLastBeforeDealer: Session 9 second live bug fix, see DetermineAdvice()'s own header comment
						haveAdvice = true;
					}
				}
			}

			if (cfg.ShowAdvice && haveAdvice)
				DrawAdviceStatus(bestAction);

			if (cfg.ShowDeckPrediction && haveValidHand)
			{
				constexpr std::int32_t kNextCardPreviewCount = 3;
				std::int32_t nextRanks[kNextCardPreviewCount];
				std::int32_t nextSuits[kNextCardPreviewCount];
				std::int32_t nextCount = 0;
				for (std::int32_t i = 0; i < kNextCardPreviewCount; i++)
				{
					std::int32_t idx = liveDeckCursor + i;
					if (idx < 0 || idx >= liveDeckCount)
						break;
					nextRanks[nextCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
					nextSuits[nextCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2 + 1);
					nextCount++;
				}
				if (nextCount > 0)
					DrawNextCardStatus(nextRanks, nextSuits, nextCount);
			}

			// Insurance is only ever a real decision when the dealer's up
			// card is an Ace. Session 7 fourth addendum: this used to be a
			// count-based deviation (a probability), but the dealer's real
			// HOLE card (dealerHand.ranks[0]) is already read every tick --
			// insurance is a bet that the dealer has blackjack, which is
			// now a CERTAINTY, not a guess: take it if and only if the
			// already-known hole card is worth 10 (10/J/Q/K), the exact
			// condition for dealer blackjack when the up card is an Ace.
			//
			// Code-review fix: shown only while the insurance decision can
			// still be pending, not for the whole round. Insurance is
			// offered right after the initial deal, before any hand
			// plays, so the window is over as soon as any card has been
			// drawn past the initial deal (2 per dealt seat + 2 for the
			// dealer -- the same deal shape SimulatePreDeal() uses and
			// PreDealCheck confirmed live) or my own seat's turn has
			// started. Insurance is state 2 of the table's state machine
			// (bjack_sp.ysc.c, the `f_2[1] == 14` branch), which runs
			// BEFORE state 4 moves any seat's f_3 from -1 to 0 -- so
			// during the prompt my f_3 still reads -1, never 0.
			bool insuranceWindowOpen = false;
			if (dealerHand.count == 2 && myHandCount == 1 && myCurrentHandIndex < 0 && myFirstHandCardCount == 2)
			{
				std::int32_t dealtSeats = 0;
				for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
				{
					std::uint32_t seatBase = kTableSlot + kSeatsBase + seat * kSeatStride;
					if (ReadInt(thread, seatBase + kSeatOccupiedOffset) != -1 && ReadInt(thread, seatBase + kSeatHandCountOffset) > 0)
						dealtSeats++;
				}
				insuranceWindowOpen = liveDeckCursor == dealtSeats * 2 + 2;
			}

			if (cfg.ShowAdvice && insuranceWindowOpen && dealerHand.ranks[1] == 14) // Session 8: folded into ShowAdvice, no separate toggle -- insurance IS advice. Session 7: ranks[1] is the real up card -- see PredictedHand's header comment above
				DrawInsuranceStatus(BlackjackHandEval::CardValue(dealerHand.ranks[0]) == 10);
		}
	}

	void OnTick()
	{
		if (!Enabled)
			return;

		DrawOverlay();
	}

#ifdef _DEBUG
	void ProbeTableStruct()
	{
		auto thread = GamePointers::FindScriptThread(rage::Joaat("bjack_sp"));
		if (!thread)
		{
			Log::Write("ProbeTableStruct: bjack_sp is not currently running");
			return;
		}

		Log::Write("ProbeTableStruct: bjack_sp thread found (id={}, stack=0x{:X}, stackSize={})",
			thread->m_Context.m_ThreadId,
			reinterpret_cast<unsigned long long>(thread->m_Stack),
			thread->m_Context.m_StackSize);

		std::int32_t roundState = ReadInt(thread, kTableSlot + kRoundStateOffset);
		std::int32_t roundResolvingState = ReadInt(thread, kTableSlot + kRoundResolvingOffset);
		std::int32_t roundPhaseState = ReadInt(thread, kTableSlot + kRoundPhaseOffset);
		Log::Write("ProbeTableStruct: round phase (f_701, slot {}, EMPIRICAL, see kRoundPhaseOffset's own comment) = {} (1=sat down, 2=waiting for bet, 3=bet placed/dealing, 7=a seat deciding, 8=dealer resolving/end of round); round state (f_579, slot {}, CONFIRMED LIVE) = {} (0/5); round resolving (f_580, slot {}, CONFIRMED LIVE) = {} (1/0)",
			kTableSlot + kRoundPhaseOffset, roundPhaseState, kTableSlot + kRoundStateOffset, roundState, kTableSlot + kRoundResolvingOffset, roundResolvingState);

		std::int32_t mySeatByF9 = ReadInt(thread, kMySeatSlot);
		std::int32_t mySeat = FindMySeatByPed(thread);
		Log::Write("ProbeTableStruct: mySeat candidates -- f_9 (slot {}, CONFIRMED LIVE) = {}, ped-array match (FindMySeatByPed, returned -1 live in Session 18 -- unreliable) = {}{}",
			kMySeatSlot, mySeatByF9, mySeat, (mySeatByF9 == mySeat) ? "  <-- AGREE" : "  <-- DISAGREE, worth re-checking against the real screen");

		Log::Write("ProbeTableStruct: seat ped handles (uLocal_14.f_1724+946, stride 46, slot base {}):", kPedSceneSlot + kSeatPedArrayOffset);
		for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
		{
			std::int32_t pedHandle = ReadInt(thread, kPedSceneSlot + kSeatPedArrayOffset + seat * kSeatPedStride);
			Log::Write("  seat {} ped handle = {}{}", seat, pedHandle, (static_cast<std::int32_t>(seat) == mySeat) ? "  <-- matches PLAYER::PLAYER_PED_ID()" : "");
		}

		HandCards dealerHand = ReadHand(thread, kTableSlot + kDealerHandOffset);
		std::string dealerStr = FormatHandCards(dealerHand);
		std::int32_t dealerValueField = ReadInt(thread, kTableSlot + kDealerHandOffset + kHandValueOffset);
		Log::Write("ProbeTableStruct: dealer hand (Table.f_2, slot {}) count={} cards=[ {}] rawValueField(f_24)={}",
			kTableSlot + kDealerHandOffset, dealerHand.count, dealerStr, dealerValueField);

		std::int32_t deckCursor = ReadInt(thread, kDeckSlot + kDeckCursorOffset);
		std::int32_t deckCount = ReadInt(thread, kDeckSlot + kDeckCountOffset);
		Log::Write("ProbeTableStruct: deck (Table.f_592, slot {}) cursor={} count={} (expect count=52 mid-round) -- next 4 undrawn cards:",
			kDeckSlot, deckCursor, deckCount);
		for (std::int32_t i = 0; i < 4; i++)
		{
			std::int32_t idx = deckCursor + i;
			if (idx < 0 || idx >= deckCount)
				break;
			std::int32_t rank = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
			std::int32_t suit = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2 + 1);
			Log::Write("  deck[{}]: {}{}", idx, RankName(rank), SuitLetter(suit));
		}

		Log::Write("ProbeTableStruct: seats (Table.f_27, base slot {}, stride {}, count {}):",
			kTableSlot + kSeatsBase, kSeatStride, kSeatCount);
		for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
		{
			std::uint32_t seatBase = kTableSlot + kSeatsBase + seat * kSeatStride;
			std::int32_t occupiedMarker = ReadInt(thread, seatBase + kSeatOccupiedOffset);
			std::int32_t handCount = ReadInt(thread, seatBase + kSeatHandCountOffset);
			std::int32_t currentHandIndex = ReadInt(thread, seatBase + kSeatCurrentHandIndexOffset);

			Log::Write("  seat {} (base slot {}): occupiedMarker(f_0)={} handCount(f_59)={} currentHandIndex(f_3, Session 5)={}{}{}",
				seat, seatBase, occupiedMarker, handCount, currentHandIndex,
				(currentHandIndex < 0) ? "  <-- waiting for its turn" : (currentHandIndex < handCount) ? "  <-- acting now" : "  <-- done acting (or unoccupied)",
				(static_cast<std::int32_t>(seat) == mySeat) ? "  <-- candidate YOUR SEAT" : "");

			if (occupiedMarker == -1)
				continue;

			for (std::uint32_t h = 0; h < kMaxHandsPerSeat; h++)
			{
				std::uint32_t handSlot = seatBase + kSeatHandsOffset + h * kHandStride;
				HandCards hand = ReadHand(thread, handSlot);
				std::string handStr = FormatHandCards(hand);
				std::int32_t rawValueField = ReadInt(thread, handSlot + kHandValueOffset);
				Log::Write("    hand {} (slot {}): count={} cards=[ {}] rawValueField(f_24)={}",
					h, handSlot, hand.count, handStr, rawValueField);
			}
		}

		Log::Write("ProbeTableStruct: raw window around kTableSlot (slot {}), offsets -4..+40, for re-deriving offsets if any of the above looks wrong:", kTableSlot);
		for (std::int32_t off = -4; off <= 40; off++)
		{
			std::int32_t value = ReadInt(thread, static_cast<std::uint32_t>(static_cast<std::int32_t>(kTableSlot) + off));
			Log::Write("  tableraw[{:+}] (slot {}) = {}", off, static_cast<std::int32_t>(kTableSlot) + off, value);
		}
	}

	void DumpLocalStackRange()
	{
		auto thread = GamePointers::FindScriptThread(rage::Joaat("bjack_sp"));
		if (!thread)
		{
			Log::Write("DumpLocalStackRange: bjack_sp is not currently running");
			return;
		}

		auto base = reinterpret_cast<std::uintptr_t>(thread->m_Stack);
		std::uint32_t stackSizeSlots = thread->m_Context.m_StackSize;
		std::uintptr_t end = base + static_cast<std::uintptr_t>(stackSizeSlots) * 8u;
		std::uintptr_t localBase = base + static_cast<std::uintptr_t>(kLocalStructIndex) * 8u;
		std::uintptr_t tableBase = base + static_cast<std::uintptr_t>(kTableSlot) * 8u;

		Log::Write("DumpLocalStackRange: start=0x{:X} end=0x{:X} (size={} slots, {} bytes)",
			static_cast<unsigned long long>(base), static_cast<unsigned long long>(end),
			stackSizeSlots, static_cast<unsigned long long>(end - base));
		Log::Write("DumpLocalStackRange: uLocal_14 (slot {}) starts at 0x{:X}, Table candidate (slot {}) starts at 0x{:X}",
			kLocalStructIndex, static_cast<unsigned long long>(localBase),
			kTableSlot, static_cast<unsigned long long>(tableBase));
	}

	void ProbeSeatHands()
	{
		auto thread = GamePointers::FindScriptThread(rage::Joaat("bjack_sp"));
		if (!thread)
		{
			Log::Write("ProbeSeatHands: bjack_sp is not currently running");
			return;
		}

		std::int32_t mySeat = FindMySeatByPed(thread);
		std::int32_t mySeatByF9 = ReadInt(thread, kMySeatSlot);
		Log::Write("ProbeSeatHands: mySeat (ped-array, unreliable -- -1 live in Session 18)={}, f_9 (CONFIRMED LIVE)={}{}",
			mySeat, mySeatByF9, (mySeat == mySeatByF9) ? "  <-- AGREE" : "  <-- DISAGREE");

		for (std::uint32_t seat = 0; seat < kSeatCount; seat++)
		{
			std::uint32_t seatBase = kTableSlot + kSeatsBase + seat * kSeatStride;
			std::int32_t occupiedMarker = ReadInt(thread, seatBase + kSeatOccupiedOffset);
			std::int32_t handCount = ReadInt(thread, seatBase + kSeatHandCountOffset);
			std::int32_t bankroll = ReadInt(thread, seatBase + kSeatBankrollOffset);

			if (occupiedMarker == -1)
			{
				Log::Write("  seat {}: unoccupied (occupiedMarker=-1)", seat);
				continue;
			}

			Log::Write("  seat {}: bankroll(f_1, candidate, slot {})={} -- sanity check: should look like a plausible in-game dollar amount",
				seat, seatBase + kSeatBankrollOffset, bankroll);

			for (std::int32_t h = 0; h < handCount && h < static_cast<std::int32_t>(kMaxHandsPerSeat); h++)
			{
				std::uint32_t handSlot = seatBase + kSeatHandsOffset + static_cast<std::uint32_t>(h) * kHandStride;
				HandCards hand = ReadHand(thread, handSlot);
				BlackjackHandEval::HandValue value = BlackjackHandEval::EvaluateHand(hand.ranks, hand.count);
				std::string handStr = FormatHandCards(hand);
				std::int32_t bet = ReadInt(thread, seatBase + kSeatBetOffset + static_cast<std::uint32_t>(h));
				std::int32_t betArraySize = ReadInt(thread, seatBase + kSeatBetArrayOffset);
				bool isSplitAceHand = (handCount == static_cast<std::int32_t>(kMaxHandsPerSeat)) && hand.count > 0 && hand.ranks[0] == 14;

				Log::Write("  seat {} hand {}: cards=[ {}] computedTotal={} soft={} bust={} blackjack={} bet(f_4[{}], slot {})={} betArraySizeWord(f_4, expect {})={} isSplitAceHand(candidate)={}{}",
					seat, h, handStr, value.total, value.soft, value.bust, value.blackjack, h, seatBase + kSeatBetOffset + static_cast<std::uint32_t>(h), bet,
					kMaxHandsPerSeat, betArraySize, isSplitAceHand,
					(static_cast<std::int32_t>(seat) == mySeat) ? "  <-- candidate YOUR SEAT" : "");
			}
		}
	}

	void ProbeDeckPrediction()
	{
		auto thread = GamePointers::FindScriptThread(rage::Joaat("bjack_sp"));
		if (!thread)
		{
			Log::Write("ProbeDeckPrediction: bjack_sp is not currently running");
			return;
		}

		HandCards dealerHand = ReadHand(thread, kTableSlot + kDealerHandOffset);
		if (dealerHand.count < 2)
		{
			Log::Write("ProbeDeckPrediction: dealer has fewer than 2 cards right now (count={}) -- run this again once a hand is dealt", dealerHand.count);
			return;
		}

		Log::Write("ProbeDeckPrediction: dealer hole card (real, ALREADY dealt but hidden on screen until reveal) = {}{}, up card (real, visible) = {}{} -- compare the hole card against the real screen once it flips over (Session 7: [0]=hole,[1]=up, the OPPOSITE of this file's original assumption -- live-confirmed by the user)",
			RankName(dealerHand.ranks[0]), SuitLetter(dealerHand.suits[0]),
			RankName(dealerHand.ranks[1]), SuitLetter(dealerHand.suits[1]));

		std::int32_t deckCursor = ReadInt(thread, kDeckSlot + kDeckCursorOffset);
		std::int32_t deckCount = ReadDeckCount(thread);
		PredictedHand predicted = SimulateDealerOutcome(thread, dealerHand, deckCursor, deckCount);
		BlackjackHandEval::HandValue predValue = BlackjackHandEval::EvaluateHand(predicted.ranks, predicted.totalCount);

		std::string predStr = FormatCardRun(predicted.ranks, predicted.suits, predicted.knownCount, predicted.totalCount);
		Log::Write("ProbeDeckPrediction: simulated dealer draw-out from cursor={} (count={}) -- predicted extra draws=[ {}] predicted final total={}{} (Session 5: this re-simulates from the LIVE cursor every tick, so it's exact once every occupied seat ahead of the dealer is done drawing -- see SimulateDealerOutcome()'s header comment; the round-end \"PredictionCheck\" log line separately validates a FROZEN round-start baseline every round, no F11 needed for that part)",
			deckCursor, deckCount, predStr, predValue.total, predValue.bust ? " BUST" : "");

		Log::Write("ProbeDeckPrediction: next 6 raw undrawn deck cards from cursor={} (whatever hand draws next, in whatever the real turn order is, gets these in order):", deckCursor);
		for (std::int32_t i = 0; i < 6; i++)
		{
			std::int32_t idx = deckCursor + i;
			if (idx < 0 || idx >= deckCount)
				break;
			std::int32_t rank = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
			std::int32_t suit = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2 + 1);
			Log::Write("  deck[{}]: {}{}", idx, RankName(rank), SuitLetter(suit));
		}
	}

	void DumpFullStackJsonl()
	{
		auto thread = GamePointers::FindScriptThread(rage::Joaat("bjack_sp"));
		if (!thread)
		{
			Log::Write("DumpFullStackJsonl: bjack_sp is not currently running");
			return;
		}

		// Timestamped so consecutive dumps (e.g. "before you hit" / "after
		// you hit") each land in their own file instead of the later one
		// clobbering the one a diff needs to compare against.
		SYSTEMTIME t;
		GetLocalTime(&t);
		std::ostringstream pathStream;
		pathStream << "BlackjackCheat_stackdump_"
			<< std::setfill('0')
			<< std::setw(4) << t.wYear << std::setw(2) << t.wMonth << std::setw(2) << t.wDay
			<< '_'
			<< std::setw(2) << t.wHour << std::setw(2) << t.wMinute << std::setw(2) << t.wSecond
			<< ".jsonl";
		std::string outPath = pathStream.str();

		if (GamePointers::DumpLocalStackJsonl(thread, outPath))
			Log::Write("DumpFullStackJsonl: wrote {} -- grep/jq it for a known real value (e.g. a visible card's rank/suit, a bankroll amount) to find where it actually lives, then diff against a prior dump's file to see what actually changed", outPath);
		else
			Log::Write("DumpFullStackJsonl: failed, see prior log line for why");
	}
#endif // _DEBUG
}
