//
//  SyncStore.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

import Combine
import Foundation
import ImageIO
import SynesthesiaVibe
import UniformTypeIdentifiers

final class SyncStore: ObservableObject {

    static let scalingChoices: [SVSyncScaling] = [.fill, .fit, .stretch]
    static let snapshotAttempts = 8
    static let defaultSnapshotDelay: TimeInterval = 6

    let mirrorPlayer: SynesthesiaPlayer

    @Published var mirrorEnabled = false {
        didSet {
            guard mirrorEnabled != oldValue else { return }
            mirrorEnabledChanged()
        }
    }
    @Published var syncEnabled = false {
        didSet {
            guard syncEnabled != oldValue else { return }
            syncEnabledChanged()
        }
    }
    @Published var scaling: SVSyncScaling = .fill {
        didSet {
            player.syncScaling = scaling
            NSLog("%@", "iOSSwiftUISample: sync scaling=\(Self.name(for: scaling))")
        }
    }
    @Published private(set) var errorText: String?
    @Published private(set) var activeGroup: SVSyncGroup?

    private let player: SynesthesiaPlayer
    private let library: PresetLibrary
    private weak var visualizer: VisualizerStore?
    private var group: SVSyncGroup?
    private var presetURLAtSyncOn: URL?
    private var presetURLAtSyncOff: URL?
    private var launchDefaultsApplied = false
    private var cancellables: Set<AnyCancellable> = []

    init(player: SynesthesiaPlayer, library: PresetLibrary) {
        self.player = player
        self.library = library
        mirrorPlayer = SynesthesiaPlayer(configuration: player.configuration)
        player.$playbackRate
            .sink { [weak self] rate in self?.activeGroup?.playbackRate = rate }
            .store(in: &cancellables)
        player.$transitionConfiguration
            .sink { [weak self] configuration in self?.activeGroup?.transitionConfiguration = configuration }
            .store(in: &cancellables)
        player.$blendPattern
            .sink { [weak self] pattern in self?.activeGroup?.blendPattern = pattern }
            .store(in: &cancellables)
    }

    func bind(to visualizer: VisualizerStore) {
        self.visualizer = visualizer
    }

    static func name(for scaling: SVSyncScaling) -> String {
        switch scaling {
        case .fit: "Fit"
        case .stretch: "Stretch"
        default: "Fill"
        }
    }

    static func scaling(named name: String) -> SVSyncScaling? {
        switch name.lowercased() {
        case "fill": .fill
        case "fit": .fit
        case "stretch": .stretch
        default: nil
        }
    }

    func applyLaunchDefaults(_ defaults: UserDefaults = .standard) {
        guard !launchDefaultsApplied else { return }
        launchDefaultsApplied = true
        if let name = defaults.string(forKey: "SyncScaling"), let value = Self.scaling(named: name) {
            scaling = value
        }
        if defaults.bool(forKey: "SyncMirror") || defaults.bool(forKey: "SyncOn") {
            mirrorEnabled = true
        }
        if defaults.bool(forKey: "SyncOn") {
            syncEnabled = true
        }
        for delay in Self.delays(in: defaults.string(forKey: "SyncToggleAfter")) {
            schedule(after: delay) { store in
                store.syncEnabled.toggle()
            }
        }
        schedule(after: defaults.double(forKey: "SyncPauseMainAfter")) { store in
            store.player.isPaused = true
            NSLog("%@", "iOSSwiftUISample: main view paused")
        }
        schedule(after: defaults.double(forKey: "SyncResumeMainAfter")) { store in
            store.player.isPaused = false
            NSLog("%@", "iOSSwiftUISample: main view resumed")
        }
        if let directory = defaults.string(forKey: "SyncSnapshotDirectory") {
            let delay = defaults.double(forKey: "SyncSnapshotDelay")
            schedule(after: delay > 0 ? delay : Self.defaultSnapshotDelay) { store in
                store.saveSnapshots(to: directory)
            }
        }
    }

    private static func delays(in list: String?) -> [TimeInterval] {
        (list ?? "").split(separator: ",").compactMap { TimeInterval($0.trimmingCharacters(in: .whitespaces)) }
    }

    func saveSnapshots(to directory: String) {
        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let url = directory.hasPrefix("/") ? URL(fileURLWithPath: directory, isDirectory: true) : documents.appendingPathComponent(directory, isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        captureSnapshots(to: url, attempt: 1)
    }

    private func schedule(after delay: TimeInterval, _ action: @escaping (SyncStore) -> Void) {
        guard delay > 0 else { return }
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
            guard let self else { return }
            action(self)
        }
    }

