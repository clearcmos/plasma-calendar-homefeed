# CLAUDE.md

Guidance for agents working in this repository.

## What this is

A `CalendarEvents::CalendarEventsPlugin` for the Plasma 6 digital clock that
fetches a JSON event feed over HTTP(S). Public repo, installed on the author's
Arch workstation through the PKGBUILD. Tier 2 of the house engineering
standard: tests, lint, format, coverage gate, hardened CI.

## Structure

- `plugin/feedparser.{h,cpp}`: pure parsing and per-day expansion. No Qt
  plugin machinery, no network. Everything with a testable rule lives here.
- `plugin/homefeedevents.{h,cpp,json}`: the Qt plugin. Config, fetch timer,
  diffing against what the clock model already holds, `ShowEventInterface`.
- `plugin/CMakeLists.txt`: `homefeedparser` static library plus the
  `homefeedevents` plugin installed to `plasmacalendarplugins/`.
- `tests/feedparsertest.cpp`, `tests/fixtures/feed.json`: QTest suite for the
  parser.
- `PKGBUILD`: Arch package, git source, version derived from `project()`.
- `.github/workflows/ci.yml`: Arch container job; see Commands.

Sources live in `plugin/`, not `src/`, because makepkg run in-tree uses
`src/` as its working directory.

## Commands

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_CXX_FLAGS="-Werror --coverage"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target clang-format
cppcheck --library=qt --enable=warning,style,performance,portability --error-exitcode=1 --suppress=missingIncludeSystem --inline-suppr -I plugin plugin tests
gcovr --root . --filter 'plugin/feedparser\.cpp' --fail-under-line 90 --print-summary build
```

CI runs exactly these, in this order, then checks the install layout
(`DESTDIR=stage cmake --install build` must produce
`usr/lib/qt6/plugins/plasmacalendarplugins/homefeedevents.so`) and runs
`namcap PKGBUILD`. Run the format target before committing; CI fails on a diff.

## Conventions

- `KDECompilerSettings` is in effect: `QT_NO_CAST_FROM_ASCII`,
  `QT_NO_KEYWORDS`, C++20. Use `u"..."_s`, `"..."_L1`, `Q_EMIT`, `Q_SIGNALS`,
  `Q_SLOTS`.
- Keep `homefeedevents.cpp` thin. Any rule about dates, spans, or document
  shape belongs in `feedparser.cpp` where the tests can reach it.
- Tests pin `TZ=UTC` through a CTest property and assert it in
  `initTestCase`. Fixture timestamps that test zone conversion use non-UTC
  offsets on purpose.
- A regression test carries a comment naming the bug it pins.
- House style: no emojis, no em dashes, factual README, default branch `main`,
  no secrets or personal identifiers in tracked files (the feed URL in docs
  is `home.example`).

## Test coverage policy

- `feedparser.cpp`: covered by `feedparsertest`, gated at 90 percent lines.
- `homefeedevents.cpp`: exempt. It needs plasmashell's `EventPluginsManager`,
  a live network endpoint, and `KConfigWatcher` notifications to exercise.
  Verified manually by installing and enabling the plugin in the clock.

## Changelog and updates

- No CHANGELOG file. Git history and GitHub releases serve the purpose.
- Build dependencies are Arch system packages resolved at build time; there
  is no lockfile. Dependabot covers GitHub Actions only.
- Deployment to the author's machine (setup script block, rc file) lives in
  the consumer's configuration repository, not here.

## Decision log

- 2026-08-25: Consume a server-published feed instead of calling the Google
  Calendar API from the desktop. The home server already holds the OAuth
  token for its dashboard; adding one JSON output there keeps credentials off
  the workstation and keeps this plugin provider-agnostic.
- 2026-08-25: Standalone public repo with a PKGBUILD rather than sources in
  the workstation config repo, matching how the author's other custom
  packages are built (clone plus `makepkg -sri` from a setup script).
- 2026-08-25: Expand multi-day events to one hash entry per day in the plugin.
  plasma-workspace's `DaysModel::onDataReady` looks events up by exact date
  and never expands spans; the bundled holidays plugin does the same
  expansion.
- 2026-08-25: Refreshes emit a diff (`eventRemoved` then `dataReady` for new
  or changed uids) instead of re-emitting everything. `DaysModel::onDataReady`
  appends without deduplicating; the model only clears when it calls
  `loadEventsForDateRange` itself on a month change, so a full re-emit at any
  other time duplicates every event.
- 2026-08-25: All-day `end` is treated as exclusive. Google and iCalendar
  both send the day after the last day; treating it as inclusive showed
  one-day events on two days. Pinned by
  `allDaySingleDayIgnoresExclusiveEnd`.
- 2026-08-25: No QML settings page in v1. The holidays plugin needs a helper
  QML module to write its config; an rc file watched by `KConfigWatcher`
  gives live reload with far less code.
- 2026-08-25: CI runs in an `archlinux:base-devel` container because Arch is
  the only target and its KF6/Qt packages match the PKGBUILD's dependencies.
