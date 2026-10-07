---
description: Pair this computer with the Ollie app on your Apple Watch
argument-hint: <code> | --relay <url> | --statusline | --status | --unpair
allowed-tools: Bash(node:*)
---

Result of `/ollie-watch:pair $ARGUMENTS`:

!`node "${CLAUDE_PLUGIN_ROOT}/scripts/pair.mjs" "$ARGUMENTS"`

Show that result to the user as is, in the user's language. Then:

- If pairing just succeeded and it says plan usage is off, ask whether they want the watch to show their 5-hour and weekly plan usage. Explain that this sets the Claude Code status line in `~/.claude/settings.json` (their current status line, if any, keeps showing; a backup is saved). Only if they say yes, run `node "${CLAUDE_PLUGIN_ROOT}/scripts/pair.mjs" --statusline` and show its output.
- If it failed because the code is wrong or expired, tell them to read the code again on the watch.
- Do nothing else.
