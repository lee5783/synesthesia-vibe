//
//  VisualizerCanvas.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import OpenGLES
import SwiftUI
import SynesthesiaVibe

struct VisualizerCanvas: UIViewRepresentable {

    let player: SynesthesiaPlayer
    let onView: (SVView) -> Void

    func makeUIView(context: Context) -> SVView {
        let view = SVView(frame: .zero)
        player.attach(to: view)
        DispatchQueue.main.async { onView(view) }
        return view
    }

    func updateUIView(_ uiView: SVView, context: Context) {}

    static func dismantleUIView(_ uiView: SVView, coordinator: Void) {
        (uiView.delegate as? SynesthesiaPlayer)?.detach(from: uiView)
    }
}

enum GLVersionReader {

    static func read(from view: SVView) -> String {
        var version = ""
        view.perform(contextCurrent:) {
            guard let raw = glGetString(GLenum(GL_VERSION)) else { return }
            version = String(cString: UnsafeRawPointer(raw).assumingMemoryBound(to: CChar.self))
        }
        return version
    }
}
