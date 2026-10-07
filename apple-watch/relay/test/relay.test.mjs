// Testes de ponta a ponta contra `wrangler dev` (sobe sozinho numa porta livre).
import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";

const PORT = 8790 + Math.floor(Math.random() * 100);
const BASE = `http://127.0.0.1:${PORT}`;
let dev;

before(async () => {
  dev = spawn("npx", ["wrangler", "dev", "--port", String(PORT), "--ip", "127.0.0.1"], {
    cwd: new URL("..", import.meta.url).pathname,
    stdio: ["ignore", "pipe", "pipe"],
  });
  for (let i = 0; i < 120; i++) {
    try {
      if ((await fetch(`${BASE}/health`)).ok) return;
    } catch {}
    await new Promise((r) => setTimeout(r, 500));
  }
  throw new Error("wrangler dev did not start");
});

after(() => dev?.kill("SIGINT"));

const call = async (method, path, token, body) => {
  const res = await fetch(BASE + path, {
    method,
    headers: { "content-type": "application/json", ...(token ? { authorization: `Bearer ${token}` } : {}) },
    body: body ? JSON.stringify(body) : undefined,
  });
  return { status: res.status, body: await res.json() };
};

async function pair() {
  const start = await call("POST", "/v1/pair/start");
  assert.equal(start.status, 200);
  assert.match(start.body.code, /^[2-9A-Z]{3}-[2-9A-Z]{3}$/);
  const claim = await call("POST", "/v1/pair/claim", null, { code: start.body.code.toLowerCase().replace("-", ""), host: "test-mac" });
  assert.equal(claim.status, 200);
  return { watch: start.body.token, host: claim.body.token, code: start.body.code };
}

test("pareamento: código vale uma vez e o relógio vê o computador", async () => {
  const { watch, code } = await pair();
  const again = await call("POST", "/v1/pair/claim", null, { code, host: "x" });
  assert.equal(again.status, 404);
  const status = await call("GET", "/v1/pair/status", watch);
  assert.equal(status.body.paired, true);
  assert.equal(status.body.hosts[0].name, "test-mac");
});

test("autenticação: token errado é recusado e o computador não lê o estado", async () => {
  const { host } = await pair();
  assert.equal((await call("GET", "/v1/state", "0".repeat(32) + "." + "1".repeat(64))).status, 401);
  assert.equal((await call("GET", "/v1/state", host)).status, 404);
});

test("eventos viram estado de sessão", async () => {
  const { watch, host } = await pair();
  const sid = "sess-1";
  await call("POST", "/v1/events", host, { event: "SessionStart", session_id: sid, cwd: "/Users/a/dev/api" });
  await call("POST", "/v1/events", host, { event: "UserPromptSubmit", session_id: sid, prompt: "Run the tests\nplease" });
  await call("POST", "/v1/events", host, { event: "PreToolUse", session_id: sid, tool_name: "Edit", target: "App.swift" });
  let s = (await call("GET", "/v1/state", watch)).body.sessions[0];
  assert.equal(s.project, "api");
  assert.equal(s.title, "Run the tests");
  assert.equal(s.state, "working");
  assert.deepEqual(s.activity, { tool: "Edit", target: "App.swift" });
  assert.equal(s.channel, false);

  await call("POST", "/v1/events", host, { event: "Notification", session_id: sid, notification_type: "permission_prompt", message: "Claude needs permission" });
  s = (await call("GET", "/v1/state", watch)).body.sessions[0];
  assert.equal(s.state, "waiting");

  await call("POST", "/v1/events", host, { event: "Stop", session_id: sid, last_assistant_message: "All 42 tests pass." });
  s = (await call("GET", "/v1/state", watch)).body.sessions[0];
  assert.equal(s.state, "done");
  assert.equal(s.lastMessage, "All 42 tests pass.");
  assert.equal(typeof s.lastDurationSec, "number");

  await call("POST", "/v1/events", host, { event: "SessionEnd", session_id: sid });
  assert.equal((await call("GET", "/v1/state", watch)).body.sessions.length, 0);
});

