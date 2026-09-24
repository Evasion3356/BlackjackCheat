# Changelog

All notable user-facing changes to BlackjackCheat are recorded here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/); this
tracks what an end user experiences, not internal implementation history
(see `JOURNAL.md` for the full session-by-session derivation/bugfix log).

## [1.5.0] - 2026-09-24

### Changed
- Betting advice is now BET MAX or BET MIN with the amount to bet and
  what the round will win, e.g. "BET MAX $2.94 (+$5.88)". Since the mod
  plays out the whole round from the deck, the result is known before
  you bet, so Low/Medium/High no longer meant anything. Worse, Medium
  was usually a Double win, which pays twice a normal win, and got bet
  small. When the winning play doubles or splits, the amount is half
  your bankroll (up to the table max), so you can still afford it.
  Betting advice now also accounts for splits and the 3:2 blackjack payout.

### Fixed
- The mod now plays out the other (AI) seats with the game's own
  decision table, so hit/stand/double/split and betting advice use the
  exact deck even when other seats draw before or after you. Before, any
  other seat drawing made it fall back to textbook strategy or a rough
  betting estimate (e.g. a sure loss showed Medium).
- No more Double advice on a split hand. The game doesn't offer Double
  after a split.
- Advice now only appears while it's actually your turn. Before, it showed
  while a seat before yours was still playing (assuming the next card was
  yours when that seat was about to take it), and it kept showing for a
  hand you had already stood on.
- Double and Split advice now check your real bet against your bankroll.
  The bet was being read from the wrong memory slot, so the check never
  blocked anything.
- Insurance advice now only shows while you still have to answer the
  insurance prompt, not for the whole round.
