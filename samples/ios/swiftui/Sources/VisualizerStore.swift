//
//  VisualizerStore.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import Combine
import Foundation
import SynesthesiaVibe

final class VisualizerStore: ObservableObject {

    enum AudioSource: String, CaseIterable, Identifiable {
        case none
        case wav
        case microphone
        case system

        var id: String { rawValue }

        var label: String {
            switch self {
            case .none: return "None"
            case .wav: return "Bundled WAV"
            case .microphone: return "Microphone"
            case .system: return "System sound"
            }
        }

        var note: String? {
            switch self {
            case .microphone: return "Asks for microphone access"
            case .system: return "Not available on iOS"
            case .none, .wav: return nil
            }
        }
    }

    static let blendSeconds: Float = 2
    static let frameRates: [Float] = [30, 60, 120, 0]
    static let autoIntervals: [TimeInterval] = [10, 15, 30, 60]
    static let bundledFolderName = "mvp-presets"
    static let transitionName = "Blend"

    let player: SynesthesiaPlayer
    let library: PresetLibrary

    @Published private(set) var presets: [Preset]
    @Published private(set) var currentIndex = 0
    @Published private(set) var folderName: String
    @Published private(set) var autoRemaining: TimeInterval = 30
    @Published private(set) var sourceNotes: [AudioSource: String] = [:]
    @Published private(set) var glVersion = ""
    @Published private(set) var renderScale: CGFloat = 1
    @Published private(set) var audioSource: AudioSource = .wav
    @Published private(set) var loadError: String?

    @Published var autoChangeEnabled = false {
        didSet {
            resetAutoCountdown()
            updateTimer()
        }
    }

    @Published private(set) var autoInterval: TimeInterval = 30 {
        didSet { resetAutoCountdown() }
    }

    @Published var fpsSetting: Float

    @Published var debugEnabled = false {
        didSet { player.debugEnabled = debugEnabled }
    }

    @Published var isFullScreen = false {
        didSet {
            guard !isFullScreen, oldValue else { return }
            returnToInlineView()
        }
    }
    @Published var userPaused = false

    private weak var inlineView: SVView?
    private var securityScopedFolderURL: URL?
    private var autoTimer: Timer?
    private var cancellables: Set<AnyCancellable> = []
    private lazy var bundledWavSampleRate: Int = {
        guard let url = library.defaultWavURL, let feeder = try? SVWavFeeder(url: url) else { return 44_100 }
        return feeder.sampleRate
    }()

