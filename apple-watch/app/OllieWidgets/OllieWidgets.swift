import SwiftUI
import WidgetKit

struct Entry: TimelineEntry {
    let date: Date
    let snapshot: WidgetSnapshot
}

struct Provider: TimelineProvider {
    func placeholder(in context: Context) -> Entry {
        Entry(date: .now, snapshot: WidgetSnapshot(fiveHourPct: 23, fiveHourResetsAt: nil, sevenDayPct: 41, working: 2, waiting: 1, paired: true))
    }

    func getSnapshot(in context: Context, completion: @escaping (Entry) -> Void) {
        completion(context.isPreview ? placeholder(in: context) : Entry(date: .now, snapshot: .cached()))
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<Entry>) -> Void) {
        // O app pede recarga sempre que o estado muda; fora isso, a cada 15 minutos.
        nonisolated(unsafe) let completion = completion
        Task {
            let snap = await WidgetSnapshot.fetch()
            completion(Timeline(entries: [Entry(date: .now, snapshot: snap)], policy: .after(.now.addingTimeInterval(15 * 60))))
        }
    }
}

struct UsageCircular: View {
    let snap: WidgetSnapshot

    var body: some View {
        if let pct = snap.fiveHourPct {
            Gauge(value: min(pct, 100), in: 0...100) {
                Text("5h")
            } currentValueLabel: {
                Text("\(Int(pct.rounded()))")
            }
            .gaugeStyle(.accessoryCircularCapacity)
            .tint(pct >= 90 ? .red : pct >= 70 ? .orange : .green)
        } else {
            ZStack {
                AccessoryWidgetBackground()
                Image(systemName: "sparkle").font(.title3)
            }
        }
    }
}

struct SessionsRectangular: View {
    let snap: WidgetSnapshot

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Label("Ollie", systemImage: "sparkle")
                .font(.headline)
                .widgetAccentable()
            if !snap.paired {
                Text("Open to pair").font(.caption)
            } else {
                Text(sessionsLine).font(.caption)
                if let pct = snap.fiveHourPct {
                    Gauge(value: min(pct, 100), in: 0...100) {
                        Text("5h")
                    } currentValueLabel: {
                        Text("\(Int(pct.rounded()))%")
                    }
                    .gaugeStyle(.accessoryLinearCapacity)
                }
            }
        }
    }

    private var sessionsLine: String {
        if snap.working == 0 && snap.waiting == 0 { return String(localized: "No active sessions") }
        var parts: [String] = []
        if snap.working > 0 { parts.append(String(localized: "\(snap.working) working")) }
        if snap.waiting > 0 { parts.append(String(localized: "\(snap.waiting) waiting")) }
        return parts.joined(separator: " · ")
    }
}

struct InlineView: View {
    let snap: WidgetSnapshot

    var body: some View {
        if let pct = snap.fiveHourPct {
            Text("5h \(Int(pct.rounded()))% · \(snap.working + snap.waiting) active")
        } else {
            Text("\(snap.working + snap.waiting) active")
        }
    }
}

struct OllieWidgetView: View {
    @Environment(\.widgetFamily) private var family
    let entry: Entry

    var body: some View {
        switch family {
        case .accessoryRectangular: SessionsRectangular(snap: entry.snapshot)
        case .accessoryInline: InlineView(snap: entry.snapshot)
        case .accessoryCorner:
            Image(systemName: "sparkle")
                .widgetLabel { Text("\(Int((entry.snapshot.fiveHourPct ?? 0).rounded()))%") }
        default: UsageCircular(snap: entry.snapshot)
        }
    }
}

@main
struct OllieWidget: Widget {
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: "OllieWidget", provider: Provider()) { entry in
            OllieWidgetView(entry: entry)
                .containerBackground(.fill.tertiary, for: .widget)
        }
        .configurationDisplayName("Ollie")
        .description("Plan usage and your coding sessions.")
        .supportedFamilies([.accessoryCircular, .accessoryRectangular, .accessoryInline, .accessoryCorner])
    }
}
