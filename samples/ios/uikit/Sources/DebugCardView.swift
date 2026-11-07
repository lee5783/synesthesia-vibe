//
//  DebugCardView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import UIKit

final class DebugCardView: UIView {

    private static let labelColumn: CGFloat = 52

    private let fpsLabel = UILabel()
    private let capLabel = UILabel()
    private let msLabel = UILabel()
    private let presetValue = UILabel()
    private let sizeValue = UILabel()
    private let glValue = UILabel()
    private let audioValue = UILabel()
    private let rateValue = UILabel()
    private let autoValue = UILabel()
    private let errorLabel = UILabel()
    private let errorValue = UILabel()
    private let bassBar = DebugBarView(label: "bass", color: DesignTokens.accentText)
    private let midBar = DebugBarView(label: "mid", color: DesignTokens.info)
    private let trebleBar = DebugBarView(label: "treb", color: DesignTokens.success)

    override init(frame: CGRect) {
        super.init(frame: frame)
        configure()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    func update(info: [String: Any], cap: String, gl: String, audio: String, auto: String, transition: String, renderScale: CGFloat) {
        let fps = Self.number(info, "display_fps")
        fpsLabel.text = String(format: "%.1f", fps)
        fpsLabel.textColor = fps < 50 ? DesignTokens.warning : DesignTokens.success
        capLabel.text = "fps · cap \(cap)"
        msLabel.text = fps > 0 ? String(format: "%.1f ms", 1000 / fps) : "-- ms"
        presetValue.text = Self.text(info, "preset_name")
        sizeValue.text = String(format: "%d×%d @%.1f", Self.integer(info, "width"), Self.integer(info, "height"), Double(renderScale))
        glValue.text = gl
        audioValue.text = audio
        rateValue.text = "1.0× · \(transition)"
        autoValue.text = auto
        bassBar.value = CGFloat(Self.number(info, "bass")) / 3
        midBar.value = CGFloat(Self.number(info, "mid")) / 3
        trebleBar.value = CGFloat(Self.number(info, "treb")) / 3
        let error = Self.text(info, "last_error")
        errorLabel.isHidden = error.isEmpty
        errorValue.isHidden = error.isEmpty
        errorValue.text = error
    }

    func reset(cap: String, gl: String, audio: String, auto: String, transition: String) {
        fpsLabel.text = "--"
        fpsLabel.textColor = DesignTokens.success
        capLabel.text = "fps · cap \(cap)"
        msLabel.text = "-- ms"
        presetValue.text = "--"
        sizeValue.text = "--"
        glValue.text = gl
        audioValue.text = audio
        rateValue.text = "1.0× · \(transition)"
        autoValue.text = auto
        bassBar.value = 0
        midBar.value = 0
        trebleBar.value = 0
        errorLabel.isHidden = true
        errorValue.isHidden = true
        errorValue.text = ""
    }

    private func configure() {
        backgroundColor = DesignTokens.debugCard
        layer.cornerRadius = 8
        layer.borderWidth = DesignTokens.hairline
        layer.borderColor = DesignTokens.overlayBorder.cgColor
        directionalLayoutMargins = .init(top: 10, leading: 12, bottom: 10, trailing: 12)

        for label in [fpsLabel, capLabel, msLabel, presetValue, sizeValue, glValue, audioValue, rateValue, autoValue, errorValue] {
            label.font = DesignTokens.monoFont(ofSize: 11)
            label.textColor = DesignTokens.textPrimary
            label.adjustsFontSizeToFitWidth = true
            label.minimumScaleFactor = 0.7
        }
        fpsLabel.font = DesignTokens.monoFont(ofSize: 18, weight: .semibold)
        fpsLabel.setContentHuggingPriority(.required, for: .horizontal)
        capLabel.textColor = DesignTokens.textMuted
        msLabel.textColor = DesignTokens.textMuted
        msLabel.setContentHuggingPriority(.required, for: .horizontal)
        errorLabel.text = "last_error"
        errorLabel.font = DesignTokens.monoFont(ofSize: 11)
        errorLabel.textColor = DesignTokens.textMuted
        errorValue.textColor = DesignTokens.danger

        let fpsRow = UIStackView(arrangedSubviews: [fpsLabel, capLabel, UIView(), msLabel])
        fpsRow.axis = .horizontal
        fpsRow.spacing = 6
        fpsRow.alignment = .firstBaseline

        let grid = UIStackView(arrangedSubviews: [
            Self.row(label: "preset", value: presetValue),
            Self.row(label: "size", value: sizeValue),
            Self.row(label: "gl", value: glValue),
            Self.row(label: "audio", value: audioValue),
            Self.row(label: "rate", value: rateValue),
            Self.row(label: "auto", value: autoValue)
        ])
        grid.axis = .vertical
        grid.spacing = 1

        let stack = UIStackView(arrangedSubviews: [fpsRow, grid, bassBar, midBar, trebleBar])
        stack.axis = .vertical
        stack.spacing = 6
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)

        let errorStack = UIStackView(arrangedSubviews: [errorLabel, errorValue])
        errorStack.axis = .vertical
        errorStack.spacing = 1
        errorStack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(errorStack)

        NSLayoutConstraint.activate([
            widthAnchor.constraint(equalToConstant: 220),
            stack.topAnchor.constraint(equalTo: layoutMarginsGuide.topAnchor),
            stack.leadingAnchor.constraint(equalTo: layoutMarginsGuide.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: layoutMarginsGuide.trailingAnchor),
            errorStack.topAnchor.constraint(equalTo: stack.bottomAnchor, constant: 6),
            errorStack.leadingAnchor.constraint(equalTo: layoutMarginsGuide.leadingAnchor),
            errorStack.trailingAnchor.constraint(equalTo: layoutMarginsGuide.trailingAnchor),
            errorStack.bottomAnchor.constraint(equalTo: layoutMarginsGuide.bottomAnchor)
        ])
    }

