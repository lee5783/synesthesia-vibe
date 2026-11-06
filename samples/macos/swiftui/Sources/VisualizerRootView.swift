//
//  VisualizerRootView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI
import SynesthesiaVibe

struct VisualizerRootView: View {

    @ObservedObject var model: VisualizerModel

    var body: some View {
        VisualizerScreen(model: model)
            .preferredColorScheme(.dark)
            .tint(Color.svAccentPrimary)
    }
}
