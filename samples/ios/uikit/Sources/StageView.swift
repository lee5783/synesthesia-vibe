//
//  StageView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SynesthesiaVibe
import UIKit

final class StageView: UIView {

    let visualizerView = SVView(frame: .zero)
    let statusPill = StatusPillView()
    let debugButton = RoundIconButton(systemName: "ladybug", accessibilityName: "Debug overlay")
    let fullScreenButton = RoundIconButton(systemName: "arrow.up.left.and.arrow.down.right", accessibilityName: "Enter full screen")
    let debugCard = DebugCardView()

    var showsStatusPill = true {
        didSet { statusPill.isHidden = !showsStatusPill }
    }

    var overlayTopInset: CGFloat = 4
    var overlayLeadingInset: CGFloat = 12

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = DesignTokens.backgroundPrimary
        addSubview(visualizerView)
        addSubview(statusPill)
        addSubview(debugCard)
        addSubview(debugButton)
        addSubview(fullScreenButton)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        let width = bounds.width
        visualizerView.frame = bounds

        let cardHeight = debugCard.systemLayoutSizeFitting(
            CGSize(width: 220, height: UIView.layoutFittingCompressedSize.height),
            withHorizontalFittingPriority: .required,
            verticalFittingPriority: .fittingSizeLevel
        ).height
        let buttonTop = overlayTopInset
        statusPill.isHidden = !showsStatusPill
        if showsStatusPill {
            statusPill.frame = CGRect(x: 12, y: 12, width: statusPill.intrinsicContentSize.width, height: 24)
        }
        debugCard.frame = CGRect(x: overlayLeadingInset, y: statusPill.isHidden ? 104 : statusPill.frame.maxY + 8, width: 220, height: cardHeight)
        debugButton.frame = CGRect(x: width - 56 - 44, y: buttonTop, width: 44, height: 44)
        fullScreenButton.frame = CGRect(x: width - 12 - 44, y: buttonTop, width: 44, height: 44)
    }
}
