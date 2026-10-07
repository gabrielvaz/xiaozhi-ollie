# Ollie for Apple Watch

Follow and steer your Claude Code sessions from your wrist. No server on your computer.

- **Sessions at a glance**: what each session is doing right now ("Editing App.swift"), a live timer, and which ones wait for you.
- **Alerts**: a notification when a session finishes or needs permission, with Allow and Deny right on it.
- **Talk to a session**: dictate on the watch; the text goes to the session as a prompt and the answer is read aloud with the watch's own voice.
- **Plan usage**: the 5-hour and weekly limits, with reset times, in the app and as a complication.
- English and Portuguese (Brazil).

## How it works

```
Claude Code ── ollie-watch plugin ── hooks / status line ──HTTPS──▶ Relay (Cloudflare Worker,  ──APNs──▶ Apple Watch
                                 └── channel (MCP, stdio) ◀──WSS───┘ one Durable Object per pair) ◀─HTTPS──┘
```

- **Plugin** (`plugin/`): hooks report progress, a status line wrapper reports plan usage, and a channel that Claude Code starts by itself delivers prompts from the watch and relays permission prompts. Node 22+, no npm dependencies.
- **Relay** (`relay/`): stores session state per pairing, sends pushes, and holds the WebSocket of each channel. It never sees your Claude credentials.
- **App** (`app/`): SwiftUI, watchOS 11+, no iPhone app.

Your Claude login never leaves Claude Code. Anthropic does not allow third-party apps to sign in with Claude.ai accounts, so the watch is linked to Claude Code by pairing, not by login.

## Set up

1. Deploy your own relay (`relay/`, a Cloudflare Worker; see Development) and build the app with your own
   identity: copy `app/Config/Local.example.xcconfig` to `app/Config/Local.xcconfig` (kept out of git) and fill in
   your Team ID, bundle ID prefix and relay URL.
2. Install the app on your Apple Watch and open it. It shows a code like `K7Q-42M`.
3. In Claude Code:
   ```
   /plugin marketplace add gabrielvaz/xiaozhi-ollie
   /plugin install ollie-watch@ollie
   /ollie-watch:pair --relay https://ollie-watch-relay.<your-subdomain>.workers.dev
   /ollie-watch:pair K7Q-42M
   ```
4. Optional, plan usage on the watch: `/ollie-watch:pair --statusline` (keeps your current status line).
5. Optional, talk to sessions and approve in parallel with the terminal: start Claude Code with the channel. Channels are a research preview, so custom ones need the development flag:
   ```
   claude --dangerously-load-development-channels plugin:ollie-watch@ollie
   ```
   Without the channel, everything else works, and approvals from the watch go through a hook that waits up to 60 s (`OLLIE_WATCH_PERMISSION_WAIT`) before the terminal asks.

Pair more computers with the same watch by running `/ollie-watch:pair` with a new code on each one.

## Development

| Part | Commands |
|---|---|
| Relay | `cd relay && npm install && npm test` (runs against `wrangler dev`) · `npx wrangler deploy` · push needs `npx wrangler secret put` for `APNS_TEAM_ID`, `APNS_TOPIC` (`<bundle prefix>.watchkitapp`), `APNS_KEY_ID` and `APNS_KEY_P8` (same names in `.dev.vars` for `wrangler dev`) |
| Plugin | `claude --plugin-dir apple-watch/plugin` · `claude plugin validate apple-watch/plugin` · `OLLIE_WATCH_HOME` and `OLLIE_WATCH_RELAY` point it at a test config and relay |
| App | Team ID, bundle IDs and relay URL come from `app/Config/Ollie.xcconfig` (examples) and your `app/Config/Local.xcconfig`. `cd app && xcodegen generate`, then the `OllieWatch` scheme (tests included). Debug builds accept `-OllieSkipPushPrompt YES` and `-OllieOpenRoute usage\|settings\|session:<id>` for screenshots |

Release: archive the `Ollie` scheme for `generic/platform=iOS` (the empty container the App Store needs for a watch-only app) and export with `app/ExportOptions.plist`.

Design notes: [`docs/superpowers/specs/2026-10-06-ollie-watch-design.md`](../docs/superpowers/specs/2026-10-06-ollie-watch-design.md).
