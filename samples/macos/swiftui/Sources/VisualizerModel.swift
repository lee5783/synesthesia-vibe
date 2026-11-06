//
//  VisualizerModel.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import Combine
import Foundation
import OpenGL.GL3
import SwiftUI
import SynesthesiaVibe

@MainActor
final class VisualizerModel: ObservableObject {

    static let defaultInterval: Double = 30
    private static let liveInputSampleRate: Double = 44_100
    private static let tickInterval: Double = 0.5

    @Published var library: PresetLibrary
    @Published var selectedPresetName: String?
    @Published var isAutoChangeOn = false {
        didSet { resetCountdown() }
    }
    @Published var autoChangeInterval = VisualizerModel.defaultInterval {
        didSet { resetCountdown() }
    }
    @Published var audioSource = AudioSource.wav
    @Published var frameRate: Float {
        didSet { player.preferredFramesPerSecond = frameRate }
    }
    @Published var debugOverlayOn = false {
        didSet { player.debugEnabled = debugOverlayOn }
    }
    @Published var isUserPaused = false
    @Published var audioError: String?
    @Published var glVersion: String?

    let player: SynesthesiaPlayer

    @Published private var secondsUntilChange: Double = VisualizerModel.defaultInterval
    private var ticker: AnyCancellable?
    private var contextProbe: AnyCancellable?
    private var playerChanges: AnyCancellable?
    private var previousSource = AudioSource.wav
    private var folderScopeURL: URL?
    private var flatLevelSeconds = 0.0
    private var didShowDeniedHint = false

    var presets: [Preset] { library.presets }

    var currentIndex: Int? {
        guard let selectedPresetName else { return nil }
        return presets.firstIndex { $0.name == selectedPresetName }
    }

    var currentPreset: Preset? {
        guard let currentIndex else { return nil }
        return presets[currentIndex]
    }

    var title: String { currentPreset?.name ?? "Visualizer" }

    var subtitle: String {
        let position = currentIndex.map { "\($0 + 1) of \(presets.count)" } ?? "0 of \(presets.count)"
        guard isAutoChangeOn else { return "\(position) · Auto-change off" }
        return "\(position) · Next preset in \(Int(ceil(secondsUntilChange))) s"
    }

    var fpsCapText: String {
        player.preferredFramesPerSecond > 0 ? String(format: "%.0f", player.preferredFramesPerSecond) : "Uncapped"
    }

    var sampleRate: Double { VisualizerModel.liveInputSampleRate }

    var autoText: String {
        isAutoChangeOn ? "every \(Int(autoChangeInterval)) s · next \(Int(ceil(secondsUntilChange))) s" : "off"
    }

    var renderScale: CGFloat { player.view?.renderScale ?? 1 }

    var systemAudioSupported: Bool { SynesthesiaPlayer.isSystemAudioSupported }

    private static var launchMaxFPS: Float? {
        let arguments = ProcessInfo.processInfo.arguments
        guard let index = arguments.firstIndex(of: "--max-fps"), index + 1 < arguments.count else { return nil }
        guard let value = Float(arguments[index + 1]), value.isFinite else { return 0 }
        return max(value, 0)
    }

