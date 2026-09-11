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
	  matters for card counting (see BlackjackCardCounting.h's own header
	  comment) and means BlackjackHandEval.h's basic-strategy chart, which
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
	+Debug), with card counting (BlackjackCardCounting.h) demoted to
	secondary/Debug-only, since direct reading is strictly better
	information than estimating from a count once the deck itself is
	readable. UpdateDeckPrediction() also self-validates the dealer
	draw-out prediction automatically every round via a "PredictionCheck"
	log line (Debug-only), the same technique PokerCheat's own predicted
	board used -- this is the most valuable live-testing signal for
	confirming kDeckSlot/kDeckCursorOffset are actually right, and needs no
	F10 interaction at all, just normal play with the Debug build running.

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
	  wired to a new F10 "Dump Full Stack JSONL" item) that dumps EVERY
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
	no longer logs a count line. The header/tests project
	(BlackjackCardCounting.h, tests/BlackjackCardCountingTests.vcxproj)
	are left on disk unused rather than deleted outright (this project
	folder has no version control, see environment notes, so deleting
	files here has no undo) -- say so explicitly if you also want those
	removed from disk/the solution.
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
#include "script.h"

#include <string>
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

	constexpr std::uint32_t kMySeatSlot = kLocalStructIndex + 9; // uLocal_14.f_9 -- HIGH confidence (Session 5, via func_597's real "is this the human seat" predicate -- see file header comment), SECONDARY candidate only -- see FindMySeatByPed() below for the primary method (Session 3)

	// uLocal_14.f_1724 -- sibling "ped/scene" struct to Table, same role
	// as poker_sp's own f_3310 -- HIGH confidence (Session 3, see file
	// header comment). f_946[seat] (stride 46) is that seat's live Ped
	// handle, offset+0 of the stride.
	constexpr std::uint32_t kPedSceneFieldOffset = 1724;
	constexpr std::uint32_t kPedSceneSlot = kLocalStructIndex + kPedSceneFieldOffset;
	constexpr std::uint32_t kSeatPedArrayOffset = 946;
	constexpr std::uint32_t kSeatPedStride = 46;

	constexpr std::uint32_t kDealerHandOffset = 2;    // Table.f_2 -- MEDIUM-HIGH confidence

	constexpr std::uint32_t kSeatsBase = 27;          // Table.f_27 -- MEDIUM-HIGH confidence
	constexpr std::uint32_t kSeatStride = 60;
	constexpr std::uint32_t kSeatCount = 4;           // CONFIRMED via func_280's direct `iParam1 < 4` bounds check -- HIGH confidence (Session 2)
	constexpr std::uint32_t kSeatOccupiedOffset = 0;  // != -1 means occupied -- confirmed (func_116)
	constexpr std::uint32_t kSeatHandsOffset = 10;    // seat.f_10[hand] (stride 25) -- CONFIRMED LIVE (Session 6, corrected from the original 8 static-trace guess -- verified against all 4 real seats' cards simultaneously, see docs/JOURNAL.md)
	constexpr std::uint32_t kSeatHandCountOffset = 59; // seat.f_59 -- HIGH confidence again (Session 7 second addendum): re-confirmed live via a raw stack dump matching the real screen (read 1 for a genuinely unsplit hand while a stale/leftover hand-1 struct sat right next to it) -- see docs/JOURNAL.md. Briefly distrusted and replaced with a raw-scan-derived count earlier in Session 7; that replacement was itself wrong (see kHandCountOffset's comment below) and has been reverted.
	constexpr std::uint32_t kMaxHandsPerSeat = 2;     // CONFIRMED cap via func_1237 case 6 (`f_59 > 1` blocks split) -- HIGH confidence (Session 2)
	constexpr std::uint32_t kSeatBankrollOffset = 1;  // seat.f_1 -- MEDIUM confidence (Session 2), not read by OnTick(), Probe-only
	constexpr std::uint32_t kSeatBetOffset = 4;       // seat.f_4[handIndex] -- MEDIUM confidence (Session 2), not read by OnTick(), Probe-only
	constexpr std::uint32_t kSeatCurrentHandIndexOffset = 3; // seat.f_3 -- HIGH confidence (Session 5, f_N=offset+N convention + func_1063's direct f_3<f_59 comparison), not read by OnTick(), Probe-only

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

	namespace
	{
		std::int32_t ReadInt(rage::scrThread* thread, std::uint32_t slot)
		{
			void* raw = GamePointers::ReadScriptLocal(thread, slot);
			return static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(raw));
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

		const char* ActionName(BlackjackHandEval::Action action)
		{
			switch (action)
			{
				case BlackjackHandEval::Action::Hit: return "HIT";
				case BlackjackHandEval::Action::Stand: return "STAND";
				case BlackjackHandEval::Action::Double: return "DOUBLE";
				case BlackjackHandEval::Action::Split: return "SPLIT";
				default: return "?";
			}
		}

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
		std::string WrapBgFormatText(const std::string& label, int fontSize)
		{
			std::ostringstream oss;
			oss << "<TEXTFORMAT RIGHTMARGIN='0'><P ALIGN='Left'><FONT FACE='$Font5' LETTERSPACING='0' SIZE='"
				<< fontSize << "'>~s~" << label << "</FONT></P><TEXTFORMAT>";
			return oss.str();
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
		// Caveat this inherits from BlackjackDeckSim::SimulateDealerFromRanks()
		// and does NOT solve: if another occupied seat still has to act
		// between this hand and the dealer's turn, their real hits will
		// shift the cursor by an amount this function can't know in
		// advance (this project deliberately never ported func_623's own
		// ~1860-line AI decision table, see the file header's Session 5
		// addendum) -- so the outcome this simulates is exact once this is
		// the last seat left to act before the dealer, and a "what if
		// nobody else draws" provisional guess otherwise, exactly like the
		// HUD's own "Predicted dealer draws" line already is.
		constexpr std::int32_t kFutureLookahead = 32; // generous bound: worst case is roughly two hands' worth of draws (this hand's own hits + the dealer's), each capped at kHandMaxCards

		BlackjackHandEval::Action DetermineAdvice(rage::scrThread* thread, const HandCards& playerHand, const HandCards& dealerHand,
			std::int32_t deckCursor, std::int32_t deckCount, bool canDouble, bool canSplit, bool isSplitAceHand)
		{
			if (isSplitAceHand)
				return BlackjackHandEval::Action::Stand; // forced by the game itself, see BlackjackHandEval.h's own header comment

			// Split is still the textbook pair chart (BlackjackHandEval.h)
			// -- fully deck-simulating BOTH resulting hands' own draw-outs
			// plus the dealer's is a much bigger expansion than this pass
			// attempts; flagged as a known scope limit, not a silent gap.
			BlackjackHandEval::Action basicSuggestion = BlackjackHandEval::GetBasicStrategyAction(
				playerHand.ranks, playerHand.count, dealerHand.ranks[1], canDouble, canSplit, false);
			if (basicSuggestion == BlackjackHandEval::Action::Split)
				return BlackjackHandEval::Action::Split;

			std::int32_t futureRanks[kFutureLookahead];
			std::int32_t futureCount = 0;
			for (; futureCount < kFutureLookahead; futureCount++)
			{
				std::int32_t idx = deckCursor + futureCount;
				if (idx < 0 || idx >= deckCount)
					break;
				futureRanks[futureCount] = ReadInt(thread, kDeckSlot + kDeckCardsBaseOffset + static_cast<std::uint32_t>(idx) * 2);
			}

			return BlackjackDeckSim::DetermineCheatAction(playerHand.ranks, playerHand.count, dealerHand.ranks, dealerHand.count,
				futureRanks, futureCount, canDouble, isSplitAceHand);
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
		// normal play with no F10 interaction needed. This is the single
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
				std::int32_t deckCount = ReadInt(thread, kDeckSlot + kDeckCountOffset);
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
		constexpr float kReleaseAdviceX = 0.48f;
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

			std::string formatText = WrapBgFormatText(ActionName(action), 40);

			UIDEBUG::_BG_SET_TEXT_COLOR(r, g, b, 255);
			UIDEBUG::_BG_DISPLAY_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(formatText.c_str())), adviceX, adviceY);
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
			const char* label = takeInsurance ? "Insurance: YES" : "Insurance: No";
			int r = takeInsurance ? 180 : 200, g = takeInsurance ? 255 : 200, b = takeInsurance ? 180 : 200;

			std::string formatText = WrapBgFormatText(label, 26);

			UIDEBUG::_BG_SET_TEXT_COLOR(r, g, b, 255);
			UIDEBUG::_BG_DISPLAY_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(formatText.c_str())), x, y);
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
			constexpr float x = 0.957f; // user-confirmed via live Reload Config tuning (0.821 initial guess -> 0.957)
			constexpr float y = 0.078f;
			constexpr float width = 0.03f;
			constexpr float height = 0.075f;
