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
    private var syncMembership: (group: SVSyncGroup?, scaling: SVSyncScaling)?

    public init(player: SynesthesiaPlayer) {
        self.player = player
    }

    public func syncGroup(_ group: SVSyncGroup?, scaling: SVSyncScaling = .fill) -> SynesthesiaView {
        var view = self
        view.syncMembership = (group, scaling)
        return view
    }

    private func makeVisualizerView() -> SVView {
        let view = SVView(frame: .zero)
        applySyncMembership()
        player.attach(to: view)
        return view
    }

    private func updateVisualizerView(_ view: SVView) {
        applySyncMembership()
        player.attach(to: view)
    }

    private func applySyncMembership() {
        guard let syncMembership else { return }
        if player.syncScaling != syncMembership.scaling {
            player.syncScaling = syncMembership.scaling
        }
        if player.syncGroup !== syncMembership.group {
            player.syncGroup = syncMembership.group
        }
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
        updateVisualizerView(uiView)
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
        updateVisualizerView(nsView)
    }

    public static func dismantleNSView(_ nsView: SVView, coordinator: Void) {
        dismantleVisualizerView(nsView)
    }
}
#endif
#endif
