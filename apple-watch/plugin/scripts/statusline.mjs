#!/usr/bin/env node
// Status line wrapper: sends rate_limits (5-hour and weekly plan usage) to the Ollie Watch
// relay at most once a minute, then prints the status line you had before, if any.
// Copied to ~/.config/ollie-watch/ by /ollie-watch:pair so it survives plugin updates.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";

const DIR = process.env.OLLIE_WATCH_HOME || join(homedir(), ".config", "ollie-watch");
const DEFAULT_RELAY = "https://ollie-watch-relay.example.workers.dev";

const chunks = [];
for await (const c of process.stdin) chunks.push(c);
const raw = Buffer.concat(chunks).toString("utf8");

let config = null;
try {
  config = JSON.parse(readFileSync(join(DIR, "config.json"), "utf8"));
} catch {}

// 1. Your previous status line keeps working.
if (config?.previousStatusLine) {
  const out = spawnSync(config.previousStatusLine, { shell: true, input: raw, encoding: "utf8", timeout: 5000 });
  if (out.stdout) process.stdout.write(out.stdout);
}

let data = {};
try {
  data = JSON.parse(raw);
} catch {}
const limits = data.rate_limits;

if (!config?.previousStatusLine) {
  const pct = (w) => (w ? `${Math.round(w.used_percentage)}%` : "–");
  process.stdout.write(limits ? `⌚ 5h ${pct(limits.five_hour)} · 7d ${pct(limits.seven_day)}` : "⌚ Ollie Watch");
}

// 2. Plan usage to the watch.
if (config?.token && limits && (limits.five_hour || limits.seven_day)) {
  const stamp = join(DIR, "usage-sent.json");
  let last = {};
  try {
    last = JSON.parse(readFileSync(stamp, "utf8"));
  } catch {}
  const body = JSON.stringify({ five_hour: limits.five_hour, seven_day: limits.seven_day });
  if (Date.now() - (last.at ?? 0) > 60_000 || last.body !== body) {
    mkdirSync(DIR, { recursive: true });
    writeFileSync(stamp, JSON.stringify({ at: Date.now(), body }));
    const relay = (process.env.OLLIE_WATCH_RELAY || config.relay || DEFAULT_RELAY).replace(/\/+$/, "");
    try {
      await fetch(`${relay}/v1/usage`, {
        method: "POST",
        headers: { "content-type": "application/json", authorization: `Bearer ${config.token}` },
        body,
        signal: AbortSignal.timeout(1500),
      });
    } catch {}
  }
}
