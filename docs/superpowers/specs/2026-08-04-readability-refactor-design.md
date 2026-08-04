# Readability Refactor — Design Spec
*2026-08-04*

## Context

Talat (C++20 matching engine + ftxui TUI) is a systems/quant interview portfolio piece. The engine and TUI are functionally complete (see `2026-03-30-tui-pivot-design.md`), but the project isn't yet optimized for how a recruiter or interviewer actually reads it: files are organized flat rather than by subsystem, headers lack any top-level "what/why" documentation, the README front-loads deep implementation prose before showing how to run the thing, and both the README and profiling write-up read as clearly AI-generated (heavy mid-sentence bold, "this design means..." transitions, repetitive parallel structure).

This is phase 1 of a three-phase effort:
1. **This spec** — readability: directory reorg, naming pass, header docs, test coverage gaps, README/DESIGN split and rewrite, small TUI command additions.
2. *(Later, separate spec)* — easy performance wins, prioritizing external-library swaps (e.g. faster hash map for hot-path lookups in `OrderBook`/`OrderMatcher`).
3. *(Later, separate spec, scope TBD after 1+2 land)* — a portfolio-worthy extension (candidates discussed: multi-instrument support, persistence/replay, observability polish — not decided yet).

Feature-cut review: engine order types (Limit/Market/Stop-Limit/Stop-Market, GTC/IOC/FOK), self-match prevention, and the three trader archetypes were all reviewed as candidates for removal. None are cut — they're the substantive answers to the project's own `questions.txt` and directly demoable. The only gap found was that Stop orders and FOK aren't reachable from the TUI command bar despite being fully implemented and tested at the engine level; this spec closes that gap instead of cutting the feature.

## Goals

- Reorganize `include/` and `src/` by subsystem so the layout itself communicates the architecture.
- Add header-level doc comments to every major class (what it is, why it exists, key invariant) — not exhaustive inline commentary.
- Close two real gaps: no test coverage for FOK (fill-or-kill) edge cases, and Stop/FOK orders unreachable from the TUI.
- Rewrite README for recruiter skim (quickstart first, deep dive split out) and eliminate AI-generated writing tells throughout.
- Update `CLAUDE.md` to match the new layout so it stays authoritative.

## Non-goals

- Any performance optimization (allocator swaps, hash map swaps, seqlocks) — deferred to phase 2.
- New features beyond exposing existing Stop/FOK support in the TUI — deferred to phase 3.
- A full test-coverage audit beyond the identified gap (FOK edge cases) plus tests for the new TUI commands.

## Directory Restructure

New layout, mirrored between `include/` and `src/`:

```
include/
  core/       order.h                              (POD order/side/TIF types, zero deps on the rest)
  engine/     order_book.h, exchange_events.h, order_matcher.h, event_bus.h,
              exchange.h, portfolio.h, stats_tracker.h
  market/     latent_price.h, trader.h, trader_registry.h,
              traders/market_maker.h, informed_trader.h, noise_trader.h
  tui/        tui.h                                (unchanged)
src/
  engine/     event_bus.cpp, exchange.cpp, order_book.cpp,
              order_matcher.cpp, portfolio.cpp
  market/     trader.cpp, trader_registry.cpp,
              traders/market_maker.cpp, informed_trader.cpp, noise_trader.cpp
  tui/        tui.cpp                               (unchanged)
  main.cpp                                          (stays at src/ root)
```

Rationale: `core` = pure data, zero behavior, zero dependencies on other subsystems. `engine` = the exchange itself (matching, book, portfolio accounting, events, stats). `market` = the simulated ecosystem that trades against the engine (traders + the latent fair-value process). This grouping matches how the architecture would be narrated out loud.

Mechanical fallout, all required for the build to keep working:
- Every `#include "x.h"` across `include/`, `src/`, and `tests/cpp/` updated to the new subpath (e.g. `#include "order.h"` → `#include "core/order.h"`; test files use `#include "include/core/order.h"` per the existing repo-root include convention).
- `CMakeLists.txt` `CORE_SOURCES` list updated to new `src/` paths.
- `CLAUDE.md` "Include Path Convention" and architecture description updated to match.

Naming: no separate renaming pass. Public API names reviewed during spec work and are already reasonably clear (`OrderMatcher`, `submit_order`, `best_bid`, etc.) — fix any awkward internal names opportunistically while touching a file for the move, but this is not a dedicated task.

## Header-Level Documentation

Add a short block comment (2-5 lines) above each major class declaration: what it is, why it exists, one key invariant or constraint if non-obvious. Applies to: `Exchange`, `OrderMatcher`, `OrderBook`, `EventBus`, `TraderRegistry`, `LatentPrice`, `Portfolio`, `StatsTracker`, `Trader` and the three trader subclasses.

Not in scope: exhaustive inline WHY comments beyond what already exists. Existing subtle-logic comments (snapshot-before-callback, self-match prevention, acquire/release ordering in `LatentPrice`) may get a small clarity pass but aren't being expanded into essays — that level of explanation belongs in `docs/DESIGN.md`, not the code.

