import Foundation
import SwiftUI

enum Format {
    /// "Editando App.swift", "Rodando npm test"…
    static func activity(_ a: Activity) -> String {
        // A descrição de um comando já é uma frase ("Run the test suite").
        if a.tool == "Bash", let target = a.target, !target.isEmpty { return target }
        let verb: String
        switch a.tool {
        case "Edit", "MultiEdit", "NotebookEdit": verb = String(localized: "Editing")
        case "Write": verb = String(localized: "Writing")
        case "Read": verb = String(localized: "Reading")
        case "Bash": verb = String(localized: "Running")
        case "Grep", "Glob": verb = String(localized: "Searching")
        case "WebFetch", "WebSearch": verb = String(localized: "Browsing")
        case "Task", "Agent": verb = String(localized: "Delegating")
        case "TodoWrite": return String(localized: "Planning")
        default: verb = a.tool
        }
        guard let target = a.target, !target.isEmpty else { return verb }
        return "\(verb) \(target)"
    }

    /// 0:42, 12:05, 1:03:20
    static func elapsed(_ seconds: Int) -> String {
        let s = max(0, seconds)
        let h = s / 3600, m = (s % 3600) / 60, sec = s % 60
        return h > 0 ? String(format: "%d:%02d:%02d", h, m, sec) : String(format: "%d:%02d", m, sec)
    }

    static func percent(_ value: Double) -> String {
        "\(Int(value.rounded()))%"
    }

    static func relative(_ date: Date, now: Date = .now) -> String {
        let f = RelativeDateTimeFormatter()
        f.unitsStyle = .short
        return f.localizedString(for: date, relativeTo: now)
    }
}

extension SessionState {
    var label: LocalizedStringKey {
        switch self {
        case .working: "Working"
        case .waiting: "Waiting for you"
        case .done: "Done"
        case .idle: "Idle"
        }
    }

    var color: Color {
        switch self {
        case .working: .orange
        case .waiting: .yellow
        case .done: .green
        case .idle: .gray
        }
    }

    var symbol: String {
        switch self {
        case .working: "sparkle"
        case .waiting: "hand.raised.fill"
        case .done: "checkmark.circle.fill"
        case .idle: "moon.zzz.fill"
        }
    }
}

extension Color {
    /// Laranja do Clawd.
    static let clawd = Color(red: 0.85, green: 0.47, blue: 0.34)

    static func usage(_ pct: Double) -> Color {
        pct >= 90 ? .red : pct >= 70 ? .orange : .green
    }
}
