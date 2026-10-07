import { error, json, parseToken, randomHex, type Env } from "./util";

export { PairingDO } from "./pairing";
export { CodesDO } from "./codes";

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    const path = url.pathname;

    if (path === "/" || path === "/health") return json({ ok: true, service: "ollie-watch-relay" });

    // Relógio: começa um pareamento e recebe o código para digitar no terminal.
    if (request.method === "POST" && path === "/v1/pair/start") {
      const pairingId = randomHex(16);
      const secret = randomHex(32);
      await env.PAIRING.get(env.PAIRING.idFromName(pairingId)).init(secret);
      const { code, expiresAt } = await env.CODES.get(env.CODES.idFromName("codes")).issue(pairingId);
      return json({ code, expiresAt, token: `${pairingId}.${secret}` });
    }

    // Plugin: troca o código por um token deste computador.
    if (request.method === "POST" && path === "/v1/pair/claim") {
      let b: any = {};
      try {
        b = await request.json();
      } catch {}
      const pairingId = await env.CODES.get(env.CODES.idFromName("codes")).take(String(b.code ?? ""));
      if (!pairingId) return error(404, "bad_code", "Code not found or expired");
      const host = await env.PAIRING.get(env.PAIRING.idFromName(pairingId)).addHost(String(b.host ?? "computer"));
      if (!host) return error(404, "bad_code");
      return json({ token: `${pairingId}.${host.secret}`, hostId: host.hostId });
    }

    if (!path.startsWith("/v1/")) return error(404, "not_found");
    const token = parseToken(request.headers.get("authorization"));
    if (!token) return error(401, "unauthorized");

    // O segredo segue num cabeçalho interno; o DO confere o hash.
    const headers = new Headers(request.headers);
    headers.delete("authorization");
    headers.set("x-ollie-secret", token.secret);
    const forwarded = new Request(request.url, {
      method: request.method,
      headers,
      body: request.body,
    });
    return env.PAIRING.get(env.PAIRING.idFromName(token.pairingId)).fetch(forwarded);
  },
} satisfies ExportedHandler<Env>;
