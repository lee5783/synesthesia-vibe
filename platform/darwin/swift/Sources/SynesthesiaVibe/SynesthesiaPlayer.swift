//
//  SynesthesiaPlayer.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

@_exported import SynesthesiaVibeCore
import Combine
import Foundation

public final class SynesthesiaPlayer: NSObject, ObservableObject, @unchecked Sendable {

    @Published public private(set) var presetName: String?
    @Published public var isPaused = false {
        didSet { view?.isPaused = isPaused }
    }
    @Published public private(set) var lastError: Error?
    @Published public private(set) var levels = SVAudioLevels(bass: 0, mid: 0, treble: 0)

    public let configuration: SVContextConfiguration
    public private(set) weak var view: SVView?

    private weak var context: SVContext?
    private var pendingLoads: [PendingLoad] = []
    private var wavFeeder: SVWavFeeder?
    private var wavFrameRemainder = 0.0
    private var audioInput: SVAudioInput?
    private var wantsMicrophone = false

    public init(configuration: SVContextConfiguration = SVContextConfiguration()) {
        self.configuration = configuration
        super.init()
    }

    deinit {
        pendingLoads.forEach { $0.continuation.resume(throwing: CancellationError()) }
        audioInput?.stop()
    }

    public func attach(to view: SVView) {
        guard self.view !== view else { return }
        if let previous = self.view {
            detach(from: previous)
        }
        view.configuration = configuration
        view.delegate = self
        view.isPaused = isPaused
        self.view = view
    }

    public func detach(from view: SVView) {
        guard self.view === view else { return }
        view.delegate = nil
        self.view = nil
        context = nil
        audioInput?.stop()
        audioInput = nil
    }

    public func loadPreset(at url: URL, blendSeconds: Float = 0) async throws {
        let ticket = LoadTicket()
        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
                DispatchQueue.main.async {
                    self.enqueue(PendingLoad(ticket: ticket, url: url, blendSeconds: blendSeconds, continuation: continuation))
                }
            }
        } onCancel: {
            DispatchQueue.main.async {
                self.cancel(ticket)
            }
        }
    }

    public func pause() {
        isPaused = true
    }

    public func resume() {
        isPaused = false
    }

    public func playWav(at url: URL) throws {
        let feeder = try SVWavFeeder(url: url)
        stopAudio()
        wavFeeder = feeder
        wavFrameRemainder = 0
    }

    public func startMicrophone() throws {
        stopAudio()
        wantsMicrophone = true
        guard let context else { return }
        try startAudioInput(with: context)
    }

    public func stopAudio() {
        wavFeeder = nil
        wantsMicrophone = false
        audioInput?.stop()
        audioInput = nil
    }

    private func enqueue(_ load: PendingLoad) {
        guard !load.ticket.isCancelled else {
            load.continuation.resume(throwing: CancellationError())
            return
        }
        pendingLoads.append(load)
    }

    private func cancel(_ ticket: LoadTicket) {
        ticket.isCancelled = true
        guard let index = pendingLoads.firstIndex(where: { $0.ticket === ticket }) else { return }
        pendingLoads.remove(at: index).continuation.resume(throwing: CancellationError())
    }

    private func performPendingLoads(with context: SVContext) {
        guard !pendingLoads.isEmpty else { return }
        let loads = pendingLoads
        pendingLoads.removeAll()
        for load in loads {
            do {
                try context.loadPreset(at: load.url, blendSeconds: load.blendSeconds)
                presetName = load.url.deletingPathExtension().lastPathComponent
                lastError = nil
                load.continuation.resume()
            } catch {
                lastError = error
                load.continuation.resume(throwing: error)
            }
        }
    }

    private func failPendingLoads(with error: Error) {
        let loads = pendingLoads
        pendingLoads.removeAll()
        loads.forEach { $0.continuation.resume(throwing: error) }
    }

    private func startAudioInput(with context: SVContext) throws {
        let input = SVAudioInput(context: context)
        try input.start()
        audioInput = input
    }

    private func feedWav(deltaTime: TimeInterval, into context: SVContext) {
        guard let feeder = wavFeeder, feeder.sampleRate > 0 else { return }
        let frames = wavFrameRemainder + deltaTime * Double(feeder.sampleRate)
        let wholeFrames = frames.rounded(.down)
        wavFrameRemainder = frames - wholeFrames
        guard wholeFrames >= 1 else { return }
        feeder.samplesPerRenderFrame = UInt(wholeFrames)
        feeder.feedNextFrame(into: context)
    }
}

extension SynesthesiaPlayer: SVViewDelegate {

    public func view(_ view: SVView, didCreateContext context: SVContext) {
        self.context = context
        performPendingLoads(with: context)
        guard wantsMicrophone, audioInput == nil else { return }
        do {
            try startAudioInput(with: context)
        } catch {
            lastError = error
        }
    }

    public func view(_ view: SVView, willRenderFrameWithDeltaTime deltaTime: TimeInterval) {
        guard let context = view.context else { return }
        self.context = context
        performPendingLoads(with: context)
        feedWav(deltaTime: deltaTime, into: context)
        levels = context.audioLevels
    }

    public func view(_ view: SVView, didFailWithError error: Error) {
        lastError = error
        guard context == nil else { return }
        failPendingLoads(with: error)
    }
}

private final class LoadTicket: @unchecked Sendable {
    var isCancelled = false
}

private struct PendingLoad {
    let ticket: LoadTicket
    let url: URL
    let blendSeconds: Float
    let continuation: CheckedContinuation<Void, Error>
}
