# v1.1.5

Sticky markers on menu open.

## Fixes

- **Trust last known markers.** Opening the menu no longer auto-reprobes every host on a timer. Hosts with good saved state display instantly and stay that way until you refresh, until a probe you triggered with `r`/`R` says otherwise, or until the post-disconnect reprobe runs after a connection. A transient outage no longer wipes the row to `?`.
- **Auto-probe still runs for unknowns.** Hosts with no saved state, or whose last probe was a failure (`?`), are still probed on menu open so they recover automatically when reachable again.

## Removed

- **`probe.cache_seconds` config key.** Made obsolete by the change above. Existing entries in `~/.config/lilypad/config` are ignored harmlessly.

## Install

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash
```

## Upgrading

```
jump update
```
