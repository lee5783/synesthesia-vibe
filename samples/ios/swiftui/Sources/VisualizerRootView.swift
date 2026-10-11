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
    @StateObject private var player: SynesthesiaPlayer

    init(library: PresetLibrary) {
        self.library = library
        let configuration = SVContextConfiguration()
        configuration.resourceURL = library.resourceURL
        _player = StateObject(wrappedValue: SynesthesiaPlayer(configuration: configuration))
    }

    var body: some View {
        VisualizerScreen(library: library, player: player)
    }
}
