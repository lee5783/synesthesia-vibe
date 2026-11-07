//
//  VisualizerViewController.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import Combine
import OpenGLES
import SynesthesiaVibe
import UIKit

final class VisualizerViewController: UIViewController {

    private enum Section: Int, CaseIterable {
        case presets
        case autoChange
        case soundSource
        case playback
        case developer
    }

    private enum PlaybackRow: Int, CaseIterable {
        case frameRate
        case transition
    }

    private let session: VisualizerSession
    private let stageView = StageView(frame: .zero)
    private let nowPlayingBar = NowPlayingBar()
    private let tableView = UITableView(frame: .zero, style: .insetGrouped)
    private let autoSwitch = UISwitch()
    private let debugSwitch = UISwitch()
    private let frameRateControl = UISegmentedControl(items: VisualizerSession.frameRates.map { $0 > 0 ? String(Int($0)) : "Max" })
    private var cancellables: Set<AnyCancellable> = []

    init(session: VisualizerSession) {
        self.session = session
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Visualizer"
        view.backgroundColor = DesignTokens.backgroundSecondary
        navigationItem.largeTitleDisplayMode = .never
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "music.note.list"),
            style: .plain,
            target: self,
            action: #selector(openPlaylist)
        )
        navigationItem.rightBarButtonItem?.accessibilityLabel = "Open playlist"

        stageView.debugButton.addTarget(self, action: #selector(toggleDebug), for: .touchUpInside)
        stageView.fullScreenButton.addTarget(self, action: #selector(enterFullScreen), for: .touchUpInside)
        nowPlayingBar.previousButton.addTarget(self, action: #selector(previousPreset), for: .touchUpInside)
        nowPlayingBar.nextButton.addTarget(self, action: #selector(nextPreset), for: .touchUpInside)
        nowPlayingBar.playPauseButton.addTarget(self, action: #selector(togglePause), for: .touchUpInside)

        configureTable()
        observeSession()
        session.start()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let width = view.bounds.width
        let header = tableView.tableHeaderView
        let headerHeight = width + 60
        if header?.frame.height != headerHeight || header?.frame.width != width {
            header?.frame = CGRect(x: 0, y: 0, width: width, height: headerHeight)
            tableView.tableHeaderView = header
            stageView.frame = CGRect(x: 0, y: 0, width: width, height: width)
            nowPlayingBar.frame = CGRect(x: 0, y: width, width: width, height: 60)
        }
    }

    private func configureTable() {
        let header = UIView(frame: CGRect(x: 0, y: 0, width: view.bounds.width, height: 500))
        header.backgroundColor = DesignTokens.backgroundSecondary
        header.addSubview(stageView)
        header.addSubview(nowPlayingBar)
        tableView.tableHeaderView = header

        tableView.dataSource = self
        tableView.delegate = self
        tableView.backgroundColor = DesignTokens.backgroundSecondary
        tableView.tintColor = DesignTokens.accentText
        tableView.register(SettingsCell.self, forCellReuseIdentifier: "SettingsCell")
        tableView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(tableView)
        NSLayoutConstraint.activate([
            tableView.topAnchor.constraint(equalTo: view.topAnchor),
            tableView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            tableView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            tableView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])

        autoSwitch.onTintColor = DesignTokens.accentPrimary
        autoSwitch.addTarget(self, action: #selector(toggleAutoChange), for: .valueChanged)
        autoSwitch.accessibilityLabel = "Auto-change presets"

        debugSwitch.onTintColor = DesignTokens.accentPrimary
        debugSwitch.addTarget(self, action: #selector(toggleDebug), for: .valueChanged)
        debugSwitch.accessibilityLabel = "Debug overlay"

        frameRateControl.selectedSegmentTintColor = DesignTokens.surfacePressed
        frameRateControl.backgroundColor = DesignTokens.backgroundSecondary
        frameRateControl.setTitleTextAttributes([.foregroundColor: DesignTokens.textSecondary], for: .normal)
        frameRateControl.setTitleTextAttributes([.foregroundColor: DesignTokens.textPrimary], for: .selected)
        frameRateControl.addTarget(self, action: #selector(changeFrameRate), for: .valueChanged)
    }

    private func observeSession() {
        session.objectWillChange
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in DispatchQueue.main.async { self?.refresh() } }
            .store(in: &cancellables)
        session.player.$debugInfo
            .receive(on: DispatchQueue.main)
            .sink { [weak self] info in
                guard let self else { return }
                guard let info else {
                    self.stageView.debugCard.reset(
                        cap: self.session.capLabel,
                        gl: self.session.glVersion,
                        audio: self.session.audioLabel,
                        auto: self.session.autoText
                    )
                    return
                }
                self.stageView.debugCard.update(
                    info: info,
                    cap: self.session.capLabel,
                    gl: self.session.glVersion,
                    audio: self.session.audioLabel,
                    auto: self.session.autoText,
                    renderScale: self.stageView.visualizerView.renderScale
                )
            }
            .store(in: &cancellables)
        session.player.$presetName
            .compactMap { $0 }
            .receive(on: DispatchQueue.main)
            .sink { [weak self] name in
                guard let player = self?.session.player else { return }
                let info = player.presetInfo
                NSLog("%@", "iOSUIKitSample: preset info \"\(name)\" version=\(info.presetVersion) warp_ps=\(info.warpPixelShaderVersion) comp_ps=\(info.compPixelShaderVersion) warp_shader=\(info.warpShaderActive) comp_shader=\(info.compShaderActive) fallback=\"\(player.presetFallbackReason)\"")
            }
            .store(in: &cancellables)
        session.player.$presetName
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                self?.readGLVersion()
                self?.refresh()
            }
            .store(in: &cancellables)
    }

    private func readGLVersion() {
        guard session.glVersion.isEmpty else { return }
        stageView.visualizerView.perform(contextCurrent:) {
            session.updateGLVersion(GLVersionReader.read())
        }
    }

    func attachPlayer() {
        session.player.attach(to: stageView.visualizerView)
        refresh()
    }

    private func refresh() {
        nowPlayingBar.update(
            title: session.currentPreset?.name ?? "--",
            subtitle: session.nowPlayingSubtitle,
            isPaused: session.player.isPaused
        )
        stageView.statusPill.update(text: session.pillText, isPaused: session.player.isPaused)
        stageView.debugButton.isOn = session.debugEnabled
        stageView.debugCard.isHidden = !session.debugEnabled
        autoSwitch.isOn = session.autoChangeEnabled
        debugSwitch.isOn = session.debugEnabled
        frameRateControl.selectedSegmentIndex = VisualizerSession.frameRates.firstIndex(of: session.fpsSetting) ?? 1
        tableView.reloadData()
    }

    @objc private func openPlaylist() {
        navigationController?.pushViewController(PresetListViewController(session: session), animated: true)
    }

    @objc private func previousPreset() { session.advance(by: -1) }
    @objc private func nextPreset() { session.advance(by: 1) }
    @objc private func togglePause() { session.togglePause() }
    @objc private func toggleAutoChange() { session.autoChangeEnabled = autoSwitch.isOn }
    @objc private func toggleDebug() { session.debugEnabled.toggle(); refresh() }

    @objc private func changeFrameRate() {
        let index = frameRateControl.selectedSegmentIndex
        guard VisualizerSession.frameRates.indices.contains(index) else { return }
        session.fpsSetting = VisualizerSession.frameRates[index]
    }

    @objc private func enterFullScreen() {
        let controller = FullScreenViewController(session: session)
        controller.onExit = { [weak self] in self?.returnFromFullScreen() }
        controller.modalPresentationStyle = .fullScreen
        present(controller, animated: true)
    }

    private func returnFromFullScreen() {
        session.player.attach(to: stageView.visualizerView)
        session.reloadCurrentPreset()
        refresh()
    }

    private func pushIntervalList() {
        let options = VisualizerSession.autoIntervals.map(VisualizerSession.intervalLabel)
        let index = VisualizerSession.autoIntervals.firstIndex(of: session.autoInterval) ?? 2
        let controller = SelectionViewController(title: "Every", options: options, selectedIndex: index) { [weak self] row in
            guard let self, VisualizerSession.autoIntervals.indices.contains(row) else { return }
            self.session.setAutoInterval(VisualizerSession.autoIntervals[row])
        }
        navigationController?.pushViewController(controller, animated: true)
    }

    private func pushTransitionList() {
        let controller = SelectionViewController(title: "Transition", options: [VisualizerSession.transitionName], selectedIndex: 0) { _ in }
        navigationController?.pushViewController(controller, animated: true)
    }
}

extension VisualizerViewController: UITableViewDataSource, UITableViewDelegate {

    func numberOfSections(in tableView: UITableView) -> Int {
        Section.allCases.count
    }

    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section) {
        case .presets, .autoChange, .developer: return 1
        case .soundSource: return VisualizerSession.AudioSource.allCases.count
        case .playback: return PlaybackRow.allCases.count
        case nil: return 0
        }
    }

    func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section) {
        case .presets: return "Presets"
        case .autoChange: return "Auto-change"
        case .soundSource: return "Sound source"
        case .playback: return "Playback"
        case .developer: return "Developer"
        case nil: return nil
        }
    }

    func tableView(_ tableView: UITableView, titleForFooterInSection section: Int) -> String? {
        switch Section(rawValue: section) {
        case .presets: return "Pick a preset or change the presets folder."
        case .autoChange: return "Moves to the next preset in the playlist."
        case .soundSource: return "iOS doesn’t let apps capture other apps’ audio."
        case .playback: return "Frame rate caps rendering; animation speed stays the same."
        case .developer: return "Shows FPS, frame time, audio levels and preset info over the visualization, in full screen too."
        case nil: return nil
        }
    }

    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "SettingsCell", for: indexPath)
        switch Section(rawValue: indexPath.section) {
        case .presets:
            cell.textLabel?.text = "Playlist"
            cell.detailTextLabel?.text = "\(session.folderName) · \(session.presetCount)"
            cell.accessoryType = .disclosureIndicator

        case .autoChange:
            if indexPath.row == 0 {
                cell.textLabel?.text = "Auto-change presets"
                cell.accessoryView = autoSwitch
                cell.selectionStyle = .none
            } else {
                cell.textLabel?.text = "Every"
                cell.detailTextLabel?.text = session.intervalLabel
                cell.textLabel?.textColor = session.autoChangeEnabled ? DesignTokens.textPrimary : DesignTokens.textDisabled
                cell.detailTextLabel?.textColor = session.autoChangeEnabled ? DesignTokens.accentText : DesignTokens.textDisabled
                cell.accessoryType = .disclosureIndicator
                cell.isUserInteractionEnabled = session.autoChangeEnabled
                cell.alpha = session.autoChangeEnabled ? 1 : 0.45
            }

        case .soundSource:
            let source = VisualizerSession.AudioSource.allCases[indexPath.row]
            cell.textLabel?.text = source.label
            cell.detailTextLabel?.text = session.note(for: source)
            cell.textLabel?.textColor = source == .system ? DesignTokens.textDisabled : DesignTokens.textPrimary
            cell.detailTextLabel?.textColor = source == .system ? DesignTokens.textDisabled : DesignTokens.textMuted
            cell.accessoryType = source == session.audioSource ? .checkmark : .none
            cell.isUserInteractionEnabled = source != .system
            cell.alpha = source == .system ? 0.45 : 1
            cell.accessibilityTraits = source == session.audioSource ? [.button, .selected] : .button

        case .playback:
            switch PlaybackRow(rawValue: indexPath.row) {
            case .frameRate:
                cell.textLabel?.text = "Frame rate"
                cell.selectionStyle = .none
                frameRateControl.translatesAutoresizingMaskIntoConstraints = false
                let container = UIView()
                container.addSubview(frameRateControl)
                NSLayoutConstraint.activate([
                    frameRateControl.leadingAnchor.constraint(equalTo: container.leadingAnchor),
                    frameRateControl.trailingAnchor.constraint(equalTo: container.trailingAnchor),
                    frameRateControl.topAnchor.constraint(equalTo: container.topAnchor, constant: 6),
                    frameRateControl.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -6),
                    container.widthAnchor.constraint(equalToConstant: 260)
                ])
                cell.accessoryView = container
            case .transition:
                cell.textLabel?.text = "Transition"
                cell.detailTextLabel?.text = nil
                cell.accessoryType = .disclosureIndicator
                let value = UILabel()
                value.text = VisualizerSession.transitionName
                value.textColor = DesignTokens.accentText
                value.font = .systemFont(ofSize: 17)
                value.sizeToFit()
                cell.accessoryView = value
            case nil:
                break
            }

        case .developer:
            cell.textLabel?.text = "Debug overlay"
            cell.accessoryView = debugSwitch
            cell.selectionStyle = .none

        case nil:
            break
        }
        return cell
    }

    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch Section(rawValue: indexPath.section) {
        case .presets:
            openPlaylist()
        case .autoChange where indexPath.row == 1:
            pushIntervalList()
        case .soundSource:
            session.select(source: VisualizerSession.AudioSource.allCases[indexPath.row])
        case .playback where indexPath.row == PlaybackRow.transition.rawValue:
            pushTransitionList()
        default:
            break
        }
    }
}
