//
//  VisualizerScreen.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI
import SynesthesiaVibe

struct VisualizerScreen: View {

    private enum AudioSource: String, CaseIterable, Identifiable {
        case wav = "WAV"
        case microphone = "Microphone"

        var id: Self { self }
    }

    let library: PresetLibrary
    let player: SynesthesiaPlayer

    @AppStorage("selectedPreset") private var selectedPresetName = ""
    @State private var audioSource = AudioSource.wav
    @State private var isUserPaused = false
    @State private var audioError: String?

    var body: some View {
        SynesthesiaView(player: player)
            .frame(minWidth: 480, minHeight: 300)
            .overlay(alignment: .bottomLeading) {
                PlayerStatusView(player: player, audioError: audioError)
                    .padding(8)
            }
            .toolbar {
                ToolbarItemGroup {
                    Picker("Preset", selection: $selectedPresetName) {
                        ForEach(library.presets, id: \.self) { preset in
                            Text(preset.name).tag(preset.name)
                        }
                    }
                    .frame(minWidth: 260)
                    .accessibilityIdentifier("preset-picker")
                    Picker("Audio", selection: audioSourceBinding) {
                        ForEach(AudioSource.allCases) { source in
                            Text(source.rawValue).tag(source)
                        }
                    }
                    .pickerStyle(.segmented)
                    .accessibilityIdentifier("audio-source-picker")
                    Button(isUserPaused ? "Play" : "Pause", action: togglePause)
                        .accessibilityIdentifier("pause-button")
                }
            }
            .task {
                if !library.presets.contains(where: { $0.name == selectedPresetName }) {
                    selectedPresetName = library.presets.first?.name ?? ""
                }
                startWav()
            }
            .task(id: selectedPresetName) {
                guard let preset = library.presets.first(where: { $0.name == selectedPresetName }) else { return }
                try? await player.loadPreset(at: preset.url)
            }
    }

    private var audioSourceBinding: Binding<AudioSource> {
        Binding(get: { audioSource }, set: select)
    }

    private func select(_ source: AudioSource) {
        switch source {
        case .wav: startWav()
        case .microphone: startMicrophone()
        }
    }

    private func startWav() {
        guard let wavURL = library.defaultWavURL else { return }
        do {
            try player.playWav(at: wavURL)
            audioSource = .wav
        } catch {
            audioError = error.localizedDescription
        }
    }

    private func startMicrophone() {
        do {
            try player.startMicrophone()
            audioSource = .microphone
            audioError = nil
        } catch {
            startWav()
            audioError = "Microphone unavailable: \(error.localizedDescription)"
        }
    }

    private func togglePause() {
        isUserPaused.toggle()
        if isUserPaused {
            player.pause()
        } else {
            player.resume()
        }
    }
}
