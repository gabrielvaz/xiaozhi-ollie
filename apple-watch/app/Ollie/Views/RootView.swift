import SwiftUI

struct RootView: View {
    @Environment(AppStore.self) private var store
    @State private var path: [Route] = []

    var body: some View {
        NavigationStack(path: $path) {
            if store.isPaired {
                SessionsView()
                    .task {
                        try? await Task.sleep(for: .milliseconds(300))
                        if path.isEmpty { path = RootView.initialPath }
                    }
            } else {
                PairingView()
            }
        }
    }

    /// Debug: `-OllieOpenRoute usage|settings|session:<id>` abre uma tela direto (capturas de tela).
    private static var initialPath: [Route] {
        #if DEBUG
        guard let raw = UserDefaults.standard.string(forKey: "OllieOpenRoute") else { return [] }
        switch raw {
        case "usage": return [.usage]
        case "settings": return [.settings]
        default: return raw.hasPrefix("session:") ? [.session(String(raw.dropFirst(8)))] : []
        }
        #else
        return []
        #endif
    }
}

struct PairingView: View {
    @Environment(AppStore.self) private var store

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 10) {
                Text("Pair with Claude Code")
                    .font(.headline)

                if let p = store.pairing {
                    Text(p.code)
                        .font(.system(size: 30, weight: .bold, design: .monospaced))
                        .minimumScaleFactor(0.6)
                        .lineLimit(1)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 6)
                        .background(.quaternary, in: RoundedRectangle(cornerRadius: 10))
                        .accessibilityLabel(Text(p.code.map(String.init).joined(separator: " ")))
                    Text("In Claude Code, run:")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    CommandText("/ollie-watch:pair \(p.code)")
                    Text("The code changes every 10 minutes.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                } else if store.pairingError {
                    Label("Can't reach the relay", systemImage: "wifi.exclamationmark")
                        .foregroundStyle(.orange)
                    Button("Try again") { Task { await store.startPairing() } }
                } else {
                    ProgressView()
                        .frame(maxWidth: .infinity)
                }

                Divider().padding(.vertical, 4)

                Text("First time? Install the plugin:")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                CommandText("/plugin marketplace add gabrielvaz/xiaozhi-ollie")
                CommandText("/plugin install ollie-watch@ollie")
            }
        }
        .navigationTitle("Ollie")
        .task { await store.waitForPairing() }
    }
}

struct CommandText: View {
    let text: String
    init(_ text: String) { self.text = text }

    var body: some View {
        Text(verbatim: text)
            .font(.system(.caption2, design: .monospaced))
            .padding(6)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color.clawd.opacity(0.18), in: RoundedRectangle(cornerRadius: 6))
    }
}
