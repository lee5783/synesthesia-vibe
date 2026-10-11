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

    private let library: PresetLibrary
    private let player: SynesthesiaPlayer
    private let visualizerView = SVView(frame: .zero)
    private let tableView = UITableView(frame: .zero, style: .plain)
    private let statusLabel = UILabel()
    private var audioSource = AudioSource.wav
    private var cancellables: Set<AnyCancellable> = []

    init(library: PresetLibrary) {
        self.library = library
        let configuration = SVContextConfiguration()
        configuration.resourceURL = library.resourceURL
        player = SynesthesiaPlayer(configuration: configuration)
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
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
    }

    private func observePlayer() {
        Publishers.CombineLatest(player.$presetName, player.$lastError)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] name, error in
                self?.statusLabel.text = [name, error?.localizedDescription].compactMap { $0 }.joined(separator: "\n")
            }
            .store(in: &cancellables)
    }

    private func updateBarButtons() {
        let sourceTitle = audioSource == .wav ? "Mic" : "WAV"
        let pauseTitle = player.isPaused ? "Play" : "Pause"
        navigationItem.leftBarButtonItem = UIBarButtonItem(title: sourceTitle, style: .plain, target: self, action: #selector(toggleAudioSource))
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: pauseTitle, style: .plain, target: self, action: #selector(togglePause))
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
