//
//  SynesthesiaVibeSwiftUIApp.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI

@main
struct SynesthesiaVibeSwiftUIApp: App {

    private let library: PresetLibrary
    private let captureRequest: CaptureRequest?

    init() {
        let library = PresetLibrary(bundle: .main)
        do {
            captureRequest = try CaptureRequest(arguments: ProcessInfo.processInfo.arguments, library: library)
        } catch {
            CaptureRunner.finish(with: .failure(error))
        }
        self.library = library
    }

    var body: some Scene {
        WindowGroup {
            if let captureRequest {
                CaptureScreen(request: captureRequest)
            } else {
                VisualizerRootView(library: library)
            }
        }
    }
}