- In an install where the game folder isn't writable, the editable
  settings copy in `%LOCALAPPDATA%\RDR2ASIMods\` is created again even
  when the shipped INI is already complete.
- Better advice when another seat still has to act: the mod now uses
  every card you'll draw, not just the next one (e.g. soft 16 with a 6
  then a 5 coming is now Hit, since it reaches 21).
- Split advice now counts a doubled hand as two bets, so it no longer
  passes up splits that win by doubling.
- Basic-strategy fixes: soft 18 against a dealer 3-6 now says Stand when
  you can't double (it said Hit), and an A,A you can't split now says Hit
  (it could say Double).
- Reloading the settings no longer deletes the INI's `[HUD]` section in
  the Release build, and no longer rewrites the file (losing your
  comments) when nothing in it needs changing.

## [1.4.0] - 2026-09-23

### Fixed
- The game no longer crashes on the loading screen when
  `BlackjackCheat.log` can't be written. This was the 1.3.0 load-screen
  crash: the mod failed to create its log during game load and took the
  game down with it, before any log existed.
- If the game folder can't be written (e.g. a `C:\Program Files` install,
  or a read-only/locked log file), the log now goes to
  `%LOCALAPPDATA%\RDR2ASIMods\BlackjackCheat.log` instead, and its first line
  names the path that couldn't be used.
- After a split, advice is now shown for the hand you're actually playing.
  Previously it could show the other hand's advice.
- While a later split hand of yours still has to be played, advice no
  longer assumes the next cards go straight to the dealer. That hand draws
  first, so the dealer prediction wasn't reliable yet.
- Naturals are scored correctly in advice and betting confidence: a
  dealer blackjack beats a three-card 21, a player blackjack beats a
  dealer's three-card 21, and a two-card 21 after a split is not a
  blackjack.
- Loads reliably even when an ASI loader injects the mod before RDR2 has
  finished unpacking itself. Startup work moved out of the DLL's load
  callback, and a failed memory scan is retried instead of leaving the mod
  inactive for the whole session.
- Settings work in a game folder that can't be written, too:
  `BlackjackCheat.ini` is then saved to `%LOCALAPPDATA%\RDR2ASIMods\BlackjackCheat.ini`
  (starting from the game folder's copy, if there is one) instead of the
  mod being stuck on default settings.

### Changed
- Default HUD positions of the advice line and your hand's card icons
  adjusted.
- The Release HUD no longer allocates memory every frame.

## [1.3.0] - 2026-09-13

### Added
- HUD advice/betting/insurance/next-cards labels are now localized into
  RDR2's 13 shipped languages, auto-detected from the game's own UI
  language. `BlackjackCheat.ini`'s new `[General]` `Language` key
  ("auto" by default) can force a different language if wanted.
  Translated terms were checked against real blackjack rules/casino
  glossary sources per language rather than machine-translated blind;
  two wrong terms (German and Russian "Stand") were caught and
  corrected this way. The two HUD concepts unique to this mod (the
  "BET LOW/MEDIUM/HIGH" readout and the "Next cards:" label) have no
  real casino-terminology equivalent to check against, so those --
  along with every translation here -- are still not reviewed by a
  native speaker.

## [1.2.0] - 2026-09-13

### Fixed
- Dealer hole-card icon no longer vanishes mid-round at some tables
  (reported at Van Horn, not reproducible at Rhodes) -- it now stays
  visible through the dealer's own reveal instead of dropping out the
  instant the script resets for the next round, using a newly-traced
  round-phase field.
- Pre-deal predictions no longer briefly show right after sitting down,
  before betting has actually opened.
- Advice engine no longer recommends Hit on a soft hand when the only
  known next card would downgrade it to a strictly worse (but
  non-busting) total -- e.g. soft 18 hitting into a known 5 for a hard
  13.
- "Next cards" preview no longer disappears when `ShowAdvice` is turned
  off; it now honors `ShowDeckPrediction` independently, as the two
  toggles were always meant to.

## [1.1.0] - 2026-09-11

### Added
- Betting advice readout (Low/Medium/High bet-sizing recommendation),
  shown during the betting phase itself so it's actionable before the
  bet is locked in.

### Fixed
- Release: the advisor could get silently disabled when ScriptHookRDR2
  re-entered `ScriptMain` on script restart, since the initial enable
  used a non-idempotent toggle.
- Advice engine: fixed cases recommending Hit into a certain bust, Stand
  instead of a known-winning Double, and Split being refused when deck-
  known upcoming cards made splitting clearly correct. Double is no
  longer recommended when the player can't cover the doubled bet.

## [1.0.0] - 2026-09-11

Initial public release.

### Added
- Deterministic deck-ahead prediction for `bjack_sp`'s single-player
  blackjack: the dealer's hole card is read directly (it's real,
  already-dealt data, just hidden on screen) and the dealer's forced
  stand-on-17 draw-out is simulated straight off the game's own
  already-shuffled deck array. The prediction re-simulates from the live
  deck cursor every tick, so it's exact by the time the dealer's real turn
  begins.
- Fully deck-derived hit/stand/double advice: instead of a textbook basic
  strategy chart, the current hand's exact future cards are known in
  advance, so every legal stopping point is simulated against the
  predicted dealer outcome and whichever one actually wins is
  recommended.
- Split/no-split advice from a basic-strategy pair chart, including
  correct handling of split Aces (one forced card each, no further
  hit/stand/double).
- Insurance Yes/No recommendation whenever the dealer's up card is an
  Ace.
- On-screen HUD: dealer hole-card icon, a "next cards" preview strip, and
  a colored hit/stand/double/split/insurance readout.
- `BlackjackCheat.ini` with independent toggles (`ShowDealerHand`,
  `ShowAdvice`, `ShowDeckPrediction`) under `[General]`, auto-created on
  first run.
- Zero-setup release build: the advisor is enabled automatically on
  injection, no menu or keybind required.
- Read-only by design: no game-memory writes, no native game-state
  manipulation -- it only reads and displays data the game has already
  dealt.

### Notes
- Targets game build **1491.50** specifically -- struct offsets are
  hardcoded from that build's decompiled scripts and are not guaranteed
  to hold on any other build.
- Single-player `bjack_sp` only.
