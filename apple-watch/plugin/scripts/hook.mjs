#!/usr/bin/env node
// One script for every hook event. It reads the hook JSON on stdin and tells the relay.
// It never blocks Claude Code on a network problem: short timeouts, silent exit.
import { loadConfig, readStdin, recordSession, relay, toolTarget } from "./lib.mjs";

const input = JSON.parse((await readStdin()) || "{}");
const event = input.hook_event_name;
const config = loadConfig();

if (event === "SessionStart" && input.session_id) recordSession(input.session_id);
if (!config?.token || !input.session_id) process.exit(0);

const common = { event, session_id: input.session_id, cwd: input.cwd };

switch (event) {
  case "SessionStart":
    await relay(config, "POST", "/v1/events", { ...common, session_title: input.session_title });
    break;
  case "UserPromptSubmit":
    await relay(config, "POST", "/v1/events", { ...common, prompt: String(input.prompt ?? "").slice(0, 500) });
    break;
  case "PreToolUse":
  case "PostToolUse":
    await relay(config, "POST", "/v1/events", { ...common, tool_name: shortTool(input.tool_name), target: toolTarget(input.tool_name, input.tool_input) });
    break;
  case "Notification":
    await relay(config, "POST", "/v1/events", { ...common, notification_type: input.notification_type, message: input.message });
    break;
  case "Stop":
    await relay(config, "POST", "/v1/events", { ...common, last_assistant_message: String(input.last_assistant_message ?? "").slice(0, 4000) });
    break;
  case "SessionEnd":
    await relay(config, "POST", "/v1/events", common, 2000);
    break;
  case "PermissionRequest":
    await permission();
    break;
}

function shortTool(name = "") {
  // mcp__server__tool → tool
  return name.startsWith("mcp__") ? name.split("__").pop() : name;
}

/**
 * Without a channel, this hook is the only way to approve from the watch. The terminal
 * dialog waits for it, so it only waits OLLIE_WATCH_PERMISSION_WAIT seconds (default 60)
 * and then hands the decision back to the terminal.
 */
async function permission() {
  const waitSec = Math.min(300, Math.max(0, Number(process.env.OLLIE_WATCH_PERMISSION_WAIT ?? config.permissionWait ?? 60)));
  if (!waitSec) return;
  let preview = "";
  try {
    preview = JSON.stringify(input.tool_input ?? {}).slice(0, 600);
  } catch {}
  const open = await relay(config, "POST", "/v1/permissions", {
    session_id: input.session_id,
    tool_name: shortTool(input.tool_name),
    description: toolTarget(input.tool_name, input.tool_input),
    input_preview: preview,
  });
  const id = open.body?.id;
  if (!id) return; // channel connected (skip) or relay unreachable
  const deadline = Date.now() + waitSec * 1000;
  while (Date.now() < deadline) {
    const r = await relay(config, "GET", `/v1/permissions/${id}/wait`, undefined, 30000);
    if (r.body?.decision === "allow" || r.body?.decision === "deny") {
      process.stdout.write(
        JSON.stringify({ hookSpecificOutput: { hookEventName: "PermissionRequest", decision: { behavior: r.body.decision } } }),
      );
      return;
    }
    if (r.body?.gone) return;
    if (r.status !== 200) await new Promise((res) => setTimeout(res, 2000));
  }
  await relay(config, "DELETE", `/v1/permissions/${id}`, undefined, 2000);
}
