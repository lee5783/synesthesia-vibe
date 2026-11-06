//
//  OverlayControls.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import UIKit

final class StatusPillView: UIView {

    private let dot = UIView()
    private let label = UILabel()

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = DesignTokens.overlay
        layer.cornerRadius = 12
        directionalLayoutMargins = .init(top: 4, leading: 10, bottom: 4, trailing: 10)

        dot.layer.cornerRadius = 3
        dot.translatesAutoresizingMaskIntoConstraints = false
        dot.widthAnchor.constraint(equalToConstant: 6).isActive = true
        dot.heightAnchor.constraint(equalToConstant: 6).isActive = true

        label.font = .systemFont(ofSize: 12)
        label.textColor = DesignTokens.textPrimary

        let stack = UIStackView(arrangedSubviews: [dot, label])
        stack.axis = .horizontal
        stack.spacing = 6
        stack.alignment = .center
        stack.isUserInteractionEnabled = false
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)

        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: layoutMarginsGuide.topAnchor),
            stack.bottomAnchor.constraint(equalTo: layoutMarginsGuide.bottomAnchor),
            stack.leadingAnchor.constraint(equalTo: layoutMarginsGuide.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: layoutMarginsGuide.trailingAnchor)
        ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    func update(text: String, isPaused: Bool) {
        label.text = text
        dot.backgroundColor = isPaused ? DesignTokens.textMuted : DesignTokens.success
    }
}

final class RoundIconButton: UIButton {

    private let circle = UIView()
    private let icon = UIImageView()
    private let diameter: CGFloat

    var isOn = false {
        didSet { updateAppearance() }
    }

    init(systemName: String, diameter: CGFloat = 32, accessibilityName: String) {
        self.diameter = diameter
        super.init(frame: .zero)
        accessibilityLabel = accessibilityName

        circle.isUserInteractionEnabled = false
        circle.layer.cornerRadius = diameter / 2
        circle.layer.borderWidth = DesignTokens.hairline
        circle.translatesAutoresizingMaskIntoConstraints = false
        addSubview(circle)

        icon.image = UIImage(systemName: systemName)
        icon.tintColor = DesignTokens.textOnAccent
        icon.contentMode = .scaleAspectFit
        icon.isUserInteractionEnabled = false
        icon.translatesAutoresizingMaskIntoConstraints = false
        circle.addSubview(icon)

        NSLayoutConstraint.activate([
            widthAnchor.constraint(equalToConstant: 44),
            heightAnchor.constraint(equalToConstant: 44),
            circle.centerXAnchor.constraint(equalTo: centerXAnchor),
            circle.centerYAnchor.constraint(equalTo: centerYAnchor),
            circle.widthAnchor.constraint(equalToConstant: diameter),
            circle.heightAnchor.constraint(equalToConstant: diameter),
            icon.centerXAnchor.constraint(equalTo: circle.centerXAnchor),
            icon.centerYAnchor.constraint(equalTo: circle.centerYAnchor),
            icon.widthAnchor.constraint(equalToConstant: 18),
            icon.heightAnchor.constraint(equalToConstant: 18)
        ])
        updateAppearance()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    func setImage(systemName: String, accessibilityName: String) {
        icon.image = UIImage(systemName: systemName)
        accessibilityLabel = accessibilityName
    }

    private func updateAppearance() {
        circle.backgroundColor = isOn ? DesignTokens.accentPrimary : DesignTokens.overlay
        circle.layer.borderColor = DesignTokens.overlayBorder.cgColor
        accessibilityTraits = isOn ? [.button, .selected] : [.button]
    }
}
