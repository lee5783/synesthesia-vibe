//
//  DebugOverlayCard.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SwiftUI
import SynesthesiaVibe

struct DebugOverlayCard: View {

    let info: [String: Any]?
    let fpsCap: Float
    let sourceLabel: String
    let sampleRate: Double
    let glVersion: String?
    let renderScale: CGFloat
    let autoText: String
    let transitionText: String
    let levels: SVAudioLevels

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline, spacing: 6) {
                Text(fpsText)
                    .font(.svMonoFPS)
                    .foregroundStyle(displayFPS >= 50 ? Color.svSuccess : Color.svWarning)
                Text("fps · cap \(capText)")
                    .foregroundStyle(Color.svTextMuted)
                Spacer(minLength: 0)
                Text("\(frameMillisecondsText) ms")
                    .foregroundStyle(Color.svTextMuted)
            }
            grid
            bars
            if !lastErrorText.isEmpty {
                Text(lastErrorText)
                    .foregroundStyle(Color.svDanger)
            }
        }
        .font(.svMonoSmall)
        .foregroundStyle(Color.svTextPrimary)
        .padding(.vertical, 10)
        .padding(.horizontal, 12)
        .frame(width: 220, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 8, style: .continuous).fill(Color.svDebugCard))
        .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).stroke(Color.svHairline, lineWidth: 1))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("debug-card")
        .accessibilityLabel("Debug overlay")
    }

    private var grid: some View {
        Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 1) {
            row("preset", presetText, truncating: true)
            row("size", sizeText, truncating: false)
            row("gl", glText, truncating: false)
            row("audio", audioText, truncating: true)
            row("rate", "1.0× · \(transitionText)", truncating: false)
            row("auto", autoText, truncating: false)
        }
    }

    private func row(_ label: String, _ value: String, truncating: Bool) -> some View {
        GridRow {
            Text(label)
                .foregroundStyle(Color.svTextMuted)
                .gridColumnAlignment(.leading)
            Text(value)
                .lineLimit(1)
                .truncationMode(.tail)
                .frame(maxWidth: truncating ? .infinity : nil, alignment: .leading)
        }
    }

    private var bars: some View {
        VStack(alignment: .leading, spacing: 3) {
            bar("bass", levels.bass, Color.svAccentText)
            bar("mid", levels.mid, Color.svInfo)
            bar("treb", levels.treble, Color.svSuccess)
        }
    }

    private func bar(_ label: String, _ value: Float, _ color: Color) -> some View {
        HStack(spacing: 8) {
            Text(label)
                .foregroundStyle(Color.svTextMuted)
                .frame(width: 34, alignment: .leading)
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.svHairline)
                    Capsule().fill(color).frame(width: geometry.size.width * CGFloat(normalized(value)))
                }
            }
            .frame(height: 6)
        }
    }

    private func normalized(_ value: Float) -> Float {
        min(max(value, 0) / 3, 1)
    }

    private var displayFPS: Double {
        (info?["display_fps"] as? NSNumber)?.doubleValue ?? 0
    }

    private var fpsText: String {
        String(format: "%.1f", displayFPS)
    }

    private var capText: String {
        fpsCap > 0 ? String(format: "%.0f", fpsCap) : "none"
    }

    private var frameMillisecondsText: String {
        displayFPS > 0 ? String(format: "%.1f", 1000 / displayFPS) : "—"
    }

    private var presetText: String {
        let name = (info?["preset_name"] as? String) ?? ""
        return name.isEmpty ? "—" : name
    }

    private var sizeText: String {
        let width = (info?["width"] as? NSNumber)?.intValue ?? 0
        let height = (info?["height"] as? NSNumber)?.intValue ?? 0
        guard width > 0, height > 0 else { return "—" }
        return "\(width)×\(height) @\(String(format: "%.1f", Double(renderScale)))"
    }

    private var glText: String {
        glVersion ?? "—"
    }

    private var audioText: String {
        "\(sourceLabel) · \(String(format: "%.1f", sampleRate / 1000)) kHz"
    }

    private var lastErrorText: String {
        ((info?["last_error"] as? String) ?? "")
    }
}
