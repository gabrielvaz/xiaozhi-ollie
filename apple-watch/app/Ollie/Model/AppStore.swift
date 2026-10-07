import Foundation
import Observation
import WidgetKit

@MainActor
@Observable
final class AppStore {
    static let shared = AppStore()

    // Pareamento
    private(set) var token: String? = Keychain.load()
    private(set) var pairing: PairStart?
    private(set) var pairingError = false

    // Estado
    private(set) var state: RelayState?
    private(set) var offline = false
    private(set) var lastRefresh: Date?

    /// Sessão que recebeu um prompt do relógio e cuja resposta deve ser lida.
    private(set) var awaitingReply: [String: Date] = [:]
    private(set) var isSpeaking = false

    var speakReplies: Bool {
        didSet { UserDefaults.standard.set(speakReplies, forKey: "speakReplies") }
    }

    private var pushToken: String?
    private var pollTask: Task<Void, Never>?

    var isPaired: Bool { token != nil }
    var client: RelayClient { RelayClient(token: token) }

    init() {
        speakReplies = UserDefaults.standard.object(forKey: "speakReplies") as? Bool ?? true
        Speaker.shared.onChange = { [weak self] speaking in self?.isSpeaking = speaking }
        if let cached = UserDefaults.standard.data(forKey: "lastState"),
           let s = try? JSONDecoder().decode(RelayState.self, from: cached) {
            state = s
        }
    }

    // MARK: Pareamento

    func startPairing() async {
        pairingError = false
        do {
            pairing = try await RelayClient().startPairing()
        } catch {
            pairingError = true
        }
    }

    /// Consulta até o computador digitar o código; troca o código quando expira.
    func waitForPairing() async {
        while !Task.isCancelled, token == nil {
            if pairing == nil || (pairing!.expires.timeIntervalSinceNow < 5) {
                await startPairing()
            }
            if let p = pairing, let status = try? await RelayClient(token: p.token).pairStatus(), status.paired {
                Keychain.save(p.token)
                token = p.token
                pairing = nil
                await sendPushToken()
                await refresh()
                return
            }
            try? await Task.sleep(for: .seconds(2))
        }
    }

    func unpair() async {
        try? await client.unpair()
        forget()
    }

    func removeHost(_ host: Host) async {
        try? await client.removeHost(id: host.id)
        await refresh()
    }

    private func forget() {
        Keychain.delete()
        token = nil
        state = nil
        UserDefaults.standard.removeObject(forKey: "lastState")
        updateWidgets()
    }

    // MARK: Estado

    func refresh() async {
        guard token != nil else { return }
        do {
            let new = try await client.state()
            offline = false
            lastRefresh = .now
            apply(new)
        } catch RelayError.unauthorized {
            forget()
        } catch {
            offline = true
        }
    }

    /// Atualiza a cada 3 s enquanto o app está na frente.
    func startPolling() {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                await self?.refresh()
                try? await Task.sleep(for: .seconds(3))
            }
        }
    }

    func stopPolling() {
        pollTask?.cancel()
        pollTask = nil
    }

    private func apply(_ new: RelayState) {
        // Resposta de um prompt feito pelo relógio: lê quando a sessão termina.
        for (id, since) in awaitingReply {
            guard let s = new.sessions.first(where: { $0.id == id }) else {
                awaitingReply[id] = nil
                continue
            }
            if s.state == .done, s.updated > since, let msg = s.lastMessage {
                awaitingReply[id] = nil
                if speakReplies { Speaker.shared.speak(msg) }
            }
        }
        guard new != state else { return }
        state = new
        if let data = try? JSONEncoder().encode(new) { UserDefaults.standard.set(data, forKey: "lastState") }
        updateWidgets()
    }

    // MARK: Ações

    func send(prompt: String, to session: Session) async throws {
        let text = prompt.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        try await client.prompt(sessionId: session.id, text: text)
        awaitingReply[session.id] = .now
        await refresh()
    }

    func decide(_ permission: Permission, allow: Bool) async throws {
        try await client.decide(permissionId: permission.id, allow: allow)
        await refresh()
    }

    func decide(permissionId: String, allow: Bool) async {
        try? await client.decide(permissionId: permissionId, allow: allow)
    }

    /// Resposta que veio no push (o app estava aberto).
    func handlePushReply(sessionId: String, reply: String) {
        guard awaitingReply[sessionId] != nil else { return }
        awaitingReply[sessionId] = nil
        if speakReplies { Speaker.shared.speak(reply) }
    }

    func speak(_ text: String) { Speaker.shared.speak(text) }
    func stopSpeaking() { Speaker.shared.stop() }

    // MARK: Push

    func setPushToken(_ data: Data) async {
        pushToken = data.map { String(format: "%02x", $0) }.joined()
        await sendPushToken()
    }

    private func sendPushToken() async {
        guard token != nil, let pushToken else { return }
        try? await client.registerPush(token: pushToken, sandbox: Self.usesSandboxAPNs)
    }

    /// Builds de desenvolvimento usam o APNs de sandbox; TestFlight e App Store, o de produção.
    static var usesSandboxAPNs: Bool {
        guard let url = Bundle.main.url(forResource: "embedded", withExtension: "mobileprovision"),
              let data = try? Data(contentsOf: url),
              let text = String(data: data, encoding: .isoLatin1) else { return false }
        return text.range(of: "<key>aps-environment</key>\\s*<string>development</string>", options: .regularExpression) != nil
    }

    // MARK: Complicações

    private func updateWidgets() {
        let snap = WidgetSnapshot(state: state, paired: token != nil)
        guard snap != WidgetSnapshot.cached() else { return }
        snap.cache()
        WidgetCenter.shared.reloadAllTimelines()
    }
}
