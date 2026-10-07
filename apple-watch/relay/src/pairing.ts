import { DurableObject } from "cloudflare:workers";
import { sendPush, type PushTarget } from "./apns";
import { clip, error, json, randomHex, safeEqual, sha256, type Env } from "./util";

const SESSION_TTL_MS = 6 * 60 * 60 * 1000;
const PERMISSION_TTL_MS = 10 * 60 * 1000;
const WAIT_MS = 25 * 1000;
const WAITING_PUSH_GAP_MS = 20 * 1000;

type SessionState = "working" | "waiting" | "done" | "idle";

interface Host {
  name: string;
  hash: string;
  pairedAt: number;
  lastSeenAt: number;
}

interface Meta {
  watchHash: string;
  createdAt: number;
  hosts: Record<string, Host>;
  pushTargets: PushTarget[];
}

export interface Session {
  id: string;
  hostId: string;
  cwd: string;
  project: string;
  title: string;
  titleFromPrompt: boolean;
  state: SessionState;
  activity?: { tool: string; target?: string };
  turnStartedAt?: number;
  lastDurationSec?: number;
  lastMessage?: string;
  lastPrompt?: string;
  waitingMessage?: string;
  viaWatch?: boolean;
  startedAt: number;
  updatedAt: number;
  lastWaitingPushAt?: number;
}

interface Permission {
  id: string;
  sessionId: string;
  tool: string;
  description: string;
  preview?: string;
  source: "hook" | "channel";
  requestId?: string;
  createdAt: number;
  decision?: "allow" | "deny";
}

interface Usage {
  fiveHour?: { pct: number; resetsAt: number };
  sevenDay?: { pct: number; resetsAt: number };
  updatedAt: number;
}

type Role = { kind: "watch" } | { kind: "host"; hostId: string };