#endif

			std::string textureName = BuildCardTextureName(rank, suit);

			GRAPHICS::DRAW_SPRITE(const_cast<char*>(cardSetDict.c_str()), const_cast<char*>(textureName.c_str()),
				x, y, width, height, 0.0f, 255, 255, 255, kHoleCardIconAlpha, 0);
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
			constexpr float baseX = 0.55f; // user-confirmed via live Reload Config tuning (0.62 initial guess -> 0.55)
			constexpr float y = 0.59f;
			constexpr float spacingX = 0.03f;
			constexpr float width = 0.025f;
			constexpr float height = 0.06f;
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
			std::string formatText = WrapBgFormatText("Next cards:", 26);

			UIDEBUG::_BG_SET_TEXT_COLOR(180, 255, 220, 255);
			UIDEBUG::_BG_DISPLAY_TEXT(GAMEPLAY::CREATE_STRING(10, const_cast<char*>("LITERAL_STRING"), const_cast<char*>(formatText.c_str())), x, y);

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

			HandCards dealerHand = ReadHand(thread, kTableSlot + kDealerHandOffset);
			bool dealerHasCards = dealerHand.count > 0;

			UpdateDeckPrediction(thread, dealerHand, dealerHasCards);

			// Read once, reused by the cheat-action simulation, the "Next
			// card" line, and insurance below -- all three need the exact
			// same live cursor/count snapshot to stay consistent with each
			// other within a single tick.
			std::int32_t liveDeckCursor = ReadInt(thread, kDeckSlot + kDeckCursorOffset);
			std::int32_t liveDeckCount = ReadInt(thread, kDeckSlot + kDeckCountOffset);

			const Config::Values& cfg = Config::Get();

