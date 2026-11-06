//
//  SVColorTokens.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SwiftUI

extension Color {

    static let svBackgroundPrimary = Color(svHex: 0x1E1E1E)
    static let svBackgroundSecondary = Color(svHex: 0x161616)
    static let svSurfaceCard = Color(svHex: 0x262626)
    static let svSurfaceElevated = Color(svHex: 0x2C2C2C)
    static let svSurfaceHover = Color(svHex: 0x313131)
    static let svSurfacePressed = Color(svHex: 0x383838)
    static let svBorderSubtle = Color(svHex: 0x333333)
    static let svBorderStrong = Color(svHex: 0x737373)
    static let svTextPrimary = Color(svHex: 0xDCDDDE)
    static let svTextSecondary = Color(svHex: 0xB4B4B6)
    static let svTextMuted = Color(svHex: 0x949494)
    static let svTextDisabled = Color(svHex: 0x5C5C5C)
    static let svTextOnAccent = Color(svHex: 0xFFFFFF)
    static let svAccentPrimary = Color(svHex: 0x7C3AED)
    static let svAccentHover = Color(svHex: 0x6D28D9)
    static let svAccentPressed = Color(svHex: 0x5B21B6)
    static let svAccentText = Color(svHex: 0xA78BFA)
    static let svAccentSubtle = Color(svHex: 0x2A2140)
    static let svOverlay = Color(svHex: 0x000000, opacity: 0xA3)
    static let svSuccess = Color(svHex: 0x4ADE80)
    static let svWarning = Color(svHex: 0xFBBF24)
    static let svInfo = Color(svHex: 0x60A5FA)
    static let svInfoSubtle = Color(svHex: 0x16233A)
    static let svDanger = Color(svHex: 0xF87171)
    static let svDangerSubtle = Color(svHex: 0x2F1818)
    static let svLetterbox = Color(svHex: 0x000000)
    static let svDebugCard = Color(svHex: 0x000000, opacity: 0xD9)
    static let svHairline = Color(svHex: 0xFFFFFF, opacity: 0x1F)
}

extension Font {

    static let svMonoSmall = Font.system(size: 11, design: .monospaced)
    static let svMonoCaption = Font.system(size: 12, design: .monospaced)
    static let svMonoFPS = Font.system(size: 18, weight: .semibold, design: .monospaced)
}

private extension Color {

    init(svHex hex: UInt32, opacity: UInt32 = 0xFF) {
        self.init(.sRGB,
                  red: Double((hex >> 16) & 0xFF) / 255,
                  green: Double((hex >> 8) & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255,
                  opacity: Double(opacity) / 255)
    }
}
