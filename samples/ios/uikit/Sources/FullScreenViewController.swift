//
//  FullScreenViewController.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import Combine
import UIKit

final class FullScreenViewController: UIViewController {

    private let session: VisualizerSession
    private let stageView = StageView(frame: .zero)
    private var cancellables: Set<AnyCancellable> = []
    var onExit: (() -> Void)?

    init(session: VisualizerSession) {
        self.session = session
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    override var prefersStatusBarHidden: Bool { true }

    override var prefersHomeIndicatorAutoHidden: Bool { true }

    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .allButUpsideDown }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        stageView.overlayTopInset = 52
        stageView.overlayLeadingInset = 16
        stageView.showsStatusPill = false
        stageView.fullScreenButton.setImage(systemName: "arrow.down.right.and.arrow.up.left", accessibilityName: "Exit full screen")
        stageView.debugButton.addTarget(self, action: #selector(toggleDebug), for: .touchUpInside)
        stageView.fullScreenButton.addTarget(self, action: #selector(exitFullScreen), for: .touchUpInside)
        stageView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stageView)
        NSLayoutConstraint.activate([
            stageView.topAnchor.constraint(equalTo: view.topAnchor),
            stageView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            stageView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            stageView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
        session.player.$debugInfo
            .receive(on: DispatchQueue.main)
            .sink { [weak self] info in self?.updateCard(info) }
            .store(in: &cancellables)
        attachPlayer()
    }

    private func attachPlayer() {
        session.player.attach(to: stageView.visualizerView)
        session.reloadCurrentPreset()
        refresh()
    }

    private func refresh() {
        stageView.debugButton.isOn = session.debugEnabled
        stageView.debugCard.isHidden = !session.debugEnabled
        updateCard(session.player.debugInfo)
    }

    private func updateCard(_ info: [String: Any]?) {
        guard let info else {
            stageView.debugCard.reset(
                cap: session.capLabel,
                gl: session.glVersion,
                audio: session.audioLabel,
                auto: session.autoText
            )
            return
        }
        stageView.debugCard.update(
            info: info,
            cap: session.capLabel,
            gl: session.glVersion,
            audio: session.audioLabel,
            auto: session.autoText,
            renderScale: stageView.visualizerView.renderScale
        )
    }

    @objc private func toggleDebug() {
        session.debugEnabled.toggle()
        refresh()
    }

    @objc private func exitFullScreen() {
        dismiss(animated: true) { [onExit] in onExit?() }
    }
}
