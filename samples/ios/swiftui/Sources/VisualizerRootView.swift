//
//  VisualizerRootView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI
import SynesthesiaVibe

struct VisualizerRootView: View {

    let library: PresetLibrary
    @StateObject private var store: VisualizerStore

    init(library: PresetLibrary) {
        self.library = library
        let configuration = SVContextConfiguration()
        configuration.resourceURL = library.resourceURL
        let player = SynesthesiaPlayer(configuration: configuration)
        _store = StateObject(wrappedValue: VisualizerStore(
            library: library,
            player: player,
            folderName: VisualizerStore.bundledFolderName,
            fps: 60
        ))
    }

    var body: some View {
        VisualizerScreen(store: store)
            .preferredColorScheme(.dark)
            .tint(DesignTokens.accentText)
    }
}
