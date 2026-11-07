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
    @Published public private(set) var presetInfo = SVPresetInfo()
    @Published public private(set) var presetFallbackReason = ""
    @Published public var isPaused = false {
        didSet { view?.isPaused = isPaused }
    }
    @Published public var preferredFramesPerSecond: Float {
        didSet { view?.preferredFramesPerSecond = preferredFramesPerSecond }
    }
    @Published public private(set) var lastError: Error?
    @Published public private(set) var levels = SVAudioLevels(bass: 0, mid: 0, treble: 0)
    @Published public private(set) var debugInfo: [String: Any]?
    @Published public var debugEnabled = false {
        didSet {
            view?.debugEnabled = debugEnabled
            if !debugEnabled { debugInfo = nil }
        }
    }
    @Published public var debugInterval: Float = 1 {
        didSet { view?.debugInterval = debugInterval }
    }

    public let configuration: SVContextConfiguration
    public private(set) weak var view: SVView?

    private weak var context: SVContext?
    private var pendingLoad: PendingLoad?
    private lazy var prepareWorker = PrepareWorker()
    private var wavFeeder: SVWavFeeder?
    private var wavFrameRemainder = 0.0
    private var audioInput: (any LiveAudioInput)?
    private var wantedSource: WantedSource = .none

    public init(configuration: SVContextConfiguration = SVContextConfiguration()) {
        self.configuration = configuration
        self.preferredFramesPerSecond = configuration.maxFps
        super.init()
    }

    deinit {
        prepareWorker.stop()
        pendingLoad?.continuation.resume(throwing: CancellationError())
        audioInput?.stop()
    }

    public static var isSystemAudioSupported: Bool { SVSystemAudioInput.isSupported }

    public func attach(to view: SVView) {
        guard self.view !== view else { return }
        if let previous = self.view {
            detach(from: previous)
        }
        view.configuration = configuration
        view.delegate = self
        view.isPaused = isPaused
        view.preferredFramesPerSecond = preferredFramesPerSecond
        view.debugInterval = debugInterval
        view.debugEnabled = debugEnabled
        view.debugInfoHandler = { [weak self] info in
            self?.debugInfo = info
        }
        self.view = view
        if let existing = view.context {
            context = existing
            guard wantedSource.isLiveInput, audioInput == nil else { return }
            do {
                try startAudioInput(with: existing)
            } catch {
                lastError = error
            }
        }
    }

    public func detach(from view: SVView) {
        guard self.view === view else { return }
        view.delegate = nil
        view.debugInfoHandler = nil
        self.view = nil
        context = nil
        prepareWorker.cancelPending()
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
        wantedSource = .wav
        wavFeeder = feeder
        wavFrameRemainder = 0
    }

    public func startMicrophone() throws {
        stopAudio()
        wantedSource = .microphone
        guard let context else { return }
        try startAudioInput(with: context)
    }

    public func startSystemAudio() throws {
        stopAudio()
        wantedSource = .systemAudio
        guard let context else { return }
        try startAudioInput(with: context)
    }

    public func stopAudio() {
        wavFeeder = nil
        wantedSource = .none
        audioInput?.stop()
        audioInput = nil
    }

    private func enqueue(_ load: PendingLoad) {
        guard !load.ticket.isCancelled else {
            load.continuation.resume(throwing: CancellationError())
            return
        }
        pendingLoad?.continuation.resume(throwing: CancellationError())
        pendingLoad = load
        if let context {
            startPreparing(load, with: context)
        }
    }

    private func cancel(_ ticket: LoadTicket) {
        ticket.isCancelled = true
        guard let load = pendingLoad, load.ticket === ticket else { return }
        pendingLoad = nil
        load.continuation.resume(throwing: CancellationError())
    }

    private func startPreparing(_ load: PendingLoad, with context: SVContext) {
        load.preparingContext = context
        prepareWorker.submit(url: load.url, context: context) { [weak self] result in
            DispatchQueue.main.async {
                guard let self, self.pendingLoad === load else { return }
                load.prepareResult = result
            }
        }
    }

    private func performPendingLoads(with context: SVContext) {
        guard let load = pendingLoad else { return }
        guard load.preparingContext === context else {
            startPreparing(load, with: context)
            return
        }
        guard let result = load.prepareResult else { return }
        pendingLoad = nil
        do {
            let prepared = try result.get()
            try context.commitPreset(prepared, blendSeconds: load.blendSeconds)
            presetName = load.url.deletingPathExtension().lastPathComponent
            presetInfo = context.presetInfo
            presetFallbackReason = context.presetFallbackReason
            lastError = nil
            load.continuation.resume()
        } catch {
            lastError = error
            load.continuation.resume(throwing: error)
        }
    }

    private func failPendingLoads(with error: Error) {
        guard let load = pendingLoad else { return }
        pendingLoad = nil
        load.continuation.resume(throwing: error)
    }

    private func startAudioInput(with context: SVContext) throws {
        switch wantedSource {
        case .microphone:
            let input = SVAudioInput(context: context)
            try input.start()
            audioInput = input
        case .systemAudio:
            guard let input = SVSystemAudioInput(context: context) else {
                throw NSError(domain: SVErrorDomain,
                              code: SVError.Code.argument.rawValue,
                              userInfo: [NSLocalizedDescriptionKey: "system audio capture needs macOS 14.2 or later"])
            }
            try input.start()
            audioInput = input
        case .wav, .none:
            break
        }
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
        guard wantedSource.isLiveInput, audioInput == nil else { return }
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

private protocol LiveAudioInput: AnyObject {
    func stop()
}

extension SVAudioInput: LiveAudioInput {}
extension SVSystemAudioInput: LiveAudioInput {}

private enum WantedSource {
    case none
    case wav
    case microphone
    case systemAudio

    var isLiveInput: Bool {
        switch self {
        case .microphone, .systemAudio:
            return true
        case .wav, .none:
            return false
        }
    }
}

private final class LoadTicket: @unchecked Sendable {
    var isCancelled = false
}

private final class PendingLoad {
    let ticket: LoadTicket
    let url: URL
    let blendSeconds: Float
    let continuation: CheckedContinuation<Void, Error>
    weak var preparingContext: SVContext?
    var prepareResult: Result<SVPreparedPreset, Error>?

    init(ticket: LoadTicket, url: URL, blendSeconds: Float, continuation: CheckedContinuation<Void, Error>) {
        self.ticket = ticket
        self.url = url
        self.blendSeconds = blendSeconds
        self.continuation = continuation
    }
}

private final class PrepareWorker: @unchecked Sendable {
    private struct Request {
        let url: URL
        weak var context: SVContext?
        let completion: (Result<SVPreparedPreset, Error>) -> Void
    }

    private static let stackBytes = 4 * 1024 * 1024

    private let condition = NSCondition()
    private var request: Request?
    private var isStopped = false
    private var thread: Thread?

    func submit(url: URL, context: SVContext, completion: @escaping (Result<SVPreparedPreset, Error>) -> Void) {
        condition.lock()
        defer { condition.unlock() }
        guard !isStopped else { return }
        request = Request(url: url, context: context, completion: completion)
        if thread == nil {
            let worker = Thread { [self] in run() }
            worker.name = "synesthesia-prepare"
            worker.stackSize = Self.stackBytes
            worker.qualityOfService = .userInitiated
            thread = worker
            worker.start()
        }
        condition.signal()
    }

    func cancelPending() {
        condition.lock()
        request = nil
        condition.unlock()
    }

    func stop() {
        condition.lock()
        isStopped = true
        request = nil
        condition.signal()
        condition.unlock()
    }

    private func run() {
        while let next = takeRequest() {
            guard let context = next.context else { continue }
            next.completion(Result { try context.preparePreset(at: next.url) })
        }
    }

    private func takeRequest() -> Request? {
        condition.lock()
        defer { condition.unlock() }
        while request == nil && !isStopped {
            condition.wait()
        }
        guard !isStopped else { return nil }
        let next = request
        request = nil
        return next
    }
}