test("uso do plano", async () => {
  const { watch, host } = await pair();
  await call("POST", "/v1/usage", host, { five_hour: { used_percentage: 23.5, resets_at: 1738425600 }, seven_day: { used_percentage: 41.2, resets_at: 1738857600 } });
  const usage = (await call("GET", "/v1/state", watch)).body.usage;
  assert.deepEqual(usage.fiveHour, { pct: 23.5, resetsAt: 1738425600 });
  assert.equal(usage.sevenDay.pct, 41.2);
});

test("permissão pelo hook: espera e recebe a decisão do relógio", async () => {
  const { watch, host } = await pair();
  await call("POST", "/v1/events", host, { event: "SessionStart", session_id: "s2", cwd: "/x/web" });
  const open = await call("POST", "/v1/permissions", host, { session_id: "s2", tool_name: "Bash", description: "Run npm test" });
  assert.ok(open.body.id);
  const pending = (await call("GET", "/v1/state", watch)).body.permissions;
  assert.equal(pending.length, 1);
  assert.equal(pending[0].tool, "Bash");

  const waiting = call("GET", `/v1/permissions/${open.body.id}/wait`, host);
  await new Promise((r) => setTimeout(r, 300));
  assert.equal((await call("POST", `/v1/permissions/${open.body.id}`, watch, { decision: "allow" })).status, 200);
  assert.deepEqual((await waiting).body, { decision: "allow" });
  assert.equal((await call("GET", "/v1/state", watch)).body.permissions.length, 0);
});

test("prompt sem channel responde 409; com channel chega pelo WebSocket", async () => {
  const { watch, host } = await pair();
  await call("POST", "/v1/events", host, { event: "SessionStart", session_id: "s3", cwd: "/x/app" });
  assert.equal((await call("POST", "/v1/sessions/s3/prompt", watch, { text: "hi" })).status, 409);

  const ws = new WebSocket(`ws://127.0.0.1:${PORT}/v1/channel?session=s3`, { headers: { authorization: `Bearer ${host}` } });
  const inbox = [];
  ws.addEventListener("message", (e) => inbox.push(JSON.parse(e.data)));
  await new Promise((resolve, reject) => {
    ws.addEventListener("open", resolve);
    ws.addEventListener("error", reject);
  });

  assert.equal((await call("GET", "/v1/state", watch)).body.sessions[0].channel, true);
  assert.equal((await call("POST", "/v1/sessions/s3/prompt", watch, { text: "Run the linter" })).status, 202);
  await new Promise((r) => setTimeout(r, 300));
  assert.equal(inbox[0]?.type, "prompt");
  assert.equal(inbox[0]?.text, "Run the linter");

  // Com channel, o hook de permissão não bloqueia o terminal.
  assert.deepEqual((await call("POST", "/v1/permissions", host, { session_id: "s3", tool_name: "Bash" })).body, { skip: true });

  // Permissão pelo channel: o veredito volta pelo WebSocket.
  ws.send(JSON.stringify({ type: "permission_request", request_id: "abcde", tool_name: "Write", description: "Write README.md", input_preview: "{}" }));
  await new Promise((r) => setTimeout(r, 300));
  const perm = (await call("GET", "/v1/state", watch)).body.permissions[0];
  assert.equal(perm.tool, "Write");
  await call("POST", `/v1/permissions/${perm.id}`, watch, { decision: "deny" });
  await new Promise((r) => setTimeout(r, 300));
  assert.deepEqual(inbox[1], { type: "verdict", request_id: "abcde", behavior: "deny" });

  // Resposta pedida pelo relógio volta no Stop.
  await call("POST", "/v1/events", host, { event: "Stop", session_id: "s3", last_assistant_message: "Lint is clean." });
  assert.equal((await call("GET", "/v1/state", watch)).body.sessions[0].lastMessage, "Lint is clean.");
  ws.close();
});

test("desfazer pareamento invalida os dois tokens", async () => {
  const { watch, host } = await pair();
  assert.equal((await call("DELETE", "/v1/pair", watch)).status, 200);
  assert.equal((await call("GET", "/v1/state", watch)).status, 401);
  assert.equal((await call("POST", "/v1/usage", host, {})).status, 401);
});
