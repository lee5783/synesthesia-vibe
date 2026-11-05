//
//  VisualizerViewController.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import Combine
import SynesthesiaVibe
import UIKit

final class VisualizerViewController: UIViewController {

    private enum AudioSource {
        case wav
        case microphone
    }

    private static let cellIdentifier = "PresetCell"
    private static let debugIntervals: [Float] = [0.5, 1, 2]

    private let library: PresetLibrary
    private let player: SynesthesiaPlayer
    private let visualizerView = SVView(frame: .zero)
    private let tableView = UITableView(frame: .zero, style: .plain)
    private let statusLabel = UILabel()
    private let debugLabel = UILabel()
    private var audioSource = AudioSource.wav
    private var cancellables: Set<AnyCancellable> = []

    init(library: PresetLibrary) {
        self.library = library
        let configuration = SVContextConfiguration()
        configuration.resourceURL = library.resourceURL
        configuration.maxFps = Self.launchMaxFps(arguments: ProcessInfo.processInfo.arguments)
        player = SynesthesiaPlayer(configuration: configuration)
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    private static func launchMaxFps(arguments: [String]) -> Float {
        guard let index = arguments.firstIndex(of: "--max-fps"), index + 1 < arguments.endIndex else { return 0 }
        return Float(arguments[index + 1]) ?? 0
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "SynesthesiaVibe"
        view.backgroundColor = .black

        player.attach(to: visualizerView)
        view.addSubview(visualizerView)

        statusLabel.textColor = .white
        statusLabel.font = .preferredFont(forTextStyle: .caption1)
        statusLabel.numberOfLines = 2
        statusLabel.shadowColor = .black
        view.addSubview(statusLabel)

        debugLabel.textColor = .white
        debugLabel.font = .monospacedSystemFont(ofSize: 10, weight: .regular)
        debugLabel.numberOfLines = 0
        debugLabel.shadowColor = .black
        debugLabel.backgroundColor = UIColor.black.withAlphaComponent(0.45)
        view.addSubview(debugLabel)

        tableView.dataSource = self
        tableView.delegate = self
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: Self.cellIdentifier)
        view.addSubview(tableView)

        updateBarButtons()
        observePlayer()
        startWav()
        selectPreset(at: 0)
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let bounds = view.bounds.inset(by: view.safeAreaInsets)
        let isLandscape = bounds.width > bounds.height
        let edge: CGRectEdge = isLandscape ? .minXEdge : .minYEdge
        let distance = isLandscape ? (bounds.width * 0.62).rounded() : (bounds.height * 0.5).rounded()
        let (visualizerFrame, listFrame) = bounds.divided(atDistance: distance, from: edge)
        visualizerView.frame = visualizerFrame
        tableView.frame = listFrame
        let statusHeight = statusLabel.font.lineHeight * 2
        statusLabel.frame = CGRect(x: visualizerFrame.minX + 8, y: visualizerFrame.maxY - statusHeight - 6, width: visualizerFrame.width - 16, height: statusHeight)
        let debugWidth = visualizerFrame.width - 16
        let debugHeight = debugLabel.sizeThatFits(CGSize(width: debugWidth, height: .greatestFiniteMagnitude)).height
        debugLabel.frame = CGRect(x: visualizerFrame.minX + 8, y: visualizerFrame.minY + 6, width: debugWidth, height: debugHeight)
    }