    private func mirrorEnabledChanged() {
        if mirrorEnabled {
            startMirrorAudio()
            loadCurrentPresetIntoMirror()
            NSLog("%@", "iOSSwiftUISample: mirror view on")
        } else {
            syncEnabled = false
            mirrorPlayer.stopAudio()
            NSLog("%@", "iOSSwiftUISample: mirror view off")
        }
    }

    private func syncEnabledChanged() {
        if syncEnabled {
            turnSyncOn()
        } else {
            turnSyncOff()
        }
    }

    private func turnSyncOn() {
        guard mirrorEnabled else {
            syncEnabled = false
            return
        }
        var created = false
        if group == nil {
            do {
                group = try SVSyncGroup(configuration: player.configuration)
                created = true
            } catch {
                errorText = error.localizedDescription
                NSLog("%@", "iOSSwiftUISample: sync group failed \(error.localizedDescription)")
                syncEnabled = false
                return
            }
        }
        guard let group else { return }
        errorText = nil
        group.playbackRate = player.playbackRate
        group.transitionConfiguration = player.transitionConfiguration
        group.blendPattern = player.blendPattern
        group.presetRequestHandler = player.presetRequestHandler
        mirrorPlayer.stopAudio()
        mirrorPlayer.syncScaling = scaling
        player.syncScaling = scaling
        player.syncGroup = group
        activeGroup = group
        let current = visualizer?.currentPreset?.url
        presetURLAtSyncOn = current
        NSLog("%@", "iOSSwiftUISample: sync on group=\(Unmanaged.passUnretained(group).toOpaque()) created=\(created)")
        if created || current != presetURLAtSyncOff {
            visualizer?.reloadCurrentPreset()
        }
    }

    private func turnSyncOff() {
        guard let group = activeGroup else { return }
        player.syncGroup = nil
        activeGroup = nil
        group.presetRequestHandler = nil
        let current = visualizer?.currentPreset?.url
        presetURLAtSyncOff = current
        NSLog("%@", "iOSSwiftUISample: sync off group=\(Unmanaged.passUnretained(group).toOpaque())")
        if current != presetURLAtSyncOn {
            visualizer?.reloadCurrentPreset()
        }
        if mirrorEnabled {
            startMirrorAudio()
            loadCurrentPresetIntoMirror()
        }
    }

    private func startMirrorAudio() {
        guard let url = library.defaultWavURL else { return }
        try? mirrorPlayer.playWav(at: url)
    }

    private func loadCurrentPresetIntoMirror() {
        guard let url = visualizer?.currentPreset?.url else { return }
        let player = mirrorPlayer
        Task { try? await player.loadPreset(at: url, blendSeconds: 0) }
    }

    private func captureSnapshots(to directory: URL, attempt: Int) {
        guard let mainView = player.view, let mirrorView = mirrorPlayer.view else {
            NSLog("%@", "iOSSwiftUISample: sync snapshots need the mirror view")
            return
        }
        var mainImage: CGImage?
        var mirrorImage: CGImage?
        var pending = 2
        let finish = { [weak self] in
            pending -= 1
            guard pending == 0 else { return }
            self?.finishSnapshots(main: mainImage, mirror: mirrorImage, directory: directory, attempt: attempt)
        }
        mainView.snapshot { image in
            mainImage = image
            finish()
        }
        mirrorView.snapshot { image in
            mirrorImage = image
            finish()
        }
    }

    private func finishSnapshots(main: CGImage?, mirror: CGImage?, directory: URL, attempt: Int) {
        let identical = Self.imagesIdentical(main, mirror)
        if !identical, attempt < Self.snapshotAttempts {
            DispatchQueue.main.async { [weak self] in
                self?.captureSnapshots(to: directory, attempt: attempt + 1)
            }
            return
        }
        let mainURL = directory.appendingPathComponent("main.png")
        let mirrorURL = directory.appendingPathComponent("mirror.png")
        let written = Self.writePNG(main, to: mainURL) && Self.writePNG(mirror, to: mirrorURL)
        NSLog("%@", "iOSSwiftUISample: sync snapshots attempt=\(attempt) identical=\(identical) synced=\(activeGroup != nil) scaling=\(Self.name(for: scaling)) main=\(main?.width ?? 0)x\(main?.height ?? 0) mirror=\(mirror?.width ?? 0)x\(mirror?.height ?? 0) written=\(written) dir=\(directory.path)")
    }

    private static func imagesIdentical(_ first: CGImage?, _ second: CGImage?) -> Bool {
        guard let first, let second, first.width == second.width, first.height == second.height,
              let firstData = first.dataProvider?.data, let secondData = second.dataProvider?.data else { return false }
        return firstData as Data == secondData as Data
    }

    private static func writePNG(_ image: CGImage?, to url: URL) -> Bool {
        guard let image,
              let destination = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil) else { return false }
        CGImageDestinationAddImage(destination, image, nil)
        return CGImageDestinationFinalize(destination)
    }
}
