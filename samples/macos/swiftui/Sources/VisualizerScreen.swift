//
//  VisualizerScreen.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import AppKit
import SwiftUI
import SynesthesiaVibe

struct VisualizerScreen: View {

    @ObservedObject var model: VisualizerModel

    @State private var columnVisibility = NavigationSplitViewVisibility.all
    @State private var isInspectorPresented = true
    @State private var isFullScreen = false

    var body: some View {
        NavigationSplitView(columnVisibility: $columnVisibility) {
            SidebarView(library: model.library,
                        onFolderPicked: model.pickFolder,
                        selection: selectedPresetBinding)
                .navigationSplitViewColumnWidth(min: 220, ideal: 260, max: 320)
        } detail: {
            visualizer
                .toolbar { toolbarContent }
                .navigationTitle(model.title)
                .navigationSubtitle(model.subtitle)
                .inspector(isPresented: $isInspectorPresented) {
                    InspectorView(isAutoChangeOn: $model.isAutoChangeOn,
                                  autoChangeInterval: $model.autoChangeInterval,
                                  blendPattern: $model.blendPattern,
                                  blendDuration: $model.blendDuration,
                                  hardCutsOn: $model.hardCutsOn,
                                  audioSource: audioSourceBinding,
                                  frameRate: $model.frameRate,
                                  debugOverlayOn: $model.debugOverlayOn,
                                  systemAudioSupported: model.systemAudioSupported)
                        .inspectorColumnWidth(min: 280, ideal: 320, max: 380)
                }
                .frame(minWidth: 1060, minHeight: 560)
        }
        .onReceive(NotificationCenter.default.publisher(for: NSWindow.didEnterFullScreenNotification)) { _ in
            enterFullScreen()
        }
        .onReceive(NotificationCenter.default.publisher(for: NSWindow.didExitFullScreenNotification)) { _ in
            exitFullScreen()
        }
    }

    private var visualizer: some View {
        ZStack(alignment: .topLeading) {
            Color.svLetterbox
            SynesthesiaView(player: model.player)
                .accessibilityLabel("Visualizer, \(model.title)")
            overlay
            overVideoButtons
        }
        .background(Color.svLetterbox)
    }

    private var overlay: some View {
        VStack(alignment: .leading, spacing: 8) {
            StatusPill(isPaused: model.isUserPaused,
                       fpsText: model.fpsCapText,
                       sourceLabel: model.audioSource.label)
            if model.debugOverlayOn {
                DebugOverlayHost(model: model, player: model.player)
            }
            if let error = model.audioError {
                Text(error)
                    .font(.system(size: 11))
                    .foregroundStyle(Color.svDanger)
                    .padding(8)
                    .frame(maxWidth: 260, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 8, style: .continuous).fill(Color.svDangerSubtle))
            }
        }
        .padding(.leading, 12)
        .padding(.top, 12)
    }

    private var overVideoButtons: some View {
        HStack(spacing: 6) {
            Spacer(minLength: 0)
            OverVideoButton(systemImage: "ladybug",
                            label: "Debug overlay",
                            isOn: model.debugOverlayOn,
                            help: "Debug Overlay (⇧⌘D)",
                            action: model.toggleDebugOverlay)
            OverVideoButton(systemImage: isFullScreen ? "arrow.down.right.and.arrow.up.left" : "arrow.up.left.and.arrow.down.right",
                            label: isFullScreen ? "Exit full screen" : "Enter full screen",
                            isOn: false,
                            help: "\(isFullScreen ? "Exit" : "Enter") Full Screen (⌃⌘F)",
                            action: toggleFullScreen)
        }
        .frame(maxWidth: .infinity, alignment: .trailing)
        .padding(.top, 10)
        .padding(.trailing, 10)
    }

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItemGroup {
            ControlGroup {
                Button {
                    model.step(by: -1)
                } label: {
                    Image(systemName: "backward.end")
                }
                .help("Previous preset")
                .accessibilityLabel("Previous preset")

                Button {
                    model.togglePause()
                } label: {
                    Image(systemName: model.isUserPaused ? "play.fill" : "pause.fill")
                }
                .help(model.isUserPaused ? "Play" : "Pause")
                .accessibilityLabel(model.isUserPaused ? "Play" : "Pause")

                Button {
                    model.step(by: 1)
                } label: {
                    Image(systemName: "forward.end")
                }
                .help("Next preset")
                .accessibilityLabel("Next preset")
            }
            Button {
                isInspectorPresented.toggle()
            } label: {
                Image(systemName: "sidebar.right")
            }
            .help("Hide inspector")
            .accessibilityLabel("Hide inspector")
        }
    }

    private var selectedPresetBinding: Binding<String?> {
        Binding(get: { model.selectedPresetName }, set: { name in
            guard let name else { return }
            model.selectPreset(name)
        })
    }

    private var audioSourceBinding: Binding<AudioSource> {
        Binding(get: { model.audioSource }, set: { model.selectAudioSource($0) })
    }

    private func toggleFullScreen() {
        NSApp.keyWindow?.toggleFullScreen(nil)
    }

    private func enterFullScreen() {
        isFullScreen = true
        columnVisibility = .detailOnly
        isInspectorPresented = false
    }

    private func exitFullScreen() {
        isFullScreen = false
        columnVisibility = .all
        isInspectorPresented = true
    }
}

private struct DebugOverlayHost: View {

    @ObservedObject var model: VisualizerModel
    @ObservedObject var player: SynesthesiaPlayer

    var body: some View {
        DebugOverlayCard(info: player.debugInfo,
                         fpsCap: player.preferredFramesPerSecond,
                         sourceLabel: model.audioSource.label,
                         sampleRate: model.sampleRate,
                         glVersion: model.glVersion,
                         renderScale: model.renderScale,
                         autoText: model.autoText,
                         transitionText: model.transitionText,
                         levels: player.levels)
    }
}