    init(library: PresetLibrary, player: SynesthesiaPlayer, folderName: String, fps: Float) {
        self.library = library
        self.player = player
        self.folderName = folderName
        self.presets = library.presets
        self.fpsSetting = fps
        player.preferredFramesPerSecond = fps
        player.$isPaused
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                self?.updateTimer()
                self?.objectWillChange.send()
            }
            .store(in: &cancellables)
        player.$lastError
            .receive(on: DispatchQueue.main)
            .sink { [weak self] error in
                guard let error, self?.loadError == nil else { return }
                self?.loadError = error.localizedDescription
            }
            .store(in: &cancellables)
        player.$presetName
            .dropFirst()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                self?.readGLVersion()
                self?.objectWillChange.send()
            }
            .store(in: &cancellables)
    }

    deinit {
        autoTimer?.invalidate()
        securityScopedFolderURL?.stopAccessingSecurityScopedResource()
    }

    var currentPreset: Preset? {
        presets.indices.contains(currentIndex) ? presets[currentIndex] : nil
    }

    var presetCount: Int { presets.count }

    var fpsLabel: String { fpsSetting > 0 ? "\(Int(fpsSetting)) fps" : "Uncapped" }

    var capLabel: String { fpsSetting > 0 ? String(Int(fpsSetting)) : "none" }

    var pillText: String {
        let state = player.isPaused ? "Paused · " : ""
        return "\(state)\(fpsLabel) · \(audioSource.label)"
    }

    var positionText: String { "\(currentIndex + 1) of \(presetCount)" }

    var nowPlayingSubtitle: String {
        guard autoChangeEnabled else { return "\(positionText) · Auto-change off" }
        return "\(positionText) · Next preset in \(Int(autoRemaining.rounded(.up))) s"
    }

    var autoText: String {
        guard autoChangeEnabled else { return "off" }
        return "every \(Int(autoInterval)) s · next \(Int(autoRemaining.rounded(.up))) s"
    }

    var intervalLabel: String { Self.intervalLabel(autoInterval) }

    var audioLabel: String {
        switch audioSource {
        case .none: return audioSource.label
        case .wav: return "\(audioSource.label) · \(Self.rateLabel(bundledWavSampleRate))"
        case .microphone, .system: return "\(audioSource.label) · \(Self.rateLabel(44_100))"
        }
    }

    func note(for source: AudioSource) -> String? {
        sourceNotes[source] ?? source.note
    }

    static func intervalLabel(_ interval: TimeInterval) -> String {
        interval >= 60 ? "\(Int(interval / 60)) minute" : "\(Int(interval)) seconds"
    }

    static func rateLabel(_ sampleRate: Int) -> String {
        String(format: "%.1f kHz", Double(sampleRate) / 1000)
    }

    func start() {
        applySource(.wav, revertingTo: nil)
        select(0, blend: 0)
    }

    func select(_ index: Int, blend: Float = VisualizerStore.blendSeconds) {
        guard presets.indices.contains(index) else { return }
        currentIndex = index
        resetAutoCountdown()
        load(presets[index], blend: blend)
    }

    func reloadCurrentPreset() {
        guard let preset = currentPreset else { return }
        load(preset, blend: 0)
    }

    func advance(by offset: Int) {
        guard !presets.isEmpty else { return }
        let count = presets.count
        select(((currentIndex + offset) % count + count) % count)
    }

    func select(source: AudioSource) {
        guard source != .system, source != audioSource else { return }
        applySource(source, revertingTo: audioSource)
    }

    func setAutoInterval(_ interval: TimeInterval) {
        autoInterval = interval
    }

    func chooseFolder(_ url: URL) {
        let started = url.startAccessingSecurityScopedResource()
        securityScopedFolderURL?.stopAccessingSecurityScopedResource()
        securityScopedFolderURL = started ? url : nil
        folderName = url.lastPathComponent
        let found = Self.presets(in: url)
        if !found.isEmpty { presets = found }
        currentIndex = 0
        resetAutoCountdown()
        if let first = presets.first { load(first) }
    }

    func togglePause() {
        userPaused.toggle()
        if userPaused {
            player.pause()
        } else {
            player.resume()
        }
    }

    func attachView(_ view: SVView) {
        inlineView = view
        player.attach(to: view)
        renderScale = view.renderScale
        readGLVersion()
    }

    private func readGLVersion() {
        guard glVersion.isEmpty, let view = inlineView else { return }
        let version = GLVersionReader.read(from: view)
        if !version.isEmpty { updateGLVersion(version) }
    }

    private func returnToInlineView() {
        guard let view = inlineView else { return }
        player.attach(to: view)
        reloadCurrentPreset()
    }

    func clearLoadError() {
        loadError = nil
    }

    func updateGLVersion(_ version: String) {
        guard glVersion != version else { return }
        glVersion = version
    }

    private func applySource(_ source: AudioSource, revertingTo previous: AudioSource?) {
        do {
            try start(source)
            audioSource = source
            sourceNotes[source] = nil
        } catch {
            if let previous {
                try? start(previous)
                sourceNotes[previous] = error.localizedDescription
            }
        }
    }

    private func start(_ source: AudioSource) throws {
        switch source {
        case .none: player.stopAudio()
        case .wav:
            guard let url = library.defaultWavURL else { throw CocoaError(.fileNoSuchFile) }
            try player.playWav(at: url)
        case .microphone: try player.startMicrophone()
        case .system: try player.startSystemAudio()
        }
    }

    private func load(_ preset: Preset, blend: Float = VisualizerStore.blendSeconds) {
        loadError = nil
        Task { try? await player.loadPreset(at: preset.url, blendSeconds: blend) }
    }

    private func resetAutoCountdown() {
        autoRemaining = autoInterval
    }

    private func updateTimer() {
        autoTimer?.invalidate()
        autoTimer = nil
        guard autoChangeEnabled, !player.isPaused else { return }
        let timer = Timer(timeInterval: 1, repeats: true) { [weak self] _ in self?.tick() }
        RunLoop.main.add(timer, forMode: .common)
        autoTimer = timer
    }

    private func tick() {
        autoRemaining -= 1
        guard autoRemaining <= 0 else { return }
        advance(by: 1)
    }

    private static func presets(in directory: URL) -> [Preset] {
        let files = (try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)) ?? []
        return files
            .filter { $0.pathExtension == "milk" }
            .sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
            .map { Preset(id: nil, url: $0) }
    }
}
