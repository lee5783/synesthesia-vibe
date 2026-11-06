//
//  VisualizerScreen.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI
import SynesthesiaVibe
import UniformTypeIdentifiers

struct VisualizerScreen: View {

    @ObservedObject var store: VisualizerStore
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        NavigationView {
            List {
                Section {
                    VisualizerStage(store: store)
                        .listRowInsets(EdgeInsets())
                        .listRowBackground(Color.clear)
                        .listRowSeparator(.hidden)
                    NowPlayingRow(store: store)
                        .listRowInsets(EdgeInsets())
                        .listRowBackground(DesignTokens.backgroundPrimary)
                }
                .listRowBackground(DesignTokens.backgroundSecondary)

                presetSection
                autoChangeSection
                soundSourceSection
                playbackSection
                developerSection
            }
            .listStyle(.insetGrouped)
            .background(DesignTokens.backgroundSecondary)
            .environment(\.defaultMinListRowHeight, 44)
            .navigationTitle("Visualizer")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    NavigationLink(destination: PlaylistScreen(store: store)) {
                        Image(systemName: "music.note.list")
                    }
                    .accessibilityLabel("Open playlist")
                }
            }
        }
        .navigationViewStyle(.stack)
        .fullScreenCover(isPresented: $store.isFullScreen) {
            FullScreenScreen(store: store)
        }
        .task { store.start() }
        .onChange(of: scenePhase) { phase in
            if phase == .active {
                if !store.userPaused { store.player.resume() }
            } else {
                store.player.pause()
            }
        }
    }

    private var presetSection: some View {
        Section {
            NavigationLink(destination: PlaylistScreen(store: store)) {
                HStack {
                    Text("Playlist")
                        .foregroundColor(DesignTokens.textPrimary)
                    Spacer()
                    Text("\(store.folderName) · \(store.presetCount)")
                        .foregroundColor(DesignTokens.textMuted)
                }
            }
        } header: {
            Text("Presets")
        } footer: {
            Text("Pick a preset or change the presets folder.")
        }
    }

    private var autoChangeSection: some View {
        Section {
            Toggle("Auto-change presets", isOn: $store.autoChangeEnabled)
                .tint(DesignTokens.accentPrimary)
                .foregroundColor(DesignTokens.textPrimary)
            NavigationLink(destination: IntervalScreen(store: store)) {
                HStack {
                    Text("Every")
                        .foregroundColor(store.autoChangeEnabled ? DesignTokens.textPrimary : DesignTokens.textDisabled)
                    Spacer()
                    Text(store.intervalLabel)
                        .foregroundColor(store.autoChangeEnabled ? DesignTokens.accentText : DesignTokens.textDisabled)
                }
            }
            .disabled(!store.autoChangeEnabled)
        } header: {
            Text("Auto-change")
        } footer: {
            Text("Moves to the next preset in the playlist.")
        }
    }

    private var soundSourceSection: some View {
        Section {
            ForEach(VisualizerStore.AudioSource.allCases) { source in
                Button {
                    store.select(source: source)
                } label: {
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(source.label)
                                .foregroundColor(source == .system ? DesignTokens.textDisabled : DesignTokens.textPrimary)
                            if let note = store.note(for: source) {
                                Text(note)
                                    .font(.footnote)
                                    .foregroundColor(DesignTokens.textMuted)
                            }
                        }
                        Spacer()
                        if source == store.audioSource {
                            Image(systemName: "checkmark")
                                .foregroundColor(DesignTokens.accentText)
                        }
                    }
                }
                .disabled(source == .system)
                .opacity(source == .system ? 0.45 : 1)
                .accessibilityAddTraits(source == store.audioSource ? .isSelected : [])
            }
        } header: {
            Text("Sound source")
        } footer: {
            Text("iOS doesn’t let apps capture other apps’ audio.")
        }
    }

    private var playbackSection: some View {
        Section {
            Picker("Frame rate", selection: $store.fpsSetting) {
                ForEach(VisualizerStore.frameRates, id: \.self) { rate in
                    Text(rate > 0 ? String(Int(rate)) : "Max").tag(rate)
                }
            }
            .pickerStyle(.segmented)
            NavigationLink(destination: TransitionScreen()) {
                HStack {
                    Text("Transition")
                        .foregroundColor(DesignTokens.textPrimary)
                    Spacer()
                    Text(VisualizerStore.transitionName)
                        .foregroundColor(DesignTokens.accentText)
                }
            }
        } header: {
            Text("Playback")
        } footer: {
            Text("Frame rate caps rendering; animation speed stays the same.")
        }
    }

    private var developerSection: some View {
        Section {
            Toggle("Debug overlay", isOn: $store.debugEnabled)
                .tint(DesignTokens.accentPrimary)
                .foregroundColor(DesignTokens.textPrimary)
        } header: {
            Text("Developer")
        } footer: {
            Text("Shows FPS, frame time, audio levels and preset info over the visualization, in full screen too.")
        }
    }
}

