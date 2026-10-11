//
//  SynesthesiaView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#if canImport(SwiftUI)
import SwiftUI

public struct SynesthesiaView {

    public let player: SynesthesiaPlayer

    public init(player: SynesthesiaPlayer) {
        self.player = player
    }

    private func makeVisualizerView() -> SVView {
        let view = SVView(frame: .zero)
        player.attach(to: view)
        return view
    }

    private static func dismantleVisualizerView(_ view: SVView) {
        (view.delegate as? SynesthesiaPlayer)?.detach(from: view)
    }
}

#if os(iOS)
extension SynesthesiaView: UIViewRepresentable {

    public func makeUIView(context: Context) -> SVView {
        makeVisualizerView()
    }

    public func updateUIView(_ uiView: SVView, context: Context) {
        player.attach(to: uiView)
    }

    public static func dismantleUIView(_ uiView: SVView, coordinator: Void) {
        dismantleVisualizerView(uiView)
    }
}
#elseif os(macOS)
extension SynesthesiaView: NSViewRepresentable {

    public func makeNSView(context: Context) -> SVView {
        makeVisualizerView()
    }

    public func updateNSView(_ nsView: SVView, context: Context) {
        player.attach(to: nsView)
    }

    public static func dismantleNSView(_ nsView: SVView, coordinator: Void) {
        dismantleVisualizerView(nsView)
    }
}
#endif
#endif
