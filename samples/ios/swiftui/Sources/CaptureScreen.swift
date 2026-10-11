//
//  CaptureScreen.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI

struct CaptureScreen: View {

    let request: CaptureRequest

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            Text("Capturing \(request.preset.name)\n\(request.frames.count) frames")
                .foregroundColor(.white)
                .multilineTextAlignment(.center)
                .padding(16)
        }
        .onAppear {
            CaptureRunner.start(request)
        }
    }
}