private struct VisualizerStage: View {

    @ObservedObject var store: VisualizerStore

    var body: some View {
        GeometryReader { geometry in
            let side = geometry.size.width
            ZStack(alignment: .topLeading) {
                VisualizerCanvas(player: store.player) { view in
                    store.attachView(view)
                }
                .frame(width: side, height: side)
                if store.debugEnabled {
                    DebugCard(store: store)
                        .padding(.leading, 12)
                        .padding(.top, 46)
                }
                HStack(spacing: 0) {
                    StatusPill(store: store)
                    Spacer()
                    OverlayButton(systemName: "ladybug", isOn: store.debugEnabled, label: "Debug overlay") {
                        store.debugEnabled.toggle()
                    }
                    OverlayButton(systemName: "arrow.up.left.and.arrow.down.right", isOn: false, label: "Enter full screen") {
                        store.isFullScreen = true
                    }
                }
                .padding(.horizontal, 4)
                .padding(.top, 4)
                .frame(width: side, alignment: .leading)
            }
            .frame(width: side, height: side)
            .background(Color.black)
        }
        .frame(height: UIScreen.main.bounds.width)
    }
}

private struct NowPlayingRow: View {

    @ObservedObject var store: VisualizerStore

    var body: some View {
        HStack(spacing: 4) {
            VStack(alignment: .leading, spacing: 0) {
                Text(store.currentPreset?.name ?? "--")
                    .font(.subheadline)
                    .fontWeight(.semibold)
                    .foregroundColor(DesignTokens.textPrimary)
                    .lineLimit(1)
                    .accessibilityLabel("Current preset")
                Text(store.nowPlayingSubtitle)
                    .font(.footnote)
                    .foregroundColor(DesignTokens.textMuted)
            }
            Spacer()
            Button { store.advance(by: -1) } label: {
                Image(systemName: "backward.end").frame(width: 44, height: 44)
            }
            .accessibilityLabel("Previous preset")
            Button { store.togglePause() } label: {
                Image(systemName: store.player.isPaused ? "play.fill" : "pause.fill")
                    .frame(width: 44, height: 44)
                    .foregroundColor(DesignTokens.textOnAccent)
                    .background(DesignTokens.accentPrimary)
                    .clipShape(Circle())
            }
            .accessibilityLabel(store.player.isPaused ? "Play" : "Pause")
            Button { store.advance(by: 1) } label: {
                Image(systemName: "forward.end").frame(width: 44, height: 44)
            }
            .accessibilityLabel("Next preset")
        }
        .padding(.leading, 16)
        .padding(.trailing, 8)
        .padding(.vertical, 8)
        .foregroundColor(DesignTokens.textPrimary)
        .background(DesignTokens.backgroundPrimary)
    }
}

private struct StatusPill: View {

    @ObservedObject var store: VisualizerStore

    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(store.player.isPaused ? DesignTokens.textMuted : DesignTokens.success)
                .frame(width: 6, height: 6)
            Text(store.pillText)
                .font(.caption2)
                .foregroundColor(DesignTokens.textPrimary)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 4)
        .background(DesignTokens.overlay)
        .clipShape(Capsule())
        .accessibilityElement(children: .combine)
    }
}

private struct OverlayButton: View {

    let systemName: String
    let isOn: Bool
    let label: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: systemName)
                .font(.system(size: 18))
                .foregroundColor(DesignTokens.textOnAccent)
                .frame(width: 32, height: 32)
                .background(isOn ? DesignTokens.accentPrimary : DesignTokens.overlay)
                .clipShape(Circle())
                .overlay(Circle().stroke(Color.white.opacity(0.12), lineWidth: 0.5))
                .frame(width: 44, height: 44)
        }
        .accessibilityLabel(label)
        .accessibilityAddTraits(isOn ? .isSelected : [])
    }
}

struct PlaylistScreen: View {

    @ObservedObject var store: VisualizerStore
    @Environment(\.dismiss) private var dismiss
    @State private var showingFolderPicker = false

