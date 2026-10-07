import { DurableObject } from "cloudflare:workers";
import type { Env } from "./util";

// Sem 0/O, 1/I/L: o código é lido no relógio e digitado no terminal.
const ALPHABET = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
const TTL_MS = 10 * 60 * 1000;

interface Entry {
  pairingId: string;
  expiresAt: number;
}

/** Instância única: mapeia código curto → pareamento por 10 minutos. */
export class CodesDO extends DurableObject<Env> {
  async issue(pairingId: string): Promise<{ code: string; expiresAt: number }> {
    for (let attempt = 0; attempt < 20; attempt++) {
      const raw = crypto.getRandomValues(new Uint8Array(6));
      const chars = [...raw].map((b) => ALPHABET[b % ALPHABET.length]).join("");
      const code = `${chars.slice(0, 3)}-${chars.slice(3)}`;
      const existing = await this.ctx.storage.get<Entry>(code);
      if (existing && existing.expiresAt > Date.now()) continue;
      const expiresAt = Date.now() + TTL_MS;
      await this.ctx.storage.put<Entry>(code, { pairingId, expiresAt });
      await this.scheduleCleanup();
      return { code, expiresAt };
    }
    throw new Error("no free code");
  }

  /** Consome o código: só funciona uma vez. */
  async take(code: string): Promise<string | null> {
    const key = normalize(code);
    if (!key) return null;
    const entry = await this.ctx.storage.get<Entry>(key);
    if (!entry) return null;
    await this.ctx.storage.delete(key);
    return entry.expiresAt > Date.now() ? entry.pairingId : null;
  }

  async alarm(): Promise<void> {
    const now = Date.now();
    const all = await this.ctx.storage.list<Entry>();
    const stale = [...all].filter(([, e]) => e.expiresAt <= now).map(([k]) => k);
    if (stale.length) await this.ctx.storage.delete(stale);
    if (all.size > stale.length) await this.scheduleCleanup();
  }

  private async scheduleCleanup(): Promise<void> {
    if ((await this.ctx.storage.getAlarm()) == null) {
      await this.ctx.storage.setAlarm(Date.now() + TTL_MS);
    }
  }
}

export function normalize(code: string): string | null {
  const c = code.toUpperCase().replace(/[^0-9A-Z]/g, "");
  if (c.length !== 6 || [...c].some((ch) => !ALPHABET.includes(ch))) return null;
  return `${c.slice(0, 3)}-${c.slice(3)}`;
}
