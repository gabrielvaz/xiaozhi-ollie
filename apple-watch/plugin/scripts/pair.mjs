#!/usr/bin/env node
// /ollie-watch:pair --relay <URL> sets the address of your relay (do this once, before pairing)
// /ollie-watch:pair <CODE>        pairs this computer with the watch showing CODE
// /ollie-watch:pair --statusline  sends plan usage to the watch through the status line
// /ollie-watch:pair --status      shows the current pairing
// /ollie-watch:pair --unpair      forgets the pairing on this computer
import { copyFileSync, readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { homedir, hostname } from "node:os";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { CONFIG_DIR, CONFIG_FILE, loadConfig, relay, relayUrl, saveConfig } from "./lib.mjs";

const arg = (process.argv[2] ?? "").trim();
const here = dirname(fileURLToPath(import.meta.url));
const SETTINGS = join(homedir(), ".claude", "settings.json");
const STATUS_SCRIPT = join(CONFIG_DIR, "statusline.mjs");
const STATUS_COMMAND = `node "${STATUS_SCRIPT}"`;

const say = (s) => process.stdout.write(s + "\n");

const NO_RELAY =
  "Set your relay first: /ollie-watch:pair --relay https://ollie-watch-relay.<your-subdomain>.workers.dev";

if (arg.startsWith("--relay")) {
  const url = arg.slice("--relay".length).trim().replace(/\/+$/, "");
  if (!/^https:\/\/[^\s/]+/.test(url)) {
    say("Use: /ollie-watch:pair --relay https://ollie-watch-relay.<your-subdomain>.workers.dev");
    process.exit(1);
  }
  saveConfig({ ...(loadConfig() ?? {}), relay: url });
  say(`Relay set to ${url}. Now open Ollie on your Apple Watch and run /ollie-watch:pair <code>.`);
  process.exit(0);
}

if (!arg || arg === "--status") {
  const config = loadConfig();
  if (!config?.token) {
    say("Not paired. Open Ollie on your Apple Watch and run /ollie-watch:pair <code>.");
    process.exit(0);
  }
  const r = await relay(config, "GET", "/v1/whoami");
  if (r.noRelay) say(NO_RELAY);
  else if (r.status === 200) say(`Paired as "${config.host}" with ${relayUrl(config)}.`);
  else if (r.status === 401) say("This pairing was removed on the watch. Run /ollie-watch:pair <code> again.");
  else say(`Paired, but the relay did not answer (${r.status || "offline"}).`);
  say(`Plan usage on the watch: ${statuslineEnabled() ? "on" : "off (run /ollie-watch:pair --statusline)"}.`);
  process.exit(0);
}

if (arg === "--unpair") {
  const config = loadConfig();
  if (config) saveConfig({ relay: config.relay });
  say("This computer is no longer paired. Remove it on the watch in Settings as well.");
  process.exit(0);
}

if (arg === "--statusline") {
  enableStatusline();
  process.exit(0);
}

const code = arg.toUpperCase().replace(/[^0-9A-Z]/g, "");
if (code.length !== 6) {
  say("The code has 6 characters, like K7Q-42M. Open Ollie on your Apple Watch to see it.");
  process.exit(1);
}

const previous = loadConfig() ?? {};
const host = hostname().replace(/\.local$/, "");
const r = await relay(previous, "POST", "/v1/pair/claim", { code, host });
if (r.noRelay) {
  say(NO_RELAY);
  process.exit(1);
}
if (r.status !== 200 || !r.body?.token) {
  say(r.status === 404 ? "That code is wrong or expired. The watch shows a new one after 10 minutes." : `Could not reach the relay (${r.status || "offline"}).`);
  process.exit(1);
}
saveConfig({ ...previous, relay: previous.relay, token: r.body.token, hostId: r.body.hostId, host, pairedAt: new Date().toISOString() });
say(`Paired: "${host}" now reports to your watch.`);
say(`Config saved to ${CONFIG_FILE} (only you can read it).`);
say(statuslineEnabled() ? "Plan usage is already on." : "Plan usage is off. To turn it on: /ollie-watch:pair --statusline");
say("Voice prompts and approvals in parallel with the terminal need the channel. Start Claude Code with:");
say("  claude --dangerously-load-development-channels plugin:ollie-watch@ollie");

function statuslineEnabled() {
  try {
    return JSON.parse(readFileSync(SETTINGS, "utf8")).statusLine?.command === STATUS_COMMAND;
  } catch {
    return false;
  }
}

function enableStatusline() {
  const config = loadConfig();
  if (!config?.token) {
    say("Pair first: /ollie-watch:pair <code>.");
    process.exit(1);
  }
  mkdirSync(CONFIG_DIR, { recursive: true, mode: 0o700 });
  copyFileSync(join(here, "statusline.mjs"), STATUS_SCRIPT);
  let settings = {};
  if (existsSync(SETTINGS)) {
    try {
      settings = JSON.parse(readFileSync(SETTINGS, "utf8"));
    } catch {
      say(`${SETTINGS} is not valid JSON; not touching it.`);
      process.exit(1);
    }
  }
  const current = settings.statusLine?.command;
  if (current === STATUS_COMMAND) {
    say("Plan usage is already on.");
    return;
  }
  if (current) saveConfig({ ...config, previousStatusLine: current });
  writeFileSync(SETTINGS + ".ollie-backup", JSON.stringify(settings, null, 2) + "\n");
  settings.statusLine = { ...(settings.statusLine ?? {}), type: "command", command: STATUS_COMMAND };
  mkdirSync(dirname(SETTINGS), { recursive: true });
  writeFileSync(SETTINGS, JSON.stringify(settings, null, 2) + "\n");
  say("Plan usage on: the status line now sends the 5-hour and weekly usage to the watch.");
  if (current) say(`Your previous status line still shows; it runs inside the new one: ${current}`);
  say(`Backup of your settings: ${SETTINGS}.ollie-backup`);
}
