//
//  PresetListViewController.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import Combine
import UIKit

final class PresetListViewController: UIViewController {

    private enum Section: Int, CaseIterable {
        case folder
        case presets
    }

    private let session: VisualizerSession
    private let tableView = UITableView(frame: .zero, style: .insetGrouped)
    private let miniPlayer = MiniPlayerView()
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
        title = "Playlist"
        navigationItem.largeTitleDisplayMode = .always
        navigationController?.navigationBar.prefersLargeTitles = true
        view.backgroundColor = DesignTokens.backgroundSecondary
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "folder.badge.plus"),
            style: .plain,
            target: self,
            action: #selector(chooseFolder)
        )
        navigationItem.rightBarButtonItem?.accessibilityLabel = "Choose presets folder"

        tableView.dataSource = self
        tableView.delegate = self
        tableView.register(SettingsCell.self, forCellReuseIdentifier: "PresetCell")
        tableView.backgroundColor = DesignTokens.backgroundSecondary
        tableView.tintColor = DesignTokens.accentText
        tableView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(tableView)

        miniPlayer.translatesAutoresizingMaskIntoConstraints = false
        miniPlayer.onOpen = { [weak self] in self?.navigationController?.popViewController(animated: true) }
        miniPlayer.onNext = { [weak self] in self?.session.advance(by: 1) }
        miniPlayer.onPlayPause = { [weak self] in self?.session.togglePause() }
        view.addSubview(miniPlayer)

        NSLayoutConstraint.activate([
            tableView.topAnchor.constraint(equalTo: view.topAnchor),
            tableView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            tableView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            tableView.bottomAnchor.constraint(equalTo: miniPlayer.topAnchor),
            miniPlayer.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            miniPlayer.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            miniPlayer.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])

        session.objectWillChange
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                DispatchQueue.main.async { self?.reload() }
            }
            .store(in: &cancellables)
        reload()
    }

    private func reload() {
        tableView.reloadData()
        miniPlayer.update(
            title: session.currentPreset?.name ?? "--",
            subtitle: "Now playing · \(session.positionText)",
            isPaused: session.player.isPaused
        )
    }

    @objc private func chooseFolder() {
        let picker = FolderPicker.make(delegate: self)
        present(picker, animated: true)
    }
}

extension PresetListViewController: UITableViewDataSource, UITableViewDelegate {

    func numberOfSections(in tableView: UITableView) -> Int {
        Section.allCases.count
    }

    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section) {
        case .folder: return 1
        case .presets: return session.presetCount
        case nil: return 0
        }
    }

    func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section) {
        case .folder: return "Folder"
        case .presets: return "\(session.presetCount) presets"
        case nil: return nil
        }
    }

    func tableView(_ tableView: UITableView, titleForFooterInSection section: Int) -> String? {
        Section(rawValue: section) == .presets ? "Tap a preset to play it." : nil
    }

    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "PresetCell", for: indexPath)
        switch Section(rawValue: indexPath.section) {
        case .folder:
            cell.textLabel?.text = session.folderName
            cell.detailTextLabel?.text = "Change"
            cell.detailTextLabel?.textColor = DesignTokens.accentText
            cell.accessoryType = .disclosureIndicator
        case .presets:
            let isCurrent = indexPath.row == session.currentIndex
            cell.textLabel?.text = session.presets[indexPath.row].name
            cell.textLabel?.font = .systemFont(ofSize: 17, weight: isCurrent ? .semibold : .regular)
            cell.textLabel?.textColor = isCurrent ? DesignTokens.accentText : DesignTokens.textPrimary
            cell.detailTextLabel?.text = "\(indexPath.row + 1)"
            cell.detailTextLabel?.textColor = isCurrent ? DesignTokens.accentText : DesignTokens.textMuted
            cell.accessoryView = isCurrent ? UIImageView(image: UIImage(systemName: "waveform")) : nil
            cell.accessoryView?.tintColor = DesignTokens.accentText
            cell.accessibilityTraits = isCurrent ? [.button, .selected] : .button
        case nil:
            break
        }
        return cell
    }

    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch Section(rawValue: indexPath.section) {
        case .folder: chooseFolder()
        case .presets: session.select(indexPath.row)
        case nil: break
        }
    }
}

extension PresetListViewController: FolderPickerDelegate {

    func folderPicker(_ controller: UIDocumentPickerViewController, didPickFolder url: URL) {
        session.chooseFolder(url)
    }
}

final class MiniPlayerView: UIView {

    var onOpen: (() -> Void)?
    var onNext: (() -> Void)?
    var onPlayPause: (() -> Void)?

