export interface Env {
  PAIRING: DurableObjectNamespace<import("./pairing").PairingDO>;
  CODES: DurableObjectNamespace<import("./codes").CodesDO>;
  APNS_TOPIC: string;
  APNS_TEAM_ID: string;
  APNS_KEY_ID?: string;
  APNS_KEY_P8?: string;
}

export function json(data: unknown, status = 200): Response {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8" },
  });
}

export function error(status: number, code: string, message?: string): Response {
  return json({ error: code, message: message ?? code }, status);
}

export function randomHex(bytes: number): string {
  const a = crypto.getRandomValues(new Uint8Array(bytes));
  return [...a].map((b) => b.toString(16).padStart(2, "0")).join("");
}

export async function sha256(text: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/** Compara dois hex do mesmo tamanho sem atalho por tempo. */
export function safeEqual(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

/** Token no formato `<pairingId>.<segredo>`. */
export function parseToken(header: string | null): { pairingId: string; secret: string } | null {
  if (!header?.startsWith("Bearer ")) return null;
  const [pairingId, secret] = header.slice(7).trim().split(".");
  if (!/^[0-9a-f]{32}$/.test(pairingId ?? "") || !/^[0-9a-f]{64}$/.test(secret ?? "")) return null;
  return { pairingId, secret };
}

export function clip(text: string | undefined | null, max: number): string | undefined {
  if (!text) return undefined;
  const t = text.replace(/\s+/g, " ").trim();
  return t.length > max ? t.slice(0, max - 1) + "…" : t;
}
