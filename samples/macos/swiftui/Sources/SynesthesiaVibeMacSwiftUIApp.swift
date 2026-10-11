//
//  SynesthesiaVibeMacSwiftUIApp.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SwiftUI

@main
enum SynesthesiaVibeMacSwiftUIMain {

    static func main() {
        let arguments = ProcessInfo.processInfo.arguments
        if arguments.contains(CaptureCommand.option) {
            exit(CaptureCommand.run(arguments: arguments, library: PresetLibrary(bundle: .main)))
        }
        SynesthesiaVibeMacSwiftUIApp.main()
    }
}

struct SynesthesiaVibeMacSwiftUIApp: App {

    private let library = PresetLibrary(bundle: .main)

    var body: some Scene {
        Window("SynesthesiaVibe SwiftUI", id: "visualizer") {
            VisualizerRootView(library: library)
        }
        .defaultSize(width: 1024, height: 640)
    }
}