/** Um por pareamento: estado das sessões, uso, permissões e channels conectados. */
export class PairingDO extends DurableObject<Env> {
  private meta: Meta | null = null;
  private sessions: Record<string, Session> = {};
  private perms: Record<string, Permission> = {};
  private usage: Usage | null = null;
  private waiters = new Map<string, Set<() => void>>();

  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    ctx.blockConcurrencyWhile(async () => {
      this.meta = (await ctx.storage.get<Meta>("meta")) ?? null;
      this.sessions = (await ctx.storage.get<Record<string, Session>>("sessions")) ?? {};
      this.perms = (await ctx.storage.get<Record<string, Permission>>("perms")) ?? {};
      this.usage = (await ctx.storage.get<Usage>("usage")) ?? null;
    });
    ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair("ping", "pong"));
  }

  // ------------------------------------------------------------------ RPC (Worker → DO)

  async init(watchSecret: string): Promise<void> {
    this.meta = { watchHash: await sha256(watchSecret), createdAt: Date.now(), hosts: {}, pushTargets: [] };
    await this.ctx.storage.put("meta", this.meta);
  }

  async addHost(name: string): Promise<{ hostId: string; secret: string } | null> {
    if (!this.meta) return null;
    const hostId = randomHex(4);
    const secret = randomHex(32);
    const now = Date.now();
    this.meta.hosts[hostId] = { name: clip(name, 60) ?? "computer", hash: await sha256(secret), pairedAt: now, lastSeenAt: now };
    await this.ctx.storage.put("meta", this.meta);
    return { hostId, secret };
  }

  // ------------------------------------------------------------------ HTTP

  async fetch(request: Request): Promise<Response> {
    const role = await this.authenticate(request);
    if (!role) return error(401, "unauthorized");
    const url = new URL(request.url);
    const path = url.pathname.replace(/^\/v1/, "");
    const m = request.method;

    if (role.kind === "watch") {
      if (m === "GET" && path === "/pair/status") return json({ paired: Object.keys(this.meta!.hosts).length > 0, hosts: this.hostList() });
      if (m === "GET" && path === "/state") return json(this.snapshot());
      if (m === "POST" && path === "/watch/push-token") return this.registerPush(await body(request));
      if (m === "DELETE" && path === "/pair") return this.unpair();
      let r = path.match(/^\/sessions\/([^/]+)\/prompt$/);
      if (m === "POST" && r) return this.prompt(decodeURIComponent(r[1]), await body(request));
      r = path.match(/^\/permissions\/([^/]+)$/);
      if (m === "POST" && r) return this.decide(r[1], await body(request));
      r = path.match(/^\/hosts\/([^/]+)$/);
      if (m === "DELETE" && r) return this.removeHost(r[1]);
      return error(404, "not_found");
    }

    this.touchHost(role.hostId);
    if (m === "GET" && path === "/whoami") return json({ hostId: role.hostId, hosts: this.hostList() });
    if (m === "POST" && path === "/events") return this.event(role.hostId, await body(request));
    if (m === "POST" && path === "/usage") return this.setUsage(await body(request));
    if (m === "POST" && path === "/permissions") return this.openPermission(await body(request), "hook");
    if (m === "GET" && path === "/channel") return this.acceptChannel(request, role.hostId, url.searchParams.get("session") ?? "");
    let r = path.match(/^\/permissions\/([^/]+)\/wait$/);
    if (m === "GET" && r) return this.waitDecision(r[1]);
    r = path.match(/^\/permissions\/([^/]+)$/);
    if (m === "DELETE" && r) {
      await this.dropPermission(r[1]);
      return json({ ok: true });
    }
    return error(404, "not_found");
  }

  private async authenticate(request: Request): Promise<Role | null> {
    if (!this.meta) return null;
    const secret = request.headers.get("x-ollie-secret");
    if (!secret) return null;
    const hash = await sha256(secret);
    if (safeEqual(hash, this.meta.watchHash)) return { kind: "watch" };
    for (const [hostId, h] of Object.entries(this.meta.hosts)) if (safeEqual(hash, h.hash)) return { kind: "host", hostId };
    return null;
  }

  // ------------------------------------------------------------------ relógio

  private snapshot() {
    this.prune();
    const order: Record<SessionState, number> = { waiting: 0, working: 1, done: 2, idle: 3 };
    const sessions = Object.values(this.sessions)
      .map((s) => ({
        id: s.id,
        host: this.meta!.hosts[s.hostId]?.name ?? "",
        project: s.project,
        title: s.title,
        state: s.state,
        activity: s.activity ?? null,
        turnStartedAt: s.turnStartedAt ?? null,
        lastDurationSec: s.lastDurationSec ?? null,
        lastMessage: s.lastMessage ?? null,
        lastPrompt: s.lastPrompt ?? null,
        waitingMessage: s.waitingMessage ?? null,
        channel: this.ctx.getWebSockets(`s:${s.id}`).length > 0,
        startedAt: s.startedAt,
        updatedAt: s.updatedAt,
      }))
      .sort((a, b) => order[a.state] - order[b.state] || b.updatedAt - a.updatedAt);
    const permissions = Object.values(this.perms)
      .filter((p) => !p.decision)
      .map((p) => ({
        id: p.id,
        sessionId: p.sessionId,
        sessionTitle: this.sessions[p.sessionId]?.title ?? "",
        tool: p.tool,
        description: p.description,
        preview: p.preview ?? null,
        createdAt: p.createdAt,
      }));
    return { now: Date.now(), sessions, permissions, usage: this.usage, hosts: this.hostList() };
  }

  private hostList() {
    return Object.entries(this.meta?.hosts ?? {}).map(([id, h]) => ({ id, name: h.name, pairedAt: h.pairedAt, lastSeenAt: h.lastSeenAt }));
  }

  private async registerPush(b: any): Promise<Response> {
    const token = String(b.token ?? "");
    if (!/^[0-9a-f]{32,200}$/i.test(token)) return error(400, "bad_token");
    const target = { token: token.toLowerCase(), sandbox: !!b.sandbox };
    const others = this.meta!.pushTargets.filter((t) => t.token !== target.token);
    this.meta!.pushTargets = [target, ...others].slice(0, 5);
    await this.ctx.storage.put("meta", this.meta);
    return json({ ok: true });
  }

  private async prompt(sessionId: string, b: any): Promise<Response> {
    const text = String(b.text ?? "").trim();
    if (!text) return error(400, "empty");
    if (text.length > 4000) return error(400, "too_long");
    const session = this.sessions[sessionId];
    if (!session) return error(404, "no_session");
    const sockets = this.ctx.getWebSockets(`s:${sessionId}`);
    if (!sockets.length) return error(409, "no_channel", "The session has no Ollie Watch channel connected");
    const msg = JSON.stringify({ type: "prompt", id: randomHex(4), text });
    for (const ws of sockets) ws.send(msg);
    const now = Date.now();
    Object.assign(session, { viaWatch: true, state: "working", turnStartedAt: now, lastPrompt: clip(text, 300), activity: undefined, updatedAt: now });
    await this.saveSessions();
    return json({ ok: true }, 202);
  }

  private async decide(id: string, b: any): Promise<Response> {
    const decision = b.decision === "allow" ? "allow" : b.decision === "deny" ? "deny" : null;
    if (!decision) return error(400, "bad_decision");
    const p = this.perms[id];
    if (!p || p.decision) return error(404, "no_permission");
    p.decision = decision;
    if (p.source === "channel" && p.requestId) {
      const msg = JSON.stringify({ type: "verdict", request_id: p.requestId, behavior: decision });
      for (const ws of this.ctx.getWebSockets(`s:${p.sessionId}`)) ws.send(msg);
      delete this.perms[id];
    }
    const s = this.sessions[p.sessionId];
    if (s) Object.assign(s, { state: "working", waitingMessage: undefined, updatedAt: Date.now() });
    await Promise.all([this.savePerms(), this.saveSessions()]);
    this.wake(id);
    return json({ ok: true });
  }

  private async unpair(): Promise<Response> {
    for (const ws of this.ctx.getWebSockets()) ws.close(4401, "unpaired");
    await this.ctx.storage.deleteAll();
    this.meta = null;
    this.sessions = {};
    this.perms = {};
    this.usage = null;
    return json({ ok: true });
  }

  private async removeHost(hostId: string): Promise<Response> {
    if (!this.meta!.hosts[hostId]) return error(404, "no_host");
    delete this.meta!.hosts[hostId];
    for (const ws of this.ctx.getWebSockets(`h:${hostId}`)) ws.close(4401, "unpaired");
    for (const [id, s] of Object.entries(this.sessions)) if (s.hostId === hostId) delete this.sessions[id];
    await Promise.all([this.ctx.storage.put("meta", this.meta), this.saveSessions()]);
    return json({ ok: true });
  }

  // ------------------------------------------------------------------ plugin

  private touchHost(hostId: string) {
    const h = this.meta!.hosts[hostId];
    if (h && Date.now() - h.lastSeenAt > 60_000) {
      h.lastSeenAt = Date.now();
      this.ctx.waitUntil(this.ctx.storage.put("meta", this.meta));
    }
  }

  private ensureSession(hostId: string, b: any): Session | null {
    const id = String(b.session_id ?? "");
    if (!/^[\w-]{1,100}$/.test(id)) return null;
    let s = this.sessions[id];
    const now = Date.now();
    if (!s) {
      const cwd = String(b.cwd ?? "");
      const project = cwd.split("/").filter(Boolean).pop() ?? "";
      s = { id, hostId, cwd, project, title: clip(b.session_title, 60) ?? project, titleFromPrompt: false, state: "idle", startedAt: now, updatedAt: now };
      this.sessions[id] = s;
    }
    if (b.session_title) {
      s.title = clip(b.session_title, 60)!;
      s.titleFromPrompt = true;
    }
    s.updatedAt = now;
    return s;
  }

  private async event(hostId: string, b: any): Promise<Response> {
    const name = String(b.event ?? "");
    if (name === "SessionEnd") {
      delete this.sessions[String(b.session_id)];
      this.clearPerms(String(b.session_id));
      await Promise.all([this.saveSessions(), this.savePerms()]);
      return json({ ok: true });
    }
    const s = this.ensureSession(hostId, b);
    if (!s) return error(400, "bad_session");
    const now = Date.now();
    switch (name) {
      case "SessionStart":
        break;
      case "UserPromptSubmit": {
        const prompt = String(b.prompt ?? "");
        if (!s.titleFromPrompt && prompt && !prompt.startsWith("<")) {
          s.title = clip(prompt.split("\n")[0], 60) ?? s.title;
          s.titleFromPrompt = true;
        }
        if (prompt && !prompt.startsWith("<channel")) {
          s.viaWatch = false;
          s.lastPrompt = clip(prompt, 300);
        }
        Object.assign(s, { state: "working", turnStartedAt: now, activity: undefined, waitingMessage: undefined });
        break;
      }
      case "PreToolUse":
        if (!s.turnStartedAt) s.turnStartedAt = now;
        Object.assign(s, { state: "working", activity: { tool: clip(b.tool_name, 40) ?? "Tool", target: clip(b.target, 60) }, waitingMessage: undefined });
        break;
      case "PostToolUse":
        this.clearPerms(s.id);
        if (s.state === "waiting") s.state = "working";
        break;
      case "Notification": {
        const type = String(b.notification_type ?? "");
        if (["permission_prompt", "elicitation_dialog", "agent_needs_input"].includes(type)) {
          s.state = "waiting";
          s.waitingMessage = clip(b.message, 200);
          await this.pushWaiting(s, s.waitingMessage ?? "");
        }
        break;
      }
      case "Stop": {
        const duration = s.turnStartedAt ? Math.round((now - s.turnStartedAt) / 1000) : undefined;
        const message = String(b.last_assistant_message ?? "");
        const viaWatch = !!s.viaWatch;
        Object.assign(s, {
          state: "done",
          lastMessage: clip(message, 1500),
          lastDurationSec: duration,
          turnStartedAt: undefined,
          activity: undefined,
          waitingMessage: undefined,
          viaWatch: false,
        });
        this.clearPerms(s.id);
        await this.pushAll(
          {
            aps: {
              alert: { "title-loc-key": "PUSH_DONE_TITLE", "title-loc-args": [s.title], body: clip(message, 180) ?? "" },
              sound: "default",
              category: "SESSION_DONE",
              "thread-id": s.id,
            },
            sessionId: s.id,
            ...(viaWatch ? { reply: clip(message, 2000) } : {}),
          },
          `done-${s.id}`,
        );
        break;
      }
      default:
        return error(400, "bad_event");
    }
    s.updatedAt = now;
    await Promise.all([this.saveSessions(), this.savePerms()]);
    return json({ ok: true });
  }

  private async setUsage(b: any): Promise<Response> {
    const win = (w: any) =>
      w && Number.isFinite(Number(w.used_percentage)) ? { pct: Math.max(0, Math.min(100, Number(w.used_percentage))), resetsAt: Number(w.resets_at) || 0 } : undefined;
    this.usage = { fiveHour: win(b.five_hour), sevenDay: win(b.seven_day), updatedAt: Date.now() };
    await this.ctx.storage.put("usage", this.usage);
    return json({ ok: true });
  }

  private async openPermission(b: any, source: "hook" | "channel", requestId?: string): Promise<Response> {
    const sessionId = String(b.session_id ?? "");
    const session = this.sessions[sessionId];
    if (!session) return error(404, "no_session");
    // Com channel conectado, a aprovação vai por ele (não bloqueia o terminal).
    if (source === "hook" && this.ctx.getWebSockets(`s:${sessionId}`).length) return json({ skip: true });
    const id = randomHex(6);
    const p: Permission = {
      id,
      sessionId,
      tool: clip(b.tool_name, 40) ?? "Tool",
      description: clip(b.description, 300) ?? "",
      preview: clip(b.input_preview, 600),
      source,
      requestId,
      createdAt: Date.now(),
    };
    this.perms[id] = p;
    Object.assign(session, { state: "waiting", waitingMessage: p.description || p.tool, updatedAt: Date.now() });
    session.lastWaitingPushAt = Date.now();
    await Promise.all([this.savePerms(), this.saveSessions()]);
    await this.pushAll(
      {
        aps: {
          alert: { "title-loc-key": "PUSH_PERMISSION_TITLE", "title-loc-args": [session.title], body: `${p.tool}: ${p.description || p.preview || ""}`.slice(0, 200) },
          sound: "default",
          category: "PERMISSION",
          "thread-id": sessionId,
        },
        sessionId,
        permissionId: id,
      },
      `perm-${id}`,
    );
    await this.scheduleAlarm();
    return json({ id });
  }

  private async waitDecision(id: string): Promise<Response> {
    type Waited = { decision: "allow" | "deny" | null; gone?: boolean; pending?: boolean };
    const ready = (): Waited | null => {
      const p = this.perms[id];
      return !p ? { decision: null, gone: true } : p.decision ? { decision: p.decision } : null;
    };
    let r: Waited | null = ready();
    if (!r) {
      await new Promise<void>((resolve) => {
        const set = this.waiters.get(id) ?? new Set();
        const done = () => {
          clearTimeout(timer);
          set.delete(done);
          resolve();
        };
        const timer = setTimeout(done, WAIT_MS);
        set.add(done);
        this.waiters.set(id, set);
      });
      r = ready() ?? { decision: null, pending: true };
    }
    if (r?.decision) {
      delete this.perms[id];
      await this.savePerms();
    }
    return json(r);
  }

  private async dropPermission(id: string): Promise<void> {
    if (this.perms[id]) {
      delete this.perms[id];
      await this.savePerms();
    }
    this.wake(id);
  }

  // ------------------------------------------------------------------ channel (WebSocket)

  private acceptChannel(request: Request, hostId: string, sessionId: string): Response {
    if (request.headers.get("upgrade") !== "websocket") return error(426, "expected_websocket");
    if (!/^[\w-]{1,100}$/.test(sessionId)) return error(400, "bad_session");
    const pair = new WebSocketPair();
    this.ctx.acceptWebSocket(pair[1], [`s:${sessionId}`, `h:${hostId}`]);
    pair[1].serializeAttachment({ sessionId, hostId });
    return new Response(null, { status: 101, webSocket: pair[0] });
  }

  async webSocketMessage(ws: WebSocket, raw: string | ArrayBuffer): Promise<void> {
    if (typeof raw !== "string") return;
    const { sessionId, hostId } = ws.deserializeAttachment() as { sessionId: string; hostId: string };
    let msg: any;
    try {
      msg = JSON.parse(raw);
    } catch {
      return;
    }
    if (msg.type === "permission_request" && /^[a-km-z]{5}$/.test(msg.request_id ?? "")) {
      if (!this.sessions[sessionId]) this.ensureSession(hostId, { session_id: sessionId, cwd: msg.cwd ?? "" });
      await this.openPermission({ session_id: sessionId, ...msg }, "channel", msg.request_id);
    }
  }

  async webSocketClose(ws: WebSocket, code: number): Promise<void> {
    try {
      ws.close(code === 1005 ? 1000 : code, "bye");
    } catch {}
  }

  // ------------------------------------------------------------------ push e limpeza

  private async pushWaiting(s: Session, message: string) {
    const hasPending = Object.values(this.perms).some((p) => p.sessionId === s.id && !p.decision);
    if (hasPending || (s.lastWaitingPushAt && Date.now() - s.lastWaitingPushAt < WAITING_PUSH_GAP_MS)) return;
    s.lastWaitingPushAt = Date.now();
    await this.pushAll(
      {
        aps: {
          alert: { "title-loc-key": "PUSH_WAITING_TITLE", "title-loc-args": [s.title], body: clip(message, 180) ?? "" },
          sound: "default",
          "thread-id": s.id,
        },
        sessionId: s.id,
      },
      `wait-${s.id}`,
    );
  }

  private async pushAll(payload: Record<string, unknown>, collapseId: string) {
    const targets = this.meta?.pushTargets ?? [];
    if (!targets.length) return;
    const results = await Promise.all(targets.map((t) => sendPush(this.env, t, payload, { collapseId })));
    const alive = targets.filter((_, i) => results[i] !== "gone");
    if (alive.length !== targets.length) {
      this.meta!.pushTargets = alive;
      await this.ctx.storage.put("meta", this.meta);
    }
  }

  private clearPerms(sessionId: string) {
    for (const [id, p] of Object.entries(this.perms)) {
      if (p.sessionId === sessionId && !p.decision) {
        delete this.perms[id];
        this.wake(id);
      }
    }
  }

  private wake(id: string) {
    for (const fn of this.waiters.get(id) ?? []) fn();
    this.waiters.delete(id);
  }

  private prune() {
    const now = Date.now();
    for (const [id, s] of Object.entries(this.sessions)) {
      if (now - s.updatedAt > SESSION_TTL_MS && !this.ctx.getWebSockets(`s:${id}`).length) delete this.sessions[id];
    }
    for (const [id, p] of Object.entries(this.perms)) if (now - p.createdAt > PERMISSION_TTL_MS) delete this.perms[id];
  }

  private async scheduleAlarm() {
    if ((await this.ctx.storage.getAlarm()) == null) await this.ctx.storage.setAlarm(Date.now() + PERMISSION_TTL_MS);
  }

  async alarm(): Promise<void> {
    if (!this.meta) return;
    this.prune();
    await Promise.all([this.saveSessions(), this.savePerms()]);
    if (Object.keys(this.perms).length) await this.scheduleAlarm();
  }

  private saveSessions() {
    return this.ctx.storage.put("sessions", this.sessions);
  }

  private savePerms() {
    return this.ctx.storage.put("perms", this.perms);
  }
}

async function body(request: Request): Promise<any> {
  try {
    return await request.json();
  } catch {
    return {};
  }
}