    init(library: PresetLibrary) {
        self.library = library
        let configuration = SVContextConfiguration()
        configuration.resourceURL = library.resourceURL
        configuration.maxFps = VisualizerModel.launchMaxFPS ?? 60
        let player = SynesthesiaPlayer(configuration: configuration)
        self.player = player
        self.frameRate = player.preferredFramesPerSecond
        self.selectedPresetName = library.presets.first?.name
        playerChanges = Publishers.Merge3(
            player.$presetName.map { _ in () },
            player.$preferredFramesPerSecond.map { _ in () },
            player.$lastError.map { _ in () }
        )
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in self?.objectWillChange.send() }
        startTicker()
        startWav()
        if let first = library.presets.first {
            load(first, blend: 0)
        }
        startContextProbe()
    }

    func load(_ preset: Preset, blend: Float = 2) {
        selectedPresetName = preset.name
        resetCountdown()
        Task { [weak self] in
            do {
                try await self?.player.loadPreset(at: preset.url, blendSeconds: blend)
            } catch is CancellationError {
            } catch {
                self?.audioError = error.localizedDescription
            }
        }
    }

    func selectPreset(_ name: String) {
        guard let preset = presets.first(where: { $0.name == name }) else { return }
        load(preset)
    }

    func step(by offset: Int) {
        guard !presets.isEmpty else { return }
        let index = currentIndex ?? 0
        let next = ((index + offset) % presets.count + presets.count) % presets.count
        load(presets[next])
    }

    func togglePause() {
        isUserPaused.toggle()
        if isUserPaused {
            player.pause()
        } else {
            player.resume()
        }
    }

    func toggleDebugOverlay() {
        debugOverlayOn.toggle()
    }

    func pickFolder(_ url: URL) {
        folderScopeURL?.stopAccessingSecurityScopedResource()
        folderScopeURL = url.startAccessingSecurityScopedResource() ? url : nil
        let library = PresetLibrary(folderURL: url, defaultWavURL: self.library.defaultWavURL)
        self.library = library
        selectedPresetName = library.presets.first?.name
        resetCountdown()
        if let first = library.presets.first {
            load(first, blend: 0)
        }
    }

    func selectAudioSource(_ source: AudioSource) {
        guard source != audioSource else { return }
        switch source {
        case .none:
            player.stopAudio()
            applySource(.none)
        case .wav:
            startWav()
        case .microphone:
            do {
                try player.startMicrophone()
                applySource(.microphone)
            } catch {
                revertSource("Microphone unavailable", error)
            }
        case .system:
            guard systemAudioSupported else {
                revertSource("System sound needs macOS 14.2 or later", nil)
                return
            }
            do {
                try player.startSystemAudio()
                applySource(.system)
            } catch {
                revertSource("System sound unavailable", error)
            }
        }
    }

    private func applySource(_ source: AudioSource) {
        previousSource = source
        audioSource = source
        audioError = nil
        flatLevelSeconds = 0
        didShowDeniedHint = false
    }

    private func revertSource(_ reason: String, _ error: Error?) {
        restorePreviousSource()
        audioError = error.map { "\(reason): \($0.localizedDescription)" } ?? reason
    }

    private func restorePreviousSource() {
        switch previousSource {
        case .none:
            player.stopAudio()
            applySource(.none)
        case .wav:
            startWav()
        case .microphone:
            do {
                try player.startMicrophone()
                applySource(.microphone)
            } catch {
                startWav()
            }
        case .system:
            do {
                try player.startSystemAudio()
                applySource(.system)
            } catch {
                startWav()
            }
        }
    }

    private func startWav() {
        guard let wavURL = library.defaultWavURL else { return }
        do {
            try player.playWav(at: wavURL)
            applySource(.wav)
        } catch {
            audioError = error.localizedDescription
        }
    }

    private func startTicker() {
        ticker = Timer.publish(every: VisualizerModel.tickInterval, on: .main, in: .common)
            .autoconnect()
            .sink { [weak self] _ in
                self?.tick()
            }
    }

    private func tick() {
        updateFlatLevelHint()
        guard isAutoChangeOn, !isUserPaused else { return }
        secondsUntilChange -= VisualizerModel.tickInterval
        guard secondsUntilChange <= 0 else { return }
        guard !presets.isEmpty else { return }
        let index = currentIndex ?? -1
        load(presets[(index + 1) % presets.count])
    }

    private func resetCountdown() {
        secondsUntilChange = autoChangeInterval
    }

    private func updateFlatLevelHint() {
        guard audioSource == .system, audioError == nil, !didShowDeniedHint else { return }
        let levels = player.levels
        guard abs(levels.bass) < 0.01, abs(levels.mid) < 0.01, abs(levels.treble) < 0.01 else {
            flatLevelSeconds = 0
            return
        }
        flatLevelSeconds += VisualizerModel.tickInterval
        guard flatLevelSeconds >= 4 else { return }
        didShowDeniedHint = true
        audioError = "No system sound detected. If you denied the request, allow it in System Settings › Privacy & Security › Screen & System Audio Recording."
    }

    private func startContextProbe() {
        contextProbe = Timer.publish(every: VisualizerModel.tickInterval, on: .main, in: .common)
            .autoconnect()
            .sink { [weak self] _ in
                self?.readGLVersion()
            }
    }

    private func readGLVersion() {
        guard glVersion == nil, let view = player.view, view.context != nil else { return }
        var version: String?
        view.perform(contextCurrent: {
            guard let raw = glGetString(GLenum(GL_VERSION)) else { return }
            version = String(cString: raw)
        })
        guard let version else { return }
        glVersion = version
        contextProbe?.cancel()
        contextProbe = nil
    }
}
