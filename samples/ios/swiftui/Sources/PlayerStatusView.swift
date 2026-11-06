//
//  PlayerStatusView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI
import SynesthesiaVibe

struct PlayerStatusView: View {

    private static let debugIntervals: [Float] = [0.5, 1, 2]

    @ObservedObject var player: SynesthesiaPlayer
    let audioError: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if let info = player.debugInfo {
                Text(Self.debugSummary(of: info))
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundColor(.white)
                    .shadow(color: .black, radius: 1)
                    .background(Color.black.opacity(0.45))
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            Text([player.presetName, player.lastError?.localizedDescription, audioError].compactMap { $0 }.joined(separator: "\n"))
                .font(.caption)
                .foregroundColor(.white)
                .shadow(color: .black, radius: 1)
                .lineLimit(2)
                .frame(maxWidth: .infinity, alignment: .leading)
            HStack(spacing: 12) {
                Button(player.debugEnabled ? "Debug on" : "Debug off", action: toggleDebug)
                Button("Every \(player.debugInterval.formatted()) s", action: cycleDebugInterval)
            }
            .font(.caption)
            .foregroundColor(.white)
        }
    }

    private func toggleDebug() {
        player.debugEnabled.toggle()
    }

    private func cycleDebugInterval() {
        let intervals = Self.debugIntervals
        let next = (intervals.firstIndex(of: player.debugInterval) ?? -1) + 1
        player.debugInterval = intervals[next % intervals.count]
    }

    private static func debugSummary(of info: [String: Any]) -> String {
        func number(_ key: String) -> Double { (info[key] as? NSNumber)?.doubleValue ?? 0 }
        func integer(_ key: String) -> Int { (info[key] as? NSNumber)?.intValue ?? 0 }
        func text(_ key: String) -> String { info[key] as? String ?? "" }
        return [
            String(format: "time %.2f  frame %d", number("time"), integer("frame")),
            String(format: "fps %.1f  steps %.2f  rate %.2f", number("display_fps"), number("sim_steps_per_frame"), number("playback_rate")),
            String(format: "ver %d  warp %d  comp %d  limiter %d", integer("preset_version"), integer("warp_shader"), integer("comp_shader"), integer("limiter_active")),
            String(format: "bass %.2f  mid %.2f  treb %.2f", number("bass"), number("mid"), number("treb")),
            String(format: "att %.2f / %.2f / %.2f", number("bass_att"), number("mid_att"), number("treb_att")),
            String(format: "tex %dx%d  view %dx%d  fb %d  gl_err %d", integer("texsize_x"), integer("texsize_y"), integer("width"), integer("height"), integer("feedback_format"), integer("gl_errors")),
            "preset: \(text("preset_name"))",
            "error: \(text("last_error"))"
        ].joined(separator: "\n")
    }
}
