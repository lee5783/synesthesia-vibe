//
//  PlayerStatusView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI
import SynesthesiaVibe

struct PlayerStatusView: View {

    @ObservedObject var player: SynesthesiaPlayer
    let audioError: String?

    var body: some View {
        Text([player.presetName, player.lastError?.localizedDescription, audioError].compactMap { $0 }.joined(separator: "\n"))
            .font(.caption)
            .foregroundStyle(.white)
            .shadow(color: .black, radius: 1)
            .lineLimit(2)
            .frame(maxWidth: .infinity, alignment: .leading)
            .accessibilityIdentifier("player-status")
    }
}
