import type { Env } from "./util";

let cached: { jwt: string; at: number; keyId: string } | null = null;

function b64url(data: ArrayBuffer | Uint8Array | string): string {
  const bytes = typeof data === "string" ? new TextEncoder().encode(data) : new Uint8Array(data);
  let s = "";
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

async function providerToken(env: Env): Promise<string | null> {
  if (!env.APNS_KEY_ID || !env.APNS_KEY_P8) return null;
  const now = Math.floor(Date.now() / 1000);
  // A Apple aceita o mesmo JWT por até 1 h e recusa renovações a menos de 20 min.
  if (cached && cached.keyId === env.APNS_KEY_ID && now - cached.at < 50 * 60) return cached.jwt;

  const pem = env.APNS_KEY_P8.replace(/-----[^-]+-----/g, "").replace(/\s+/g, "");
  const der = Uint8Array.from(atob(pem), (c) => c.charCodeAt(0));
  const key = await crypto.subtle.importKey("pkcs8", der, { name: "ECDSA", namedCurve: "P-256" }, false, ["sign"]);
  const head = b64url(JSON.stringify({ alg: "ES256", kid: env.APNS_KEY_ID }));
  const claims = b64url(JSON.stringify({ iss: env.APNS_TEAM_ID, iat: now }));
  const sig = await crypto.subtle.sign({ name: "ECDSA", hash: "SHA-256" }, key, new TextEncoder().encode(`${head}.${claims}`));
  const jwt = `${head}.${claims}.${b64url(sig)}`;
  cached = { jwt, at: now, keyId: env.APNS_KEY_ID };
  return jwt;
}

export interface PushTarget {
  token: string;
  sandbox: boolean;
}

export type PushResult = "sent" | "gone" | "failed" | "disabled";

export async function sendPush(
  env: Env,
  target: PushTarget,
  payload: Record<string, unknown>,
  opts: { collapseId?: string } = {},
): Promise<PushResult> {
  const jwt = await providerToken(env);
  if (!jwt) return "disabled";
  const host = target.sandbox ? "api.sandbox.push.apple.com" : "api.push.apple.com";
  const headers: Record<string, string> = {
    authorization: `bearer ${jwt}`,
    "apns-topic": env.APNS_TOPIC,
    "apns-push-type": "alert",
    "apns-priority": "10",
    "content-type": "application/json",
  };
  if (opts.collapseId) headers["apns-collapse-id"] = opts.collapseId.slice(0, 64);
  try {
    const res = await fetch(`https://${host}/3/device/${target.token}`, {
      method: "POST",
      headers,
      body: JSON.stringify(payload),
    });
    if (res.ok) return "sent";
    const body = await res.text();
    console.warn("apns", res.status, body);
    if (res.status === 410 || body.includes("BadDeviceToken") || body.includes("DeviceTokenNotForTopic")) return "gone";
    return "failed";
  } catch (e) {
    console.warn("apns fetch", String(e));
    return "failed";
  }
}
