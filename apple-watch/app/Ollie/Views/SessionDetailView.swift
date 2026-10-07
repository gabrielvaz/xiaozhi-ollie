import SwiftUI

struct SessionDetailView: View {
    @Environment(AppStore.self) private var store
    let sessionId: String

    @State private var sending = false
    @State private var notice: LocalizedStringKey?

    private var session: Session? { store.state?.sessions.first { $0.id == sessionId } }

    var body: some View {
        ScrollView {
            if let session {
                VStack(alignment: .leading, spacing: 10) {
                    header(session)

                    ForEach(store.state?.permissions(for: session.id) ?? []) { permission in
                        PermissionCard(permission: permission, showSession: false)
                    }

                    actions(session)

                    if let notice {
                        Text(notice)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }

                    if let prompt = session.lastPrompt {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("You").font(.caption2).foregroundStyle(.secondary)
                            Text(prompt).font(.footnote)
                        }
                    }

                    if let message = session.lastMessage {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Claude").font(.caption2).foregroundStyle(Color.clawd)
                            Text(message).font(.footnote)
                        }
                    }

                    Text("\(session.project) · \(session.host)")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
            } else {
                Text("This session ended.")
                    .foregroundStyle(.secondary)
            }
        }
        .navigationTitle(session?.project ?? "")
    }

    @ViewBuilder
    private func header(_ s: Session) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Label(s.state.label, systemImage: s.state.symbol)
                .foregroundStyle(s.state.color)
                .font(.footnote.bold())
            Text(s.title)
                .font(.headline)
            if s.state == .working {
                Text(s.activity.map(Format.activity) ?? String(localized: "Thinking…"))
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            ElapsedText(session: s)
                .font(.title3.monospacedDigit())
        }
    }

    @ViewBuilder
    private func actions(_ s: Session) -> some View {
        HStack(spacing: 8) {
            TextFieldLink(prompt: Text("Ask Claude…")) {
                Label("Talk", systemImage: "mic.fill")
                    .frame(maxWidth: .infinity)
            } onSubmit: { text in
                Task { await send(text, to: s) }
            }
            .disabled(!s.channel || sending)
            .buttonStyle(.borderedProminent)
            .tint(.clawd)

            if store.isSpeaking {
                Button { store.stopSpeaking() } label: { Image(systemName: "stop.fill") }
                    .accessibilityLabel(Text("Stop"))
            } else if let message = s.lastMessage {
                Button { store.speak(message) } label: { Image(systemName: "speaker.wave.2.fill") }
                    .accessibilityLabel(Text("Listen"))
            }
        }
        if !s.channel {
            Text("To talk to this session, start Claude Code with the Ollie Watch channel. See Settings.")
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
    }

    private func send(_ text: String, to session: Session) async {
        sending = true
        defer { sending = false }
        do {
            try await store.send(prompt: text, to: session)
            notice = store.speakReplies ? "Sent. I'll read the answer when it's done." : "Sent."
        } catch RelayError.noChannel {
            notice = "This session has no channel connected."
        } catch {
            notice = "Couldn't send. Check the connection."
        }
    }
}

struct PermissionCard: View {
    @Environment(AppStore.self) private var store
    let permission: Permission
    let showSession: Bool
    @State private var busy = false

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Label("Permission", systemImage: "hand.raised.fill")
                .font(.caption.bold())
                .foregroundStyle(.yellow)
            if showSession, !permission.sessionTitle.isEmpty {
                Text(permission.sessionTitle).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
            }
            Text(permission.tool).font(.headline)
            if !permission.description.isEmpty {
                Text(permission.description).font(.footnote).lineLimit(4)
            }
            HStack {
                Button(role: .destructive) { decide(false) } label: { Text("Deny") }
                Button { decide(true) } label: { Text("Allow") }
                    .tint(.green)
            }
            .disabled(busy)
        }
        .padding(.vertical, 4)
    }

    private func decide(_ allow: Bool) {
        busy = true
        Task {
            try? await store.decide(permission, allow: allow)
            busy = false
        }
    }
}
