import SwiftUI

struct SettingsView: View {
    @Environment(AppStore.self) private var store
    @State private var confirmUnpair = false

    var body: some View {
        @Bindable var store = store
        List {
            Section {
                Toggle("Read answers aloud", isOn: $store.speakReplies)
            } footer: {
                Text("After you talk to a session, the watch reads Claude's answer when the turn ends.")
            }

            Section("Computers") {
                ForEach(store.state?.hosts ?? []) { host in
                    VStack(alignment: .leading) {
                        Text(host.name)
                        Text("Seen \(Format.relative(Date(timeIntervalSince1970: host.lastSeenAt / 1000)))")
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                    .swipeActions {
                        Button(role: .destructive) {
                            Task { await store.removeHost(host) }
                        } label: {
                            Label("Remove", systemImage: "trash")
                        }
                    }
                }
            }

            Section {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Talk and approve in parallel with the terminal").font(.footnote.bold())
                    Text("Start Claude Code with the channel:").font(.caption2).foregroundStyle(.secondary)
                    CommandText("claude --dangerously-load-development-channels plugin:ollie-watch@ollie")
                    Text("Plan usage on the watch:").font(.caption2).foregroundStyle(.secondary)
                    CommandText("/ollie-watch:pair --statusline")
                }
            } header: {
                Text("Setup")
            }

            Section {
                Button("Unpair this watch", role: .destructive) { confirmUnpair = true }
            } footer: {
                Text("Version \(Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "") (\(Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? ""))")
            }
        }
        .navigationTitle("Settings")
        .confirmationDialog("Unpair this watch?", isPresented: $confirmUnpair) {
            Button("Unpair", role: .destructive) { Task { await store.unpair() } }
        } message: {
            Text("Your computers stop sending sessions to this watch.")
        }
    }
}
