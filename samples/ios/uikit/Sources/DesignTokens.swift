//
//  DesignTokens.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import UIKit

extension UIColor {

    convenience init(token: UInt32) {
        self.init(
            red: CGFloat((token >> 16) & 0xff) / 255,
            green: CGFloat((token >> 8) & 0xff) / 255,
            blue: CGFloat(token & 0xff) / 255,
            alpha: CGFloat((token >> 24) & 0xff) / 255
        )
    }
}

enum DesignTokens {

    static let backgroundPrimary = UIColor(token: 0xff1e1e1e)
    static let backgroundSecondary = UIColor(token: 0xff161616)
    static let surfaceCard = UIColor(token: 0xff262626)
    static let surfaceElevated = UIColor(token: 0xff2c2c2c)
    static let surfaceHover = UIColor(token: 0xff313131)
    static let surfacePressed = UIColor(token: 0xff383838)
    static let borderSubtle = UIColor(token: 0xff333333)
    static let borderStrong = UIColor(token: 0xff737373)
    static let textPrimary = UIColor(token: 0xffdcddde)
    static let textSecondary = UIColor(token: 0xffb4b4b6)
    static let textMuted = UIColor(token: 0xff949494)
    static let textDisabled = UIColor(token: 0xff5c5c5c)
    static let textOnAccent = UIColor(token: 0xffffffff)
    static let accentPrimary = UIColor(token: 0xff7c3aed)
    static let accentText = UIColor(token: 0xffa78bfa)
    static let accentSubtle = UIColor(token: 0xff2a2140)
    static let overlay = UIColor(token: 0xa3000000)
    static let overlayBorder = UIColor(token: 0x1fffffff)
    static let debugCard = UIColor(token: 0xd9000000)
    static let success = UIColor(token: 0xff4ade80)
    static let warning = UIColor(token: 0xfffbbf24)
    static let info = UIColor(token: 0xff60a5fa)
    static let danger = UIColor(token: 0xfff87171)

    static let hairline: CGFloat = 1 / UIScreen.main.scale

    static func monoFont(ofSize size: CGFloat, weight: UIFont.Weight = .regular) -> UIFont {
        .monospacedSystemFont(ofSize: size, weight: weight)
    }
}
