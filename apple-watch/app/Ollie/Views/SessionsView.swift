import SwiftUI

struct SessionsView: View {
    @Environment(AppStore.self) private var store

    var body: some View {
        List {
            if store.offline {
                Label("Offline. Showing the last update.", systemImage: "wifi.slash")
                    .font(.footnote)
                    .foregroundStyle(.orange)
                    .listRowBackground(Color.clear)
            }

            if let usage = store.state?.usage {
                NavigationLink(value: Route.usage) {
                    UsageSummary(usage: usage)
                }
            }

            ForEach(store.state?.permissions ?? []) { permission in
                PermissionCard(permission: permission, showSession: true)
            }

            if let sessions = store.state?.sessions, !sessions.isEmpty {
                ForEach(sessions) { session in
                    NavigationLink(value: Route.session(session.id)) {
                        SessionRow(session: session)
                    }
                }
            } else if store.state != nil {
                EmptySessions()
            } else {
                ProgressView().frame(maxWidth: .infinity)
            }
        }
        .navigationTitle("Sessions")
        .navigationDestination(for: Route.self) { route in
            switch route {
            case .session(let id): SessionDetailView(sessionId: id)
            case .usage: UsageView()
            case .settings: SettingsView()
            }
        }
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                NavigationLink(value: Route.settings) {
                    Image(systemName: "gearshape.fill")
                        .foregroundStyle(.white)
                }
                .accessibilityLabel(Text("Settings"))
            }
        }
        .refreshable { await store.refresh() }
        .task { store.startPolling() }
    }
}

enum Route: Hashable {
    case session(String)
    case usage
    case settings
}

struct SessionRow: View {
    let session: Session

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: 6) {
                Image(systemName: session.state.symbol)
                    .foregroundStyle(session.state.color)
                    .symbolEffect(.pulse, isActive: session.state == .working)
                    .font(.caption)
                Text(session.title)
                    .font(.headline)
                    .lineLimit(2)
            }
            Text(subtitle)
                .font(.footnote)
                .foregroundStyle(session.state == .waiting ? .yellow : .secondary)
                .lineLimit(2)
            HStack {
                Text(session.project)
                    .lineLimit(1)
                Spacer(minLength: 4)
                ElapsedText(session: session)
            }
            .font(.caption2)
            .foregroundStyle(.secondary)
        }
        .padding(.vertical, 2)
        .accessibilityElement(children: .combine)
    }

    private var subtitle: String {
        switch session.state {
        case .working: session.activity.map(Format.activity) ?? String(localized: "Thinking…")
        case .waiting: session.waitingMessage ?? String(localized: "Waiting for you")
        case .done: session.lastMessage ?? String(localized: "Done")
        case .idle: String(localized: "Idle")
        }
    }
}

/// Cronômetro ao vivo enquanto trabalha; duração da última tarefa quando termina.
struct ElapsedText: View {
    let session: Session

    var body: some View {
        if let start = session.turnStart, session.state == .working || session.state == .waiting {
            TimelineView(.periodic(from: .now, by: 1)) { context in
                Text(Format.elapsed(Int(context.date.timeIntervalSince(start))))
                    .monospacedDigit()
            }
        } else if session.state == .done, let d = session.lastDurationSec {
            Text(Format.elapsed(d)).monospacedDigit()
        } else {
            Text(Format.relative(session.updated))
        }
    }
}

struct EmptySessions: View {
    var body: some View {
        VStack(spacing: 6) {
            Image(systemName: "terminal")
                .font(.title2)
                .foregroundStyle(Color.clawd)
            Text("No active sessions")
                .font(.headline)
            Text("Start Claude Code on a paired computer and it shows up here.")
                .font(.footnote)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .listRowBackground(Color.clear)
    }
}
