# Changelog

All notable user-facing changes to BlackjackCheat are recorded here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/); this
tracks what an end user experiences, not internal implementation history
(see `JOURNAL.md` for the full session-by-session derivation/bugfix log).

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
