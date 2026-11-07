//
//  VisualizerSession.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import Combine
import Foundation
import SynesthesiaVibe

final class VisualizerSession: ObservableObject {

    enum AudioSource: String, CaseIterable {
        case none
        case wav
        case microphone
        case system

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

    static let blendDurations: [Float] = [0, 1, 2, 2.7, 4, 6, 8, 10]
    static let defaultBlendDuration: Float = 2
    static let frameRates: [Float] = [30, 60, 120, 0]
    static let autoIntervals: [TimeInterval] = [10, 15, 30, 60]
    static let bundledFolderName = "mvp-presets"
    static let blendPatternChoices: [Int] = [SVBlendPattern.random.rawValue] + Array(0..<SVBlendPatternCount)

    let player: SynesthesiaPlayer
    let library: PresetLibrary

    @Published private(set) var presets: [Preset]
    @Published private(set) var currentIndex = 0
    @Published private(set) var folderName: String
    @Published private(set) var sourceNotes: [AudioSource: String] = [:]
    @Published private(set) var glVersion = ""

    @Published var autoChangeEnabled = false {
        didSet { applyTransitionConfiguration() }
    }

    @Published private(set) var autoInterval: TimeInterval = 30 {
        didSet { applyTransitionConfiguration() }
    }

    @Published private(set) var blendDuration = VisualizerSession.defaultBlendDuration {
        didSet { applyTransitionConfiguration() }
    }

    @Published var hardCutsEnabled = false {
        didSet { applyTransitionConfiguration() }
    }

    @Published private(set) var blendPattern = SVBlendPattern.random.rawValue {
        didSet { player.blendPattern = blendPattern }
    }

    @Published var fpsSetting: Float {
        didSet { player.preferredFramesPerSecond = fpsSetting }
    }

    @Published private(set) var audioSource: AudioSource = .wav
    @Published private(set) var loadError: String?

    @Published var debugEnabled = false {
        didSet { player.debugEnabled = debugEnabled }
    }

    private var securityScopedFolderURL: URL?
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
        applyTransitionConfiguration()
        player.presetRequestHandler = { [weak self] request in
            DispatchQueue.main.async { self?.handle(request) }
        }
        player.$lastError
            .receive(on: DispatchQueue.main)
            .sink { [weak self] error in
                guard let error, self?.loadError == nil else { return }
                self?.loadError = error.localizedDescription
            }
            .store(in: &cancellables)
    }

    deinit {
        player.presetRequestHandler = nil
        securityScopedFolderURL?.stopAccessingSecurityScopedResource()
    }

    var currentPreset: Preset? {
        presets.indices.contains(currentIndex) ? presets[currentIndex] : nil
    }

    var presetCount: Int { presets.count }

    var fpsLabel: String { fpsSetting > 0 ? "\(Int(fpsSetting)) fps" : "Uncapped" }

    var fpsSegmentLabel: String { fpsSetting > 0 ? String(Int(fpsSetting)) : "Max" }

    var capLabel: String { fpsSetting > 0 ? String(Int(fpsSetting)) : "none" }

    var pillText: String {
        let state = player.isPaused ? "Paused · " : ""
        return "\(state)\(fpsLabel) · \(audioSource.label)"
    }

    var positionText: String { "\(currentIndex + 1) of \(presetCount)" }

    var nowPlayingSubtitle: String {
        guard autoChangeEnabled else { return "\(positionText) · Auto-change off" }
        return "\(positionText) · Auto-change every \(Int(autoInterval)) s"
    }

    var autoText: String {
        guard autoChangeEnabled else { return "off" }
        return "every \(Int(autoInterval)) s"
    }

    var transitionLabel: String { SVBlendPattern.name(for: blendPattern) }

    var blendDurationLabel: String { Self.blendDurationLabel(blendDuration) }

    var hardCutLoudness: Float {
        let stored = UserDefaults.standard.float(forKey: "HardCutLoudness")
        return stored > 0 ? stored : 2.5
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

    static func blendDurationLabel(_ seconds: Float) -> String {
        seconds <= 0 ? "Instant" : String(format: "%g s", seconds)
    }

    static func rateLabel(_ sampleRate: Int) -> String {
        String(format: "%.1f kHz", Double(sampleRate) / 1000)
    }

    func start() {
        let defaults = UserDefaults.standard
        let launchInterval = defaults.double(forKey: "AutoChangeSeconds")
        if launchInterval > 0 {
            autoInterval = launchInterval
            autoChangeEnabled = true
        }
        hardCutsEnabled = defaults.bool(forKey: "HardCuts")
        applySource(.wav, revertingTo: nil)
        select(0, blend: 0)
    }

    func select(_ index: Int, blend: Float? = nil) {
        guard presets.indices.contains(index) else { return }
        currentIndex = index
        load(presets[index], blend: blend ?? blendDuration)
    }

    func reloadCurrentPreset() {
        guard let preset = currentPreset else { return }
        load(preset, blend: 0)
    }

    func advance(by offset: Int, blend: Float? = nil) {
        guard !presets.isEmpty else { return }
        let count = presets.count
        select(((currentIndex + offset) % count + count) % count, blend: blend)
    }

    func select(source: AudioSource) {
        guard source != .system, source != audioSource else { return }
        applySource(source, revertingTo: audioSource)
    }

    func setAutoInterval(_ interval: TimeInterval) {
        autoInterval = interval
    }

    func setBlendDuration(_ seconds: Float) {
        blendDuration = seconds
    }

    func setBlendPattern(_ pattern: Int) {
        blendPattern = pattern
    }

    func chooseFolder(_ url: URL) {
        let started = url.startAccessingSecurityScopedResource()
        securityScopedFolderURL?.stopAccessingSecurityScopedResource()
        securityScopedFolderURL = started ? url : nil
        folderName = url.lastPathComponent
        let found = Self.presets(in: url)
        if !found.isEmpty { presets = found }
        currentIndex = 0
        if let first = presets.first { load(first) }
    }

    func togglePause() {
        player.isPaused ? player.resume() : player.pause()
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

    private func load(_ preset: Preset, blend: Float? = nil) {
        loadError = nil
        Task { try? await player.loadPreset(at: preset.url, blendSeconds: blend ?? blendDuration) }
    }

    private func applyTransitionConfiguration() {
        player.updateTransitionConfiguration { configuration in
            configuration.softCutSeconds = blendDuration
            configuration.presetDuration = autoChangeEnabled ? Float(autoInterval) : 0
            configuration.presetDurationRandom = 0
            configuration.hardCutsEnabled = hardCutsEnabled
            configuration.hardCutLoudness = hardCutLoudness
        }
    }

    private func handle(_ request: SVPresetRequest) {
        let hardCut = request.reason == .hardCut
        let blend = hardCut ? 0 : request.suggestedBlendSeconds
        NSLog("%@", "iOSUIKitSample: preset request reason=\(hardCut ? "hard-cut" : "timer") blend=\(blend)")
        advance(by: 1, blend: blend)
    }

    private static func presets(in directory: URL) -> [Preset] {
        let files = (try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)) ?? []
        return files
            .filter { $0.pathExtension == "milk" }
            .sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
            .map { Preset(id: nil, url: $0) }
    }
}
