//
//  DebugCard.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SwiftUI
import SynesthesiaVibe

struct DebugCard: View {

    @ObservedObject var store: VisualizerStore
    @ObservedObject private var player: SynesthesiaPlayer

    init(store: VisualizerStore) {
        self.store = store
        _player = ObservedObject(wrappedValue: store.player)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline, spacing: 6) {
                Text(fpsText)
                    .font(.system(size: 18, weight: .semibold, design: .monospaced))
                    .foregroundColor(fpsWarning ? DesignTokens.warning : DesignTokens.success)
                Text("fps · cap \(store.capLabel)")
                    .foregroundColor(DesignTokens.textMuted)
                Spacer()
                Text(msText).foregroundColor(DesignTokens.textMuted)
            }
            VStack(alignment: .leading, spacing: 1) {
                row("preset", presetText)
                row("size", sizeText)
                row("gl", store.glVersion.isEmpty ? "--" : store.glVersion)
                row("audio", store.audioLabel)
                row("rate", "1.0× · \(store.transitionLabel)")
                row("auto", store.autoText)
            }
            if let error = errorText {
                row("last_error", error, valueColor: DesignTokens.danger)
            }
            VStack(spacing: 3) {
                bar("bass", value: bass, color: DesignTokens.accentText)
                bar("mid", value: mid, color: DesignTokens.info)
                bar("treb", value: treble, color: DesignTokens.success)
            }
        }
        .font(.system(size: 11, design: .monospaced))
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .frame(width: 220, alignment: .leading)
        .background(Color.black.opacity(0.85))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.white.opacity(0.12), lineWidth: 0.5))
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .accessibilityElement(children: .combine)
    }

    private var info: [String: Any]? { player.debugInfo }

    private var fpsWarning: Bool { number("display_fps") < 50 && info != nil }

    private var fpsText: String { info == nil ? "--" : String(format: "%.1f", number("display_fps")) }

    private var msText: String {
        guard info != nil else { return "-- ms" }
        let fps = number("display_fps")
        return fps > 0 ? String(format: "%.1f ms", 1000 / fps) : "-- ms"
    }

    private var presetText: String { info == nil ? "--" : text("preset_name") }

    private var sizeText: String {
        guard info != nil else { return "--" }
        return String(format: "%d×%d @%.1f", integer("width"), integer("height"), Double(renderScale))
    }

    private var errorText: String? {
        guard let info else { return nil }
        let value = info["last_error"] as? String ?? ""
        return value.isEmpty ? nil : value
    }

    private var renderScale: CGFloat { store.renderScale }

    private var bass: CGFloat { barValue("bass") }
    private var mid: CGFloat { barValue("mid") }
    private var treble: CGFloat { barValue("treb") }

    private func barValue(_ key: String) -> CGFloat {
        guard info != nil else { return 0 }
        return min(max(CGFloat(number(key)) / 3, 0), 1)
    }

    private func number(_ key: String) -> Double {
        (info?[key] as? NSNumber)?.doubleValue ?? 0
    }

    private func integer(_ key: String) -> Int {
        (info?[key] as? NSNumber)?.intValue ?? 0
    }

    private func text(_ key: String) -> String {
        info?[key] as? String ?? ""
    }

    private func row(_ label: String, _ value: String, valueColor: Color = DesignTokens.textPrimary) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text(label)
                .foregroundColor(DesignTokens.textMuted)
                .frame(width: 52, alignment: .leading)
            Text(value)
                .foregroundColor(valueColor)
                .lineLimit(1)
                .truncationMode(.tail)
        }
    }

    private func bar(_ label: String, value: CGFloat, color: Color) -> some View {
        HStack(spacing: 8) {
            Text(label)
                .foregroundColor(DesignTokens.textMuted)
                .frame(width: 34, alignment: .leading)
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.white.opacity(0.12))
                    Capsule().fill(color).frame(width: geometry.size.width * value)
                }
            }
            .frame(height: 6)
        }
    }
}
