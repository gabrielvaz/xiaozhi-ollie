import Foundation

enum RelayError: Error, Equatable {
    case unauthorized
    case noChannel
    case notFound
    case http(Int)
    case offline
}

/// Fala com o relay por HTTPS comum. (No watchOS, WebSocket só é permitido com áudio tocando.)
struct RelayClient: Sendable {
    /// Endereço do relay, do Info.plist (OllieRelayURL), que vem de Config/*.xcconfig
    static let defaultBase: URL = {
        let texto = Bundle.main.object(forInfoDictionaryKey: "OllieRelayURL") as? String ?? ""
        return URL(string: texto) ?? URL(string: "https://ollie-watch-relay.example.workers.dev")!
    }()

    var base: URL = RelayClient.defaultBase
    var token: String?

    private static let session: URLSession = {
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 15
        config.waitsForConnectivity = false
        return URLSession(configuration: config)
    }()

    func startPairing() async throws -> PairStart {
        try await send("POST", "/v1/pair/start")
    }

    func pairStatus() async throws -> PairStatus {
        try await send("GET", "/v1/pair/status")
    }

    func state() async throws -> RelayState {
        try await send("GET", "/v1/state")
    }

    func prompt(sessionId: String, text: String) async throws {
        let _: Ack = try await send("POST", "/v1/sessions/\(escape(sessionId))/prompt", body: ["text": text])
    }

    func decide(permissionId: String, allow: Bool) async throws {
        let _: Ack = try await send("POST", "/v1/permissions/\(escape(permissionId))", body: ["decision": allow ? "allow" : "deny"])
    }

    func registerPush(token: String, sandbox: Bool) async throws {
        let _: Ack = try await send("POST", "/v1/watch/push-token", body: PushBody(token: token, sandbox: sandbox))
    }

    func removeHost(id: String) async throws {
        let _: Ack = try await send("DELETE", "/v1/hosts/\(escape(id))")
    }

    func unpair() async throws {
        let _: Ack = try await send("DELETE", "/v1/pair")
    }

    // MARK: -

    private struct Ack: Decodable {}
    private struct PushBody: Encodable { var token: String; var sandbox: Bool }

    private func escape(_ s: String) -> String {
        s.addingPercentEncoding(withAllowedCharacters: .urlPathAllowed.subtracting(CharacterSet(charactersIn: "/"))) ?? s
    }

    private func send<T: Decodable>(_ method: String, _ path: String) async throws -> T {
        try await send(method, path, body: Optional<[String: String]>.none)
    }

    private func send<T: Decodable, B: Encodable>(_ method: String, _ path: String, body: B?) async throws -> T {
        var request = URLRequest(url: base.appending(path: path))
        request.httpMethod = method
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        if let token { request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization") }
        if let body { request.httpBody = try JSONEncoder().encode(body) }

        let data: Data
        let response: URLResponse
        do {
            (data, response) = try await Self.session.data(for: request)
        } catch {
            throw RelayError.offline
        }
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        switch status {
        case 200..<300:
            return try JSONDecoder().decode(T.self, from: data)
        case 401: throw RelayError.unauthorized
        case 404: throw RelayError.notFound
        case 409: throw RelayError.noChannel
        default: throw RelayError.http(status)
        }
    }
}
