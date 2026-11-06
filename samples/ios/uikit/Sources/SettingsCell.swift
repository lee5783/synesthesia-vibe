//
//  SettingsCell.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import UIKit

final class SettingsCell: UITableViewCell {

    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: .subtitle, reuseIdentifier: reuseIdentifier)
        backgroundColor = DesignTokens.surfaceCard
        tintColor = DesignTokens.accentText
        textLabel?.numberOfLines = 1
        detailTextLabel?.numberOfLines = 0
        detailTextLabel?.font = .systemFont(ofSize: 13)
        detailTextLabel?.textColor = DesignTokens.textMuted
        selectedBackgroundView = UIView()
        selectedBackgroundView?.backgroundColor = DesignTokens.surfacePressed
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    override func prepareForReuse() {
        super.prepareForReuse()
        textLabel?.text = nil
        textLabel?.attributedText = nil
        textLabel?.font = .systemFont(ofSize: 17)
        textLabel?.textColor = DesignTokens.textPrimary
        detailTextLabel?.text = nil
        detailTextLabel?.textColor = DesignTokens.textMuted
        accessoryView = nil
        accessoryType = .none
        selectionStyle = .default
        isUserInteractionEnabled = true
        alpha = 1
        accessibilityTraits = .button
    }
}
