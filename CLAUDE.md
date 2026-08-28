# CLAUDE.md

Guidance for agents working in this repository.

## What this is

A `CalendarEvents::CalendarEventsPlugin` for the Plasma 6 digital clock that
fetches a JSON event feed over HTTP(S). Public repo, installed on the author's
Arch workstation through the PKGBUILD. Tier 2 of the house engineering
standard: tests, lint, format, coverage gate, hardened CI.

## Structure

- `plugin/feedparser.{h,cpp}`: pure parsing, per-day expansion, and the
  publish diff (`diffPublished`). No Qt plugin machinery, no network.
  Everything with a testable rule lives here.
- `plugin/homefeedevents.{h,cpp,json}`: the Qt plugin. Config, fetch timer,
  emitting the diff to the clock model, `ShowEventInterface`.
- `plugin/CMakeLists.txt`: `homefeedparser` static library plus the
  `homefeedevents` plugin installed to `plasmacalendarplugins/`.
- `tests/feedparsertest.cpp`, `tests/fixtures/feed.json`: QTest suite for the
  parser and the diff.
- `tests/pluginloadtest.cpp`: loads the built `.so` through `KPluginMetaData`
  and `QPluginLoader` (plasma-workspace's path) with an empty
  `XDG_CONFIG_HOME`; checks metadata, both interfaces, and offline no-ops.
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
  Any new rule about dates, spans, document shape, or what to emit goes here
  with a test, never into the plugin file.
- `homefeedevents.cpp`: covered structurally by `pluginloadtest` (metadata,
  loadability, both interfaces, unconfigured no-ops). The remaining paths
  (network reply handling, `KConfigWatcher` reload, live emission into
  plasmashell) are exempt: they need a live endpoint and a running shell.
  Verified manually by installing and enabling the plugin in the clock.

## Changelog and updates

- No CHANGELOG file. Git history and GitHub releases serve the purpose.
- Build dependencies are Arch system packages resolved at build time; there
  is no lockfile. Dependabot covers GitHub Actions only.
- CI's `archlinux:base-devel` image is a tag, not a digest, and `pacman -Syu`
  installs whatever is current. This is deliberate: the only target is a
  rolling distro, so the question CI answers is "does this build on today's
  Arch", the same question the PKGBUILD faces on the user's machine. Pinning
  the image would make CI drift from the target, and Dependabot does not bump
  workflow `container:` digests.
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
- 2026-08-27: Moved the refresh diff out of the plugin into
  `HomeFeed::diffPublished`. It was pure logic hiding under the "plugin glue,
  exempt" label; it now has tests for new, vanished, changed, and moved
  events, including the rule that a multi-day event is removed once by uid.
- 2026-08-27: Added `pluginloadtest`. The explicit `Id` metadata warning
  reached an installed build because nothing in CI loaded the plugin the way
  plasmashell does; the test now pins that and the two interfaces.
- 2026-08-27: Recorded the unpinned Arch CI image as a decision rather than a
  gap (see Changelog and updates).
