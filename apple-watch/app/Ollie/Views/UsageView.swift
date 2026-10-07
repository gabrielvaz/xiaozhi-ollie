import SwiftUI

struct UsageSummary: View {
    let usage: Usage

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Plan usage").font(.caption2).foregroundStyle(.secondary)
            if let w = usage.fiveHour { UsageBar(title: "5h", window: w) }
            if let w = usage.sevenDay { UsageBar(title: "7d", window: w) }
        }
    }
}

struct UsageBar: View {
    let title: String
    let window: UsageWindow

    var body: some View {
        HStack(spacing: 6) {
            Text(verbatim: title).font(.caption2.monospacedDigit()).frame(width: 24, alignment: .leading)
            Gauge(value: min(window.pct, 100), in: 0...100) { EmptyView() }
                .gaugeStyle(.accessoryLinearCapacity)
                .tint(Color.usage(window.pct))
            Text(Format.percent(window.pct)).font(.caption2.monospacedDigit())
        }
    }
}

struct UsageView: View {
    @Environment(AppStore.self) private var store

    var body: some View {
        ScrollView {
            if let usage = store.state?.usage {
                VStack(spacing: 12) {
                    HStack(spacing: 14) {
                        if let w = usage.fiveHour { ring("5h", w) }
                        if let w = usage.sevenDay { ring("7d", w) }
                    }
                    if let reset = usage.fiveHour?.resetDate {
                        resetLine("5-hour window resets", reset)
                    }
                    if let reset = usage.sevenDay?.resetDate {
                        resetLine("Weekly limit resets", reset)
                    }
                    Text("Updated \(Format.relative(usage.updated))")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
            } else {
                Text("Usage appears after the first answer in a session, on Pro and Max plans, with plan usage turned on in the plugin.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
        .navigationTitle("Usage")
    }

    private func ring(_ title: String, _ w: UsageWindow) -> some View {
        VStack(spacing: 4) {
            Gauge(value: min(w.pct, 100), in: 0...100) {
                Text(verbatim: title)
            } currentValueLabel: {
                Text(Format.percent(w.pct))
            }
            .gaugeStyle(.accessoryCircularCapacity)
            .tint(Color.usage(w.pct))
            .scaleEffect(1.15)
            .padding(6)
            Text(verbatim: title).font(.caption2).foregroundStyle(.secondary)
        }
    }

    private func resetLine(_ title: LocalizedStringKey, _ date: Date) -> some View {
        VStack(spacing: 1) {
            Text(title).font(.caption2).foregroundStyle(.secondary)
            Text(date, style: .relative).font(.footnote.monospacedDigit())
        }
    }
}