    private let thumbnail = UIView()
    private let titleLabel = UILabel()
    private let subtitleLabel = UILabel()
    private let playPauseButton = UIButton(type: .system)
    private let nextButton = UIButton(type: .system)

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = DesignTokens.backgroundPrimary

        thumbnail.backgroundColor = DesignTokens.accentSubtle
        thumbnail.layer.cornerRadius = 8
        thumbnail.translatesAutoresizingMaskIntoConstraints = false
        let waveform = UIImageView(image: UIImage(systemName: "waveform"))
        waveform.tintColor = DesignTokens.accentText
        waveform.translatesAutoresizingMaskIntoConstraints = false
        thumbnail.addSubview(waveform)

        titleLabel.font = .systemFont(ofSize: 15, weight: .semibold)
        titleLabel.textColor = DesignTokens.textPrimary
        subtitleLabel.font = .systemFont(ofSize: 13)
        subtitleLabel.textColor = DesignTokens.textMuted

        let text = UIStackView(arrangedSubviews: [titleLabel, subtitleLabel])
        text.axis = .vertical
        text.spacing = 0

        let openButton = UIButton(type: .custom)
        openButton.accessibilityLabel = "Open visualizer"
        openButton.addTarget(self, action: #selector(open), for: .touchUpInside)
        openButton.translatesAutoresizingMaskIntoConstraints = false

        playPauseButton.accessibilityLabel = "Pause"
        playPauseButton.tintColor = DesignTokens.textOnAccent
        playPauseButton.backgroundColor = DesignTokens.accentPrimary
        playPauseButton.layer.cornerRadius = 22
        playPauseButton.addTarget(self, action: #selector(playPause), for: .touchUpInside)
        playPauseButton.translatesAutoresizingMaskIntoConstraints = false

        nextButton.setImage(UIImage(systemName: "forward.end"), for: .normal)
        nextButton.tintColor = DesignTokens.textPrimary
        nextButton.accessibilityLabel = "Next preset"
        nextButton.addTarget(self, action: #selector(nextPreset), for: .touchUpInside)
        nextButton.translatesAutoresizingMaskIntoConstraints = false

        let row = UIStackView(arrangedSubviews: [thumbnail, text, UIView(), playPauseButton, nextButton])
        row.axis = .horizontal
        row.spacing = 12
        row.alignment = .center
        row.translatesAutoresizingMaskIntoConstraints = false
        addSubview(row)
        addSubview(openButton)

        NSLayoutConstraint.activate([
            row.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 16),
            row.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -16),
            row.topAnchor.constraint(equalTo: topAnchor, constant: 8),
            row.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -34),
            thumbnail.widthAnchor.constraint(equalToConstant: 44),
            thumbnail.heightAnchor.constraint(equalToConstant: 44),
            waveform.centerXAnchor.constraint(equalTo: thumbnail.centerXAnchor),
            waveform.centerYAnchor.constraint(equalTo: thumbnail.centerYAnchor),
            waveform.widthAnchor.constraint(equalToConstant: 24),
            waveform.heightAnchor.constraint(equalToConstant: 24),
            playPauseButton.widthAnchor.constraint(equalToConstant: 44),
            playPauseButton.heightAnchor.constraint(equalToConstant: 44),
            nextButton.widthAnchor.constraint(equalToConstant: 44),
            nextButton.heightAnchor.constraint(equalToConstant: 44),
            openButton.leadingAnchor.constraint(equalTo: row.leadingAnchor),
            openButton.topAnchor.constraint(equalTo: row.topAnchor),
            openButton.bottomAnchor.constraint(equalTo: row.bottomAnchor),
            openButton.trailingAnchor.constraint(equalTo: text.trailingAnchor)
        ])

        let separator = UIView()
        separator.backgroundColor = DesignTokens.borderSubtle
        separator.translatesAutoresizingMaskIntoConstraints = false
        addSubview(separator)
        NSLayoutConstraint.activate([
            separator.leadingAnchor.constraint(equalTo: leadingAnchor),
            separator.trailingAnchor.constraint(equalTo: trailingAnchor),
            separator.topAnchor.constraint(equalTo: topAnchor),
            separator.heightAnchor.constraint(equalToConstant: DesignTokens.hairline)
        ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    func update(title: String, subtitle: String, isPaused: Bool) {
        titleLabel.text = title
        subtitleLabel.text = subtitle
        playPauseButton.setImage(UIImage(systemName: isPaused ? "play.fill" : "pause.fill"), for: .normal)
        playPauseButton.accessibilityLabel = isPaused ? "Play" : "Pause"
    }

    @objc private func open() { onOpen?() }
    @objc private func nextPreset() { onNext?() }
    @objc private func playPause() { onPlayPause?() }
}
