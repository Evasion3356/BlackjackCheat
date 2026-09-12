# Changelog

All notable user-facing changes to BlackjackCheat are recorded here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/); this
tracks what an end user experiences, not internal implementation history
(see `JOURNAL.md` for the full session-by-session derivation/bugfix log).

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