    private static func row(label: String, value: UILabel) -> UIView {
        let name = UILabel()
        name.text = label
        name.font = DesignTokens.monoFont(ofSize: 11)
        name.textColor = DesignTokens.textMuted
        name.setContentHuggingPriority(.required, for: .horizontal)
        let row = UIStackView(arrangedSubviews: [name, value])
        row.axis = .horizontal
        row.spacing = 8
        row.alignment = .firstBaseline
        name.widthAnchor.constraint(equalToConstant: labelColumn).isActive = true
        return row
    }

    private static func number(_ info: [String: Any], _ key: String) -> Double {
        (info[key] as? NSNumber)?.doubleValue ?? 0
    }

    private static func integer(_ info: [String: Any], _ key: String) -> Int {
        (info[key] as? NSNumber)?.intValue ?? 0
    }

    private static func text(_ info: [String: Any], _ key: String) -> String {
        info[key] as? String ?? ""
    }
}

private final class DebugBarView: UIView {

    private let track = UIView()
    private let fill = UIView()
    private var fillWidth: NSLayoutConstraint?

    var value: CGFloat = 0 {
        didSet { updateFill() }
    }

    init(label: String, color: UIColor) {
        super.init(frame: .zero)
        let name = UILabel()
        name.text = label
        name.font = DesignTokens.monoFont(ofSize: 11)
        name.textColor = DesignTokens.textMuted
        name.translatesAutoresizingMaskIntoConstraints = false
        name.widthAnchor.constraint(equalToConstant: 34).isActive = true

        track.backgroundColor = DesignTokens.overlayBorder
        track.layer.cornerRadius = 3
        track.clipsToBounds = true
        track.translatesAutoresizingMaskIntoConstraints = false

        fill.backgroundColor = color
        fill.translatesAutoresizingMaskIntoConstraints = false
        track.addSubview(fill)

        addSubview(name)
        addSubview(track)

        NSLayoutConstraint.activate([
            name.leadingAnchor.constraint(equalTo: leadingAnchor),
            name.centerYAnchor.constraint(equalTo: centerYAnchor),
            track.leadingAnchor.constraint(equalTo: name.trailingAnchor, constant: 8),
            track.trailingAnchor.constraint(equalTo: trailingAnchor),
            track.topAnchor.constraint(equalTo: topAnchor),
            track.bottomAnchor.constraint(equalTo: bottomAnchor),
            heightAnchor.constraint(equalToConstant: 6),
            fill.leadingAnchor.constraint(equalTo: track.leadingAnchor),
            fill.topAnchor.constraint(equalTo: track.topAnchor),
            fill.bottomAnchor.constraint(equalTo: track.bottomAnchor)
        ])
        updateFill()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    private func updateFill() {
        let clamped = min(max(value, 0), 1)
        fillWidth?.isActive = false
        let constraint = fill.widthAnchor.constraint(equalTo: track.widthAnchor, multiplier: clamped)
        constraint.isActive = true
        fillWidth = constraint
    }
}
