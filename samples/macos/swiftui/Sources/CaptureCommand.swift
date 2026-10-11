//
//  CaptureCommand.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import AppKit
import SynesthesiaVibe

enum CaptureCommand {

    static let option = "--capture"
    static let standardFrames = "0,30,60,150,300,450,599"

    static func run(arguments: [String], library: PresetLibrary) -> Int32 {
        guard let presetKey = value(for: option, in: arguments), let outPath = value(for: "--out", in: arguments) else {
            log("usage: SynesthesiaVibeMacSwiftUI --capture <preset-id> [--wav <path>] [--frames \(standardFrames)] --out <dir>")
            return 2
        }
        guard let preset = library.capturePreset(matching: presetKey) else {
            log("unknown preset '\(presetKey)'")
            return 2
        }
        let framesText = value(for: "--frames", in: arguments) ?? standardFrames
        guard let frames = frames(from: framesText) else {
            log("invalid --frames '\(framesText)'")
            return 2
        }
        let wavURL = value(for: "--wav", in: arguments).map { URL(fileURLWithPath: $0) } ?? library.defaultWavURL
        let outURL = URL(fileURLWithPath: outPath, isDirectory: true)

        let attributes: [NSOpenGLPixelFormatAttribute] = [
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAOpenGLProfile), NSOpenGLPixelFormatAttribute(NSOpenGLProfileVersion3_2Core),
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAColorSize), 24,
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAAlphaSize), 8,
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAAccelerated),
            0
        ]
        guard let pixelFormat = NSOpenGLPixelFormat(attributes: attributes),
              let glContext = NSOpenGLContext(format: pixelFormat, share: nil) else {
            log("could not create an OpenGL 3.2 Core context")
            return 1
        }
        glContext.makeCurrentContext()
        defer { NSOpenGLContext.clearCurrentContext() }

        return autoreleasepool {
            let configuration = SVContextConfiguration()
            configuration.resourceURL = library.resourceURL
            configuration.cacheURL = URL(fileURLWithPath: NSTemporaryDirectory(), isDirectory: true)
            let session = SVCaptureSession(
                configuration: configuration,
                presetURL: preset.url,
                wavURL: wavURL,
                frames: frames.map { NSNumber(value: $0) },
                outputDirectoryURL: outURL
            )
            log("capture preset=\(presetKey) file=\"\(preset.url.lastPathComponent)\" wav=\"\(wavURL?.path ?? "none")\" frames=\(frames.map(String.init).joined(separator: ",")) out=\"\(outURL.path)\"")
            do {
                try session.run()
            } catch {
                session.writtenFileURLs.forEach { log("wrote \($0.path)") }
                log("capture failed: \(error.localizedDescription)")
                return 1
            }
            session.writtenFileURLs.forEach { log("wrote \($0.path)") }
            log("capture OK files=\(session.writtenFileURLs.count)")
            return 0
        }
    }

    private static func value(for option: String, in arguments: [String]) -> String? {
        guard let index = arguments.firstIndex(of: option), index + 1 < arguments.endIndex else { return nil }
        return arguments[index + 1]
    }

    private static func frames(from text: String) -> [Int]? {
        let components = text.split(separator: ",", omittingEmptySubsequences: false)
        let frames = components.compactMap { Int($0.trimmingCharacters(in: .whitespaces)) }
        guard !frames.isEmpty, frames.count == components.count, frames.allSatisfy({ $0 >= 0 }) else { return nil }
        return frames
    }

    private static func log(_ line: String) {
        FileHandle.standardError.write(Data("SynesthesiaVibeMacSwiftUI: \(line)\n".utf8))
    }
}