    var body: some View {
        List {
            Section {
                Button { showingFolderPicker = true } label: {
                    HStack {
                        Image(systemName: "folder").foregroundColor(DesignTokens.info)
                        Text(store.folderName).foregroundColor(DesignTokens.textPrimary)
                        Spacer()
                        Text("Change").foregroundColor(DesignTokens.accentText)
                    }
                }
            } header: {
                Text("Folder")
            }

            Section {
                ForEach(Array(store.presets.enumerated()), id: \.element.url) { index, preset in
                    Button { store.select(index) } label: {
                        HStack {
                            Text(preset.name)
                                .foregroundColor(index == store.currentIndex ? DesignTokens.accentText : DesignTokens.textPrimary)
                                .fontWeight(index == store.currentIndex ? .semibold : .regular)
                            Spacer()
                            if index == store.currentIndex {
                                Image(systemName: "waveform").foregroundColor(DesignTokens.accentText)
                            }
                        }
                    }
                    .accessibilityAddTraits(index == store.currentIndex ? .isSelected : [])
                }
            } header: {
                Text("\(store.presetCount) presets")
            } footer: {
                Text("Tap a preset to play it.")
            }
        }
        .listStyle(.insetGrouped)
        .background(DesignTokens.backgroundSecondary)
        .navigationTitle("Playlist")
        .navigationBarTitleDisplayMode(.large)
        .toolbar {
            ToolbarItem(placement: .navigationBarTrailing) {
                Button { showingFolderPicker = true } label: {
                    Image(systemName: "folder.badge.plus")
                }
                .accessibilityLabel("Choose presets folder")
            }
        }
        .fileImporter(isPresented: $showingFolderPicker, allowedContentTypes: [.folder]) { result in
            if case .success(let url) = result { store.chooseFolder(url) }
        }
        .safeAreaInset(edge: .bottom) {
            MiniPlayerRow(store: store) { dismiss() }
        }
    }
}

private struct MiniPlayerRow: View {

    @ObservedObject var store: VisualizerStore
    let onOpen: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            Button(action: onOpen) {
                HStack(spacing: 12) {
                    Image(systemName: "waveform")
                        .foregroundColor(DesignTokens.accentText)
                        .frame(width: 44, height: 44)
                        .background(DesignTokens.accentSubtle)
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                    VStack(alignment: .leading, spacing: 0) {
                        Text(store.currentPreset?.name ?? "--")
                            .font(.subheadline)
                            .fontWeight(.semibold)
                            .foregroundColor(DesignTokens.textPrimary)
                            .lineLimit(1)
                        Text("Now playing · \(store.positionText)")
                            .font(.footnote)
                            .foregroundColor(DesignTokens.textMuted)
                    }
                }
            }
            .accessibilityLabel("Open visualizer")
            Spacer()
            Button { store.togglePause() } label: {
                Image(systemName: store.player.isPaused ? "play.fill" : "pause.fill")
                    .frame(width: 44, height: 44)
                    .foregroundColor(DesignTokens.textOnAccent)
                    .background(DesignTokens.accentPrimary)
                    .clipShape(Circle())
            }
            .accessibilityLabel(store.player.isPaused ? "Play" : "Pause")
            Button { store.advance(by: 1) } label: {
                Image(systemName: "forward.end").frame(width: 44, height: 44)
            }
            .accessibilityLabel("Next preset")
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 8)
        .background(DesignTokens.backgroundPrimary)
    }
}

struct IntervalScreen: View {

    @ObservedObject var store: VisualizerStore

    var body: some View {
        List {
            ForEach(VisualizerStore.autoIntervals, id: \.self) { interval in
                Button {
                    store.setAutoInterval(interval)
                } label: {
                    HStack {
                        Text(VisualizerStore.intervalLabel(interval))
                            .foregroundColor(DesignTokens.textPrimary)
                        Spacer()
                        if interval == store.autoInterval {
                            Image(systemName: "checkmark").foregroundColor(DesignTokens.accentText)
                        }
                    }
                }
            }
        }
        .listStyle(.insetGrouped)
        .background(DesignTokens.backgroundSecondary)
        .navigationTitle("Every")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct TransitionScreen: View {

    var body: some View {
        List {
            HStack {
                Text(VisualizerStore.transitionName).foregroundColor(DesignTokens.textPrimary)
                Spacer()
                Image(systemName: "checkmark").foregroundColor(DesignTokens.accentText)
            }
        }
        .listStyle(.insetGrouped)
        .background(DesignTokens.backgroundSecondary)
        .navigationTitle("Transition")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct FullScreenScreen: View {

    @ObservedObject var store: VisualizerStore

    var body: some View {
        ZStack(alignment: .topLeading) {
            Color.black.ignoresSafeArea()
            SynesthesiaView(player: store.player)
                .ignoresSafeArea()
                .task { store.reloadCurrentPreset() }
            if store.debugEnabled {
                DebugCard(store: store)
                    .padding(.leading, 16)
                    .padding(.top, 104)
            }
            HStack {
                Spacer()
                OverlayButton(systemName: "ladybug", isOn: store.debugEnabled, label: "Debug overlay") {
                    store.debugEnabled.toggle()
                }
                OverlayButton(systemName: "arrow.down.right.and.arrow.up.left", isOn: false, label: "Exit full screen") {
                    store.isFullScreen = false
                }
            }
            .padding(.trailing, 12)
            .padding(.top, 52)
        }
        .statusBarHidden()
    }
}
