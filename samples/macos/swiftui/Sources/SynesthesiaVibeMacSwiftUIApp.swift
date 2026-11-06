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

    @StateObject private var model = VisualizerModel(library: PresetLibrary(bundle: .main))

    var body: some Scene {
        Window("SynesthesiaVibe SwiftUI", id: "visualizer") {
            VisualizerRootView(model: model)
        }
        .defaultSize(width: 1180, height: 760)
        .commands {
            CommandGroup(after: .toolbar) {
                Button(model.debugOverlayOn ? "Hide Debug Overlay" : "Show Debug Overlay") {
                    model.toggleDebugOverlay()
                }
                .keyboardShortcut("d", modifiers: [.shift, .command])
            }
        }
    }
}
