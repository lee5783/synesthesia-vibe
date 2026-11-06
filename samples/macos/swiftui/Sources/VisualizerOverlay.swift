//
//  VisualizerOverlay.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SwiftUI
import SynesthesiaVibe

struct StatusPill: View {

    let isPaused: Bool
    let fpsText: String
    let sourceLabel: String

    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(isPaused ? Color.svTextMuted : Color.svSuccess)
                .frame(width: 6, height: 6)
            Text("\(isPaused ? "Paused" : "Playing") · \(fpsText) · \(sourceLabel)")
                .font(.system(size: 12))
                .foregroundStyle(Color.svTextPrimary)
        }
        .padding(.vertical, 4)
        .padding(.horizontal, 10)
        .background(Capsule().fill(Color.svOverlay))
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("status-pill")
    }
}

struct OverVideoButton: View {

    let systemImage: String
    let label: String
    let isOn: Bool
    let help: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .font(.system(size: 16, weight: .medium))
                .foregroundStyle(Color.svTextOnAccent)
                .frame(width: 32, height: 28)
                .background(RoundedRectangle(cornerRadius: 6, style: .continuous).fill(isOn ? Color.svAccentPrimary : Color.svOverlay))
                .overlay(RoundedRectangle(cornerRadius: 6, style: .continuous).stroke(Color.svHairline, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .help(help)
        .accessibilityLabel(label)
        .accessibilityAddTraits(isOn ? [.isSelected] : [])
    }
}
