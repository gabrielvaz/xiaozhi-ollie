// Shared by the hooks, the channel and the pairing command. Node 22+, no npm dependencies.
import { existsSync, mkdirSync, readFileSync, writeFileSync, chmodSync, readdirSync, statSync, unlinkSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { execFileSync } from "node:child_process";

// Your relay (your own Cloudflare Workers deployment): saved by /ollie-watch:pair --relay <url>, or OLLIE_WATCH_RELAY
export const DEFAULT_RELAY = "";
export const CONFIG_DIR = process.env.OLLIE_WATCH_HOME || join(homedir(), ".config", "ollie-watch");
export const CONFIG_FILE = join(CONFIG_DIR, "config.json");
export const RUN_DIR = join(CONFIG_DIR, "run");

export function loadConfig() {
  try {
    return JSON.parse(readFileSync(CONFIG_FILE, "utf8"));
  } catch {
    return null;
  }
}

export function saveConfig(config) {
  mkdirSync(CONFIG_DIR, { recursive: true, mode: 0o700 });
  writeFileSync(CONFIG_FILE, JSON.stringify(config, null, 2) + "\n", { mode: 0o600 });
  chmodSync(CONFIG_FILE, 0o600);
}

export function relayUrl(config) {
  return (process.env.OLLIE_WATCH_RELAY || config?.relay || DEFAULT_RELAY).replace(/\/+$/, "");
}

/** JSON request to the relay. Never throws: returns { status, body } or { status: 0 }. */
export async function relay(config, method, path, body, timeoutMs = 4000) {
  if (!relayUrl(config)) return { status: 0, noRelay: true };
  try {
    const res = await fetch(relayUrl(config) + path, {
      method,
      headers: {
        "content-type": "application/json",
        ...(config?.token ? { authorization: `Bearer ${config.token}` } : {}),
      },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal: AbortSignal.timeout(timeoutMs),
    });
    let data = null;
    try {
      data = await res.json();
    } catch {}
    return { status: res.status, body: data };
  } catch {
    return { status: 0, body: null };
  }
}

export async function readStdin() {
  const chunks = [];
  for await (const c of process.stdin) chunks.push(c);
  return Buffer.concat(chunks).toString("utf8");
}

/** Parent process ids, nearest first (the Claude Code process is among the first two). */
export function ancestorPids(levels = 3) {
  const pids = [];
  let pid = process.ppid;
  for (let i = 0; i < levels && pid > 1; i++) {
    pids.push(pid);
    try {
      pid = Number(execFileSync("ps", ["-o", "ppid=", "-p", String(pid)], { encoding: "utf8" }).trim());
    } catch {
      break;
    }
  }
  return pids;
}

/** The SessionStart hook records which session each Claude Code process is running. */
export function recordSession(sessionId) {
  mkdirSync(RUN_DIR, { recursive: true, mode: 0o700 });
  for (const pid of ancestorPids()) writeFileSync(join(RUN_DIR, `${pid}.json`), JSON.stringify({ sessionId, at: Date.now() }));
  // Clean entries of processes that are gone.
  for (const name of readdirSync(RUN_DIR)) {
    const pid = Number(name.replace(".json", ""));
    try {
      process.kill(pid, 0);
    } catch {
      try {
        unlinkSync(join(RUN_DIR, name));
      } catch {}
    }
  }
}

export function sessionForPid(pid) {
  const file = join(RUN_DIR, `${pid}.json`);
  if (!existsSync(file)) return null;
  try {
    return { ...JSON.parse(readFileSync(file, "utf8")), mtime: statSync(file).mtimeMs };
  } catch {
    return null;
  }
}

const base = (p) => String(p ?? "").split("/").filter(Boolean).pop() ?? "";

/** A short, safe description of what a tool call touches. */
export function toolTarget(tool, input = {}) {
  if (typeof input === "string") {
    try {
      input = JSON.parse(input);
    } catch {
      input = {};
    }
  }
  switch (tool) {
    case "Read":
    case "Edit":
    case "Write":
    case "MultiEdit":
      return base(input.file_path);
    case "NotebookEdit":
      return base(input.notebook_path);
    case "Bash":
      return String(input.description || input.command || "").split("\n")[0].slice(0, 80);
    case "Grep":
    case "Glob":
      return String(input.pattern ?? "").slice(0, 60);
    case "WebFetch":
      try {
        return new URL(input.url).hostname;
      } catch {
        return "";
      }
    case "WebSearch":
      return String(input.query ?? "").slice(0, 60);
    case "Task":
    case "Agent":
      return String(input.description ?? "").slice(0, 60);
    default:
      return "";
  }
}
