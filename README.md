# plasma-calendar-homefeed

Calendar events plugin for the KDE Plasma 6 digital clock. It reads a JSON
event feed over HTTP(S) and shows the events in the clock's calendar popup,
alongside the built-in holidays. No Akonadi and no provider credentials on the
desktop: whatever produces the feed owns the upstream calendar access.

## Why

The stock clock only sources events from Akonadi (via kdepim-addons). On a
machine that does not run the PIM stack, but where a home server already
talks to the calendar provider and can publish a plain JSON file on the LAN,
this plugin is the missing piece between that file and the clock.

## How it works

- plasma-workspace's `EventPluginsManager` loads every Qt plugin found under
  `<qt plugin dir>/plasmacalendarplugins/` that implements
  `CalendarEvents::CalendarEventsPlugin` (KDeclarative's `KF6::CalendarEvents`).
  Each one shows up as a checkbox under Digital Clock settings, Calendar tab.
- On start and every `RefreshMinutes`, the plugin fetches `FeedUrl`, parses
  it, and answers the clock's `loadEventsForDateRange` with the events inside
  the requested six-week window, one entry per day an event touches.
- Later refreshes emit only the difference (`eventRemoved` plus `dataReady`
  for new or changed events), so the clock's model never holds duplicates.
- A failed fetch or a malformed document keeps the previous events and logs a
  warning in the `org.clearcmos.plasma.homefeed` category.
- Where the clock exposes it, opening an event follows the event's `url`
  (`ShowEventInterface`).

## Feed format

A JSON object with an `events` array. Everything else is informational.

```json
{
  "generated_at": "2026-08-25T12:00:00-04:00",
  "window": {"start": "2026-05-27", "end": "2027-08-25"},
  "events": [
    {
      "uid": "unique-per-instance",
      "title": "Dentist",
      "start": "2026-08-25T14:00:00-04:00",
      "end": "2026-08-25T15:00:00-04:00",
      "all_day": false,
      "location": "Downtown",
      "description": null,
      "url": "https://calendar.example/event/1",
      "color": "#7986cb"
    },
    {
      "uid": "payday-2026-08-28",
      "title": "Payday",
      "start": "2026-08-28",
      "end": "2026-08-29",
      "all_day": true
    }
  ]
}
```

Rules:

- Timed events use RFC 3339 timestamps with an offset; days are assigned in
  the desktop's local time zone. An event ending exactly at midnight does not
  spill onto the next day.
- All-day events use bare `YYYY-MM-DD` dates and an exclusive `end` (the
  Google Calendar and iCalendar convention). `start` 2026-09-04 with `end`
  2026-09-07 covers the 4th, 5th, and 6th.
- A missing or invalid `end` yields a one-day or zero-length event.
- Recurring events must arrive pre-expanded, one entry per instance, each
  with its own `uid`.
- Entries without a `uid` or with an unparsable `start` are skipped and
  counted; the count is logged at info level. Unknown keys are ignored.
- `location`, `description`, `url`, and `color` may be null or absent.

The producer is out of scope for this repository. The author's is a small
Python script on a home server that queries the Google Calendar API with
`singleEvents=true` over a -90/+365 day window and writes this file every two
minutes behind a LAN-only reverse proxy.

## Install

Arch Linux:

```bash
git clone https://github.com/clearcmos/plasma-calendar-homefeed.git && cd plasma-calendar-homefeed && makepkg -sri
```

This installs `/usr/lib/qt6/plugins/plasmacalendarplugins/homefeedevents.so`.

Anywhere else, with ECM, Qt 6 (Core, Gui, Network), KF6 Config, CoreAddons,
and Declarative development packages installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build && sudo cmake --install build
```

plasma-workspace scans for plugins when plasmashell starts, so after the first
install restart it (`plasmashell --replace &`) or log out and back in.

## Configure

Create `~/.config/plasma-calendar-homefeedrc`:

```ini
[General]
FeedUrl=https://home.example/calendar.json
RefreshMinutes=5
EventColor=#3daee9
```

- `FeedUrl` (required): where to fetch the document. Redirects to a less
  secure scheme are refused; the transfer times out after 15 seconds.
- `RefreshMinutes` (default 5, minimum 1): polling interval.
- `EventColor` (optional): color for events whose feed entry has no `color`.

Changes to the file are picked up live. Then enable the plugin: right-click
the clock, Configure Digital Clock, Calendar, tick "Home feed".

## Development

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_CXX_FLAGS="-Werror --coverage"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target clang-format
cppcheck --library=qt --enable=warning,style,performance,portability --error-exitcode=1 --suppress=missingIncludeSystem --inline-suppr -I plugin plugin tests
gcovr --root . --filter 'plugin/feedparser\.cpp' --fail-under-line 90 --print-summary build
```

The `clang-format` target rewrites sources in the KDE style shipped with ECM;
CI runs the same target and fails on any resulting diff. Two test binaries
run under CTest: `feedparsertest` covers parsing, per-day expansion, and the
refresh diff (with `TZ=UTC`, which it asserts, because day assignment depends
on the zone); `pluginloadtest` loads the built `.so` the way plasmashell does
and checks its metadata and interfaces, with an empty `XDG_CONFIG_HOME` so it
never touches the network.

To see the plugin's log while it runs inside plasmashell:

```bash
QT_LOGGING_RULES="org.clearcmos.plasma.homefeed.debug=true" plasmashell --replace
```

## Limitations

- No settings page in the clock's configuration dialog; the rc file is the
  only configuration.
- No authentication for the feed URL. It is meant for LAN or VPN-only
  endpoints.
- Events outside the feed's window are simply absent.

## License

MIT. See [LICENSE](LICENSE).