## TUI: Expose Stop Orders and FOK

Add command-bar syntax for the two engine features that exist but aren't reachable, following the existing verb-first `buy`/`sell` pattern:
- `buy <qty> stop <stop_price>` / `sell <qty> stop <stop_price>` → submits a `StopMarketOrder`
- `buy <qty> stop <stop_price> @ <limit_price>` / `sell <qty> stop <stop_price> @ <limit_price>` → submits a `StopLimitOrder`
- `buy <qty> @ <price> fok` / `sell <qty> @ <price> fok` → submits a limit order with `TimeInForce::FOK` instead of the default `GTC`

Update the in-TUI `/help` text and README Commands section to document the new syntax.

## FOK Correctness Fix

While scoping FOK test coverage, found `try_match_limit` (`src/order_matcher.cpp:66-133`) treats FOK identically to IOC: it walks the book and emits real `FillEvent`s incrementally, only deciding afterward not to rest the unfilled remainder. That's IOC semantics, not FOK — a FOK order that the book can't fully satisfy should reject with **zero** fills, not partially execute. This is a correctness bug in a feature the project's own `questions.txt` (Q10) asks you to explain.

Fix: before walking the book for a FOK taker, do a read-only pass summing available quantity at qualifying price levels (using the existing `total_qty` already returned per-level by `for_each_bid`/`for_each_ask`, no need to inspect individual orders) and compare against `taker.qty`. If insufficient, publish `OrderRejectedEvent{taker.id, taker.trader_id, "insufficient_liquidity_fok"}` and return before any fills execute. If sufficient, fall through to the existing walk — since liquidity is now guaranteed sufficient, the walk will fully consume the order, so no other branch of the existing fill/rest/reject logic needs to change. Safe against concurrent mutation because the matcher is single-threaded and no other writer touches the book between the two passes within one command's processing.

## Test Coverage Additions

Current gaps identified:
- Self-match prevention already has coverage (`tests/cpp/test_order_matcher.cpp:78`, `SelfMatchPrevented`) — no gap here, confirmed during planning.
- **FOK (fill-or-kill) edge cases** are untested anywhere in the suite, and exercise the bug above. Add tests covering full fill, insufficient-liquidity-so-zero-fills (the case that currently fails before the fix), and exact-liquidity full fill.
- **New TUI commands** (Stop-Limit, Stop-Market, FOK via command bar) get parser/dispatch tests once implemented.
- Delete `tests/cpp/placeholder_test.cpp` — dead stub (`TEST(Placeholder, AlwaysPasses)`), superseded by the real suites.

Not in scope: a broader audit of every branch in every file (e.g. EventBus unsubscribe-during-publish) — explicitly deferred per user decision, scope stays tied to the gaps above.

## README and DESIGN.md Split

**Problem with current README**: Technical Design and Profiling Analysis sections (~85 lines) read as AI-generated — mid-sentence bold-as-emphasis-spam (`**moodycamel ConcurrentQueue**`, `**shared_mutex**`), "this design means..." explanatory transitions, identical parallel structure repeated per trader/section, tables used as a formatting crutch rather than for genuinely tabular data. They also bury the quickstart (Build & Run, Commands) below all of that, which is backwards for a recruiter skim.

**New README shape** (top to bottom):
1. Title + tagline + TUI screenshot
2. Requirements + Build & Run
3. Commands (including new Stop/FOK syntax)
4. Architecture — trimmed version of what exists today: component tree, order types line, trader behavior table (~30 lines, this part of the current README is already terse and mostly survives as-is)
5. Performance — benchmark table only (throughput/latency numbers), no profiling essay
6. License
7. One line linking to `docs/DESIGN.md` for the deep dive

**New `docs/DESIGN.md`**: Matching Engine, Order Book, EventBus, Latent Price, Traders, Portfolio & PnL, StatsTracker, Fixed-Point Arithmetic, Profiling Analysis — same technical content as today's README sections, rewritten in a plainer voice:
- Bold reserved for genuinely scannable labels (table headers, command/type names on their own), not sprinkled mid-sentence for emphasis.
- No "this design means..." / "this is why..." transition formula — state the fact, let it stand.
- Vary sentence shape per section instead of repeating the same what→formula→why template for every trader.
- Tables only where the content is actually tabular (benchmark numbers, syscall breakdown) — prose elsewhere, including the PnL formulas (currently three redundant bullet restatements of the same three lines of math).

## Verification

- `make build` succeeds after the directory move and include-path updates.
- `make test` passes, including new FOK/TUI-command tests, with `placeholder_test.cpp` removed from the suite count.
- Manual TUI smoke test: new `stop` and `fok` commands work end-to-end (submit, see it rest/trigger/fill in the Order Book and Recent Fills panels).
- README and DESIGN.md read-through: no mid-sentence bold-as-emphasis, no "this design means" transitions, quickstart appears before deep technical content.
