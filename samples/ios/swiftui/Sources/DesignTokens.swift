//
//  DesignTokens.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SwiftUI

enum DesignTokens {

    static let backgroundPrimary = Color(hex: 0x1e1e1e)
    static let backgroundSecondary = Color(hex: 0x161616)
    static let surfaceCard = Color(hex: 0x262626)
    static let surfaceElevated = Color(hex: 0x2c2c2c)
    static let surfacePressed = Color(hex: 0x383838)
    static let borderSubtle = Color(hex: 0x333333)
    static let textPrimary = Color(hex: 0xdcddde)
    static let textSecondary = Color(hex: 0xb4b4b6)
    static let textMuted = Color(hex: 0x949494)
    static let textDisabled = Color(hex: 0x5c5c5c)
    static let textOnAccent = Color.white
    static let accentPrimary = Color(hex: 0x7c3aed)
    static let accentText = Color(hex: 0xa78bfa)
    static let accentSubtle = Color(hex: 0x2a2140)
    static let overlay = Color(red: 0, green: 0, blue: 0, opacity: 0.64)
    static let overlayBorder = Color(white: 1, opacity: 0.12)
    static let success = Color(hex: 0x4ade80)
    static let warning = Color(hex: 0xfbbf24)
    static let info = Color(hex: 0x60a5fa)
    static let danger = Color(hex: 0xf87171)
}

extension Color {

    init(hex: UInt32) {
        self.init(
            red: Double((hex >> 16) & 0xff) / 255,
            green: Double((hex >> 8) & 0xff) / 255,
            blue: Double(hex & 0xff) / 255
        )
    }
}