    private func observePlayer() {
        Publishers.CombineLatest(player.$presetName, player.$lastError)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] name, error in
                self?.statusLabel.text = [name, error?.localizedDescription].compactMap { $0 }.joined(separator: "\n")
            }
            .store(in: &cancellables)
        player.$debugInfo
            .receive(on: DispatchQueue.main)
            .sink { [weak self] info in
                self?.debugLabel.text = info.map(Self.debugSummary(of:))
                self?.view.setNeedsLayout()
            }
            .store(in: &cancellables)
    }

    private static func debugSummary(of info: [String: Any]) -> String {
        func number(_ key: String) -> Double { (info[key] as? NSNumber)?.doubleValue ?? 0 }
        func integer(_ key: String) -> Int { (info[key] as? NSNumber)?.intValue ?? 0 }
        func text(_ key: String) -> String { info[key] as? String ?? "" }
        return [
            String(format: "time %.2f  frame %d", number("time"), integer("frame")),
            String(format: "fps %.1f  steps %.2f  rate %.2f", number("display_fps"), number("sim_steps_per_frame"), number("playback_rate")),
            String(format: "ver %d  warp %d  comp %d  limiter %d", integer("preset_version"), integer("warp_shader"), integer("comp_shader"), integer("limiter_active")),
            String(format: "bass %.2f  mid %.2f  treb %.2f", number("bass"), number("mid"), number("treb")),
            String(format: "att %.2f / %.2f / %.2f", number("bass_att"), number("mid_att"), number("treb_att")),
            String(format: "tex %dx%d  view %dx%d  fb %d  gl_err %d", integer("texsize_x"), integer("texsize_y"), integer("width"), integer("height"), integer("feedback_format"), integer("gl_errors")),
            "preset: \(text("preset_name"))",
            "error: \(text("last_error"))"
        ].joined(separator: "\n")
    }

    private func updateBarButtons() {
        let sourceTitle = audioSource == .wav ? "Mic" : "WAV"
        let pauseTitle = player.isPaused ? "Play" : "Pause"
        navigationItem.leftBarButtonItem = UIBarButtonItem(title: sourceTitle, style: .plain, target: self, action: #selector(toggleAudioSource))
        let pauseItem = UIBarButtonItem(title: pauseTitle, style: .plain, target: self, action: #selector(togglePause))
        let debugItem = UIBarButtonItem(title: player.debugEnabled ? "Debug on" : "Debug off", style: .plain, target: self, action: #selector(toggleDebug))
        let intervalItem = UIBarButtonItem(title: String(format: "%g s", player.debugInterval), style: .plain, target: self, action: #selector(cycleDebugInterval))
        navigationItem.rightBarButtonItems = [pauseItem, debugItem, intervalItem]
    }

    private func startWav() {
        guard let wavURL = library.defaultWavURL else { return }
        do {
            try player.playWav(at: wavURL)
            audioSource = .wav
        } catch {
            statusLabel.text = error.localizedDescription
        }
    }

    private func startMicrophone() {
        do {
            try player.startMicrophone()
            audioSource = .microphone
        } catch {
            statusLabel.text = error.localizedDescription
            startWav()
        }
    }

    private func selectPreset(at row: Int) {
        guard library.presets.indices.contains(row) else { return }
        let preset = library.presets[row]
        tableView.selectRow(at: IndexPath(row: row, section: 0), animated: false, scrollPosition: .none)
        Task {
            try? await player.loadPreset(at: preset.url, blendSeconds: 0)
        }
    }

    @objc private func toggleAudioSource() {
        switch audioSource {
        case .wav: startMicrophone()
        case .microphone: startWav()
        }
        updateBarButtons()
    }

    @objc private func toggleDebug() {
        player.debugEnabled.toggle()
        updateBarButtons()
    }

    @objc private func cycleDebugInterval() {
        let intervals = Self.debugIntervals
        let next = (intervals.firstIndex(of: player.debugInterval) ?? -1) + 1
        player.debugInterval = intervals[next % intervals.count]
        updateBarButtons()
    }

    @objc private func togglePause() {
        player.isPaused.toggle()
        updateBarButtons()
    }
}

extension VisualizerViewController: UITableViewDataSource, UITableViewDelegate {

    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        library.presets.count
    }

    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: Self.cellIdentifier, for: indexPath)
        cell.textLabel?.text = library.presets[indexPath.row].name
        cell.textLabel?.numberOfLines = 1
        return cell
    }

    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        selectPreset(at: indexPath.row)
    }
}
