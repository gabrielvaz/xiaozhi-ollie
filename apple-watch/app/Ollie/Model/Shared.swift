import Foundation

/// O que as complicações mostram. Cada processo (app e extensão) guarda a sua cópia;
/// a extensão busca o estado no relay com o token do Keychain compartilhado.
struct WidgetSnapshot: Codable, Equatable {
    var fiveHourPct: Double?
    var fiveHourResetsAt: Double?
    var sevenDayPct: Double?
    var working: Int
    var waiting: Int
    var paired: Bool

    private static let key = "widgetSnapshot"

    static let empty = WidgetSnapshot(fiveHourPct: nil, fiveHourResetsAt: nil, sevenDayPct: nil, working: 0, waiting: 0, paired: false)

    init(fiveHourPct: Double?, fiveHourResetsAt: Double?, sevenDayPct: Double?, working: Int, waiting: Int, paired: Bool) {
        self.fiveHourPct = fiveHourPct
        self.fiveHourResetsAt = fiveHourResetsAt
        self.sevenDayPct = sevenDayPct
        self.working = working
        self.waiting = waiting
        self.paired = paired
    }

    init(state: RelayState?, paired: Bool) {
        self.init(
            fiveHourPct: state?.usage?.fiveHour?.pct,
            fiveHourResetsAt: state?.usage?.fiveHour?.resetsAt,
            sevenDayPct: state?.usage?.sevenDay?.pct,
            working: state?.working ?? 0,
            waiting: state?.waiting ?? 0,
            paired: paired
        )
    }

    static func cached() -> WidgetSnapshot {
        guard let data = UserDefaults.standard.data(forKey: key),
              let snap = try? JSONDecoder().decode(WidgetSnapshot.self, from: data) else { return .empty }
        return snap
    }

    func cache() {
        guard let data = try? JSONEncoder().encode(self) else { return }
        UserDefaults.standard.set(data, forKey: Self.key)
    }

    /// Busca agora no relay; sem rede, devolve a última cópia.
    static func fetch() async -> WidgetSnapshot {
        guard let token = Keychain.load() else { return .empty }
        do {
            let snap = WidgetSnapshot(state: try await RelayClient(token: token).state(), paired: true)
            snap.cache()
            return snap
        } catch RelayError.unauthorized {
            return .empty
        } catch {
            return cached()
        }
    }
}
