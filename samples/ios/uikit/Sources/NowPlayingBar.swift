//
//  NowPlayingBar.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import UIKit

final class NowPlayingBar: UIView {

    let titleLabel = UILabel()
    let subtitleLabel = UILabel()
    let previousButton = UIButton(type: .system)
    let nextButton = UIButton(type: .system)
    let playPauseButton = UIButton(type: .system)

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = DesignTokens.backgroundPrimary

        titleLabel.font = .systemFont(ofSize: 15, weight: .semibold)
        titleLabel.textColor = DesignTokens.textPrimary
        titleLabel.lineBreakMode = .byTruncatingTail

        subtitleLabel.font = .systemFont(ofSize: 13)
        subtitleLabel.textColor = DesignTokens.textMuted

        let text = UIStackView(arrangedSubviews: [titleLabel, subtitleLabel])
        text.axis = .vertical
        text.spacing = 0
        text.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        configure(previousButton, systemName: "backward.end", label: "Previous preset", tint: DesignTokens.textPrimary)
        configure(nextButton, systemName: "forward.end", label: "Next preset", tint: DesignTokens.textPrimary)

        playPauseButton.accessibilityLabel = "Pause"
        playPauseButton.backgroundColor = DesignTokens.accentPrimary
        playPauseButton.tintColor = DesignTokens.textOnAccent
        playPauseButton.layer.cornerRadius = 22
        playPauseButton.translatesAutoresizingMaskIntoConstraints = false
        playPauseButton.widthAnchor.constraint(equalToConstant: 44).isActive = true
        playPauseButton.heightAnchor.constraint(equalToConstant: 44).isActive = true

        let stack = UIStackView(arrangedSubviews: [text, previousButton, playPauseButton, nextButton])
        stack.axis = .horizontal
        stack.spacing = 4
        stack.alignment = .center
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)

        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 16),
            stack.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -8),
            stack.topAnchor.constraint(equalTo: topAnchor, constant: 8),
            stack.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -8)
        ])

        let separator = UIView()
        separator.backgroundColor = DesignTokens.borderSubtle
        separator.translatesAutoresizingMaskIntoConstraints = false
        addSubview(separator)
        NSLayoutConstraint.activate([
            separator.leadingAnchor.constraint(equalTo: leadingAnchor),
            separator.trailingAnchor.constraint(equalTo: trailingAnchor),
            separator.bottomAnchor.constraint(equalTo: bottomAnchor),
            separator.heightAnchor.constraint(equalToConstant: DesignTokens.hairline)
        ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    func update(title: String, subtitle: String, isPaused: Bool) {
        titleLabel.text = title
        subtitleLabel.text = subtitle
        let symbol = isPaused ? "play.fill" : "pause.fill"
        playPauseButton.setImage(UIImage(systemName: symbol), for: .normal)
        playPauseButton.accessibilityLabel = isPaused ? "Play" : "Pause"
    }

    private func configure(_ button: UIButton, systemName: String, label: String, tint: UIColor) {
        button.setImage(UIImage(systemName: systemName), for: .normal)
        button.tintColor = tint
        button.accessibilityLabel = label
        button.translatesAutoresizingMaskIntoConstraints = false
        button.widthAnchor.constraint(equalToConstant: 44).isActive = true
        button.heightAnchor.constraint(equalToConstant: 44).isActive = true
    }
}