#ifdef _DEBUG
			float x = cfg.PanelX;
			float y = cfg.PanelY;
			constexpr float kLineHeight = 0.028f;
			constexpr float kPanelPadding = 0.012f;
			constexpr int kMaxLines = 10; // title + dealer + predicted draws + count + up to 4 seats*2 hands, generously

			DrawPanel(x - kPanelPadding, y - kPanelPadding,
				0.36f + kPanelPadding * 2.0f,
				kMaxLines * kLineHeight + kPanelPadding * 2.0f);

			DrawLine(x, y, "BlackjackCheat (UNCONFIRMED offsets -- see docs/JOURNAL.md)", true);
			y += kLineHeight;

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

#endif

			// Release+Debug: the dealer's real hole card, exact data read
			// straight from the already-dealt hand struct (see
			// PredictedHand's header comment) -- drawn as a card-face icon
			// top-right, see DrawDealerHoleCardIcon()'s own header comment.
			if (cfg.ShowDeckPrediction && dealerHand.count >= 2)
				DrawDealerHoleCardIcon(dealerHand.ranks[0], dealerHand.suits[0]); // Session 7: index 0 is the real hole card, not index 1 -- see PredictedHand's header comment above

			BlackjackHandEval::Action bestAction = BlackjackHandEval::Action::Stand;
			bool haveAdvice = false;

			if (cfg.ShowAdvice)
			{
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

					for (std::int32_t h = 0; h < handCount; h++)
					{
						std::uint32_t handSlot = seatBase + kSeatHandsOffset + static_cast<std::uint32_t>(h) * kHandStride;
						HandCards hand = ReadHand(thread, handSlot);
						if (hand.count <= 0)
							continue;

						BlackjackHandEval::HandValue value = BlackjackHandEval::EvaluateHand(hand.ranks, hand.count);

						// Advice is only computed/shown for the local
						// player's own hand(s) -- there's no reason to
						// recommend a play for an AI opponent's cards, and
						// per-hand action requires knowing which hand is
						// actually "up" (not tracked here), so this always
						// evaluates every one of the player's hands and
						// shows whichever is currently NOT a bust/21, same
						// simplification PokerCheat's single "your hand"
						// assumption made.
						if (isMe && dealerHasCards && !value.bust && value.total < 21)
						{
							bool canDouble = (hand.count == 2);
							bool canSplit = (hand.count == 2 && handCount < static_cast<std::int32_t>(kMaxHandsPerSeat));

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

							bestAction = DetermineAdvice(thread, hand, dealerHand, liveDeckCursor, liveDeckCount, canDouble, canSplit, isSplitAceHand); // Session 7 fourth/sixth addendum: deck-derived simulation (BlackjackDeckSim.h), not blind basic strategy -- see that function's own header comment
							haveAdvice = true;
						}
					}
				}
			}

			if (cfg.ShowAdvice && haveAdvice)
				DrawAdviceStatus(bestAction);

			if (cfg.ShowDeckPrediction && haveAdvice)
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
			if (cfg.ShowAdvice && dealerHand.count >= 2 && dealerHand.ranks[1] == 14) // Session 8: folded into ShowAdvice, no separate toggle -- insurance IS advice. Session 7: ranks[1] is the real up card -- see PredictedHand's header comment above
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

		std::int32_t mySeatByF9 = ReadInt(thread, kMySeatSlot);
		std::int32_t mySeat = FindMySeatByPed(thread);
		Log::Write("ProbeTableStruct: mySeat candidates -- f_9 (slot {}, SECONDARY, MEDIUM confidence) = {}, ped-array match (PRIMARY, HIGH confidence, see FindMySeatByPed) = {}{}",
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
				(currentHandIndex < handCount) ? "  <-- still acting this round" : "  <-- done acting (or unoccupied)",
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
		Log::Write("ProbeSeatHands: mySeat (ped-array, PRIMARY)={}, f_9 (SECONDARY)={}{}",
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
				bool isSplitAceHand = (handCount == static_cast<std::int32_t>(kMaxHandsPerSeat)) && hand.count > 0 && hand.ranks[0] == 14;

				Log::Write("  seat {} hand {}: cards=[ {}] computedTotal={} soft={} bust={} blackjack={} bet(f_4[{}], candidate)={} isSplitAceHand(candidate)={}{}",
					seat, h, handStr, value.total, value.soft, value.bust, value.blackjack, h, bet, isSplitAceHand,
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
		std::int32_t deckCount = ReadInt(thread, kDeckSlot + kDeckCountOffset);
		PredictedHand predicted = SimulateDealerOutcome(thread, dealerHand, deckCursor, deckCount);
		BlackjackHandEval::HandValue predValue = BlackjackHandEval::EvaluateHand(predicted.ranks, predicted.totalCount);

		std::string predStr = FormatCardRun(predicted.ranks, predicted.suits, predicted.knownCount, predicted.totalCount);
		Log::Write("ProbeDeckPrediction: simulated dealer draw-out from cursor={} (count={}) -- predicted extra draws=[ {}] predicted final total={}{} (Session 5: this re-simulates from the LIVE cursor every tick, so it's exact once every occupied seat ahead of the dealer is done drawing -- see SimulateDealerOutcome()'s header comment; the round-end \"PredictionCheck\" log line separately validates a FROZEN round-start baseline every round, no F10 needed for that part)",
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

		if (GamePointers::DumpLocalStackJsonl(thread, outPath.c_str()))
			Log::Write("DumpFullStackJsonl: wrote {} -- grep/jq it for a known real value (e.g. a visible card's rank/suit, a bankroll amount) to find where it actually lives, then diff against a prior dump's file to see what actually changed", outPath);
		else
			Log::Write("DumpFullStackJsonl: failed, see prior log line for why");
	}
#endif // _DEBUG
}
