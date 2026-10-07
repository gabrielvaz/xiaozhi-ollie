import Foundation

/// Resposta de `GET /v1/state`.
struct RelayState: Codable, Equatable, Sendable {
    var now: Double
    var sessions: [Session]
    var permissions: [Permission]
    var usage: Usage?
    var hosts: [Host]

    var working: Int { sessions.filter { $0.state == .working }.count }
    var waiting: Int { sessions.filter { $0.state == .waiting }.count }

    func permissions(for sessionId: String) -> [Permission] {
        permissions.filter { $0.sessionId == sessionId }
    }
}

enum SessionState: String, Codable, Sendable {
    case working, waiting, done, idle

    init(from decoder: Decoder) throws {
        let raw = try decoder.singleValueContainer().decode(String.self)
        self = SessionState(rawValue: raw) ?? .idle
    }
}

struct Activity: Codable, Equatable, Hashable, Sendable {
    var tool: String
    var target: String?
}

struct Session: Codable, Identifiable, Equatable, Hashable, Sendable {
    var id: String
    var host: String
    var project: String
    var title: String
    var state: SessionState
    var activity: Activity?
    var turnStartedAt: Double?
    var lastDurationSec: Int?
    var lastMessage: String?
    var lastPrompt: String?
    var waitingMessage: String?
    var channel: Bool
    var startedAt: Double
    var updatedAt: Double

    var turnStart: Date? { turnStartedAt.map { Date(timeIntervalSince1970: $0 / 1000) } }
    var updated: Date { Date(timeIntervalSince1970: updatedAt / 1000) }
}

struct Permission: Codable, Identifiable, Equatable, Hashable, Sendable {
    var id: String
    var sessionId: String
    var sessionTitle: String
    var tool: String
    var description: String
    var preview: String?
    var createdAt: Double
}

struct UsageWindow: Codable, Equatable, Sendable {
    var pct: Double
    /// Segundos desde 1970 (vem do `resets_at` da status line).
    var resetsAt: Double

    var resetDate: Date? { resetsAt > 0 ? Date(timeIntervalSince1970: resetsAt) : nil }
}

struct Usage: Codable, Equatable, Sendable {
    var fiveHour: UsageWindow?
    var sevenDay: UsageWindow?
    var updatedAt: Double

    var updated: Date { Date(timeIntervalSince1970: updatedAt / 1000) }
}

struct Host: Codable, Identifiable, Equatable, Hashable, Sendable {
    var id: String
    var name: String
    var pairedAt: Double
    var lastSeenAt: Double
}

struct PairStart: Codable, Equatable, Sendable {
    var code: String
    var expiresAt: Double
    var token: String

    var expires: Date { Date(timeIntervalSince1970: expiresAt / 1000) }
}

struct PairStatus: Codable, Sendable {
    var paired: Bool
    var hosts: [Host]
}
