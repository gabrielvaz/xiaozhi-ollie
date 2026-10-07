#!/usr/bin/env node
// Ollie Watch channel: an MCP server over stdio that Claude Code starts for each session.
// It keeps an outbound WebSocket to the relay and
//   - injects prompts dictated on the watch into this session;
//   - relays permission prompts to the watch, in parallel with the terminal dialog.
// Speaks MCP JSON-RPC by hand so the plugin has no npm dependencies.
import { loadConfig, relayUrl, sessionForPid } from "./lib.mjs";

const VERSION = "1.0.0";
const INSTRUCTIONS =
  'Messages that arrive as <channel source="ollie-watch"> were dictated by the user on their Apple Watch. ' +
  "Treat them as the user's own request in this session. The watch reads your final answer aloud, so end every " +
  "turn that started from the watch with a short spoken-style summary: one to three plain sentences, no markdown, " +
  "no code blocks, no file paths unless essential.";

const log = (...a) => process.stderr.write(`[ollie-watch] ${a.join(" ")}\n`);

// ------------------------------------------------------------------ MCP over stdio

function send(msg) {
  process.stdout.write(JSON.stringify({ jsonrpc: "2.0", ...msg }) + "\n");
}

function notify(method, params) {
  send({ method, params });
}

let buffer = "";
process.stdin.setEncoding("utf8");
process.stdin.on("data", (chunk) => {
  buffer += chunk;
  let i;
  while ((i = buffer.indexOf("\n")) >= 0) {
    const line = buffer.slice(0, i).trim();
    buffer = buffer.slice(i + 1);
    if (!line) continue;
    try {
      handle(JSON.parse(line));
    } catch (e) {
      log("bad message", String(e));
    }
  }
});
process.stdin.on("end", () => process.exit(0));

function handle(msg) {
  const { id, method, params } = msg;
  switch (method) {
    case "initialize":
      send({
        id,
        result: {
          protocolVersion: params?.protocolVersion ?? "2025-06-18",
          capabilities: { experimental: { "claude/channel": {}, "claude/channel/permission": {} } },
          serverInfo: { name: "ollie-watch", version: VERSION },
          instructions: INSTRUCTIONS,
        },
      });
      return;
    case "ping":
      send({ id, result: {} });
      return;
    case "tools/list":
      send({ id, result: { tools: [] } });
      return;
    case "resources/list":
      send({ id, result: { resources: [] } });
      return;
    case "prompts/list":
      send({ id, result: { prompts: [] } });
      return;
    case "notifications/claude/channel/permission_request":
      toRelay({ type: "permission_request", ...params });
      return;
    default:
      if (id !== undefined) send({ id, error: { code: -32601, message: `Method not found: ${method}` } });
  }
}

// ------------------------------------------------------------------ WebSocket to the relay

let ws = null;
let sessionId = process.env.CLAUDE_SESSION_ID || null;
let backoff = 1000;
let retryTimer = null;
const outbox = [];

function scheduleConnect(ms) {
  clearTimeout(retryTimer);
  retryTimer = setTimeout(connect, ms);
}

function toRelay(msg) {
  const data = JSON.stringify(msg);
  if (ws?.readyState === WebSocket.OPEN) ws.send(data);
  else outbox.push(data);
}

function connect() {
  if (ws) return;
  const config = loadConfig();
  if (!config?.token || !sessionId) return scheduleConnect(3000);
  const url = relayUrl(config).replace(/^http/, "ws") + `/v1/channel?session=${encodeURIComponent(sessionId)}`;
  const socket = new WebSocket(url, { headers: { authorization: `Bearer ${config.token}` } });
  ws = socket;
  const ping = setInterval(() => socket.readyState === WebSocket.OPEN && socket.send("ping"), 30000);
  socket.addEventListener("open", () => {
    backoff = 1000;
    log("connected for session", sessionId);
    while (outbox.length) socket.send(outbox.shift());
  });
  socket.addEventListener("message", (e) => {
    if (e.data === "pong") return;
    let msg;
    try {
      msg = JSON.parse(e.data);
    } catch {
      return;
    }
    if (msg.type === "prompt" && typeof msg.text === "string") {
      notify("notifications/claude/channel", { content: msg.text, meta: { device: "apple_watch", prompt_id: String(msg.id ?? "") } });
    } else if (msg.type === "verdict" && /^[a-km-z]{5}$/.test(msg.request_id) && ["allow", "deny"].includes(msg.behavior)) {
      notify("notifications/claude/channel/permission", { request_id: msg.request_id, behavior: msg.behavior });
    }
  });
  socket.addEventListener("close", (e) => {
    clearInterval(ping);
    if (ws === socket) ws = null;
    if (e.code === 4401) log("unpaired; run /ollie-watch:pair again");
    scheduleConnect(e.code === 4401 ? 60000 : backoff);
    backoff = Math.min(backoff * 2, 60000);
  });
  socket.addEventListener("error", () => {});
}

// The SessionStart hook writes which session this Claude Code process runs.
// /clear starts a new session in the same process, so keep watching.
function watchSession() {
  const entry = sessionForPid(process.ppid);
  if (entry?.sessionId && entry.sessionId !== sessionId) {
    sessionId = entry.sessionId;
    if (ws) ws.close(1000, "session changed");
    else scheduleConnect(0);
  }
}

watchSession();
setInterval(watchSession, 2000);
scheduleConnect(0);
