//
//  VisualizerScreen.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI
import SynesthesiaVibe

struct VisualizerScreen: View {

    private enum AudioSource {
        case wav
        case microphone
    }

    let library: PresetLibrary
    let player: SynesthesiaPlayer

    @Environment(\.scenePhase) private var scenePhase
    @State private var selectedPresetURL: URL?
    @State private var audioSource = AudioSource.wav
    @State private var isUserPaused = false
    @State private var audioError: String?

    var body: some View {
        NavigationView {
            GeometryReader { geometry in
                let size = geometry.size
                let isLandscape = size.width > size.height
                let visualizerSize = isLandscape
                    ? CGSize(width: (size.width * 0.62).rounded(), height: size.height)
                    : CGSize(width: size.width, height: (size.height * 0.5).rounded())
                ZStack(alignment: .topLeading) {
                    SynesthesiaView(player: player)
                        .frame(width: visualizerSize.width, height: visualizerSize.height)
                        .overlay(alignment: .bottomLeading) {
                            PlayerStatusView(player: player, audioError: audioError)
                                .padding(8)
                        }
                    presetList
                        .frame(
                            width: isLandscape ? size.width - visualizerSize.width : size.width,
                            height: isLandscape ? size.height : size.height - visualizerSize.height
                        )
                        .offset(x: isLandscape ? visualizerSize.width : 0, y: isLandscape ? 0 : visualizerSize.height)
                }
            }
            .navigationTitle("SynesthesiaVibe")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button(audioSource == .wav ? "Mic" : "WAV", action: toggleAudioSource)
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(isUserPaused ? "Play" : "Pause", action: togglePause)
                }
            }
        }
        .navigationViewStyle(.stack)
        .task {
            startWav()
            guard let preset = library.presets.first else { return }
            selectedPresetURL = preset.url
            try? await player.loadPreset(at: preset.url)
        }
        .onChange(of: scenePhase) { phase in
            if phase == .active {
                if !isUserPaused {
                    player.resume()
                }
            } else {
                player.pause()
            }
        }
    }

    private var presetList: some View {
        List(library.presets, id: \.url) { preset in
            Button {
                select(preset)
            } label: {
                HStack {
                    Text(preset.name)
                        .lineLimit(1)
                        .foregroundColor(.primary)
                    Spacer()
                    if preset.url == selectedPresetURL {
                        Image(systemName: "checkmark")
                            .foregroundColor(.accentColor)
                    }
                }
            }
        }
        .listStyle(.plain)
    }

    private func select(_ preset: Preset) {
        selectedPresetURL = preset.url
        Task {
            try? await player.loadPreset(at: preset.url, blendSeconds: 0)
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
            audioError = error.localizedDescription
            startWav()
        }
    }

    private func toggleAudioSource() {
        switch audioSource {
        case .wav: startMicrophone()
        case .microphone: startWav()
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
