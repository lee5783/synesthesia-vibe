//
//  CaptureRunner.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import Foundation
import OpenGLES
import SynesthesiaVibe

enum CaptureRunner {

    static func start(_ request: CaptureRequest) {
        let thread = Thread {
            let result = Result { try capture(request) }
            DispatchQueue.main.async { finish(with: result) }
        }
        thread.name = "SynesthesiaVibeCapture"
        thread.start()
    }

    static func finish(with result: Result<[URL], Error>) -> Never {
        switch result {
        case .success(let files):
            log("CAPTURE OK files=\(files.count) out=\(files.first?.deletingLastPathComponent().path ?? "-")")
            exit(EXIT_SUCCESS)
        case .failure(let error):
            log("CAPTURE FAILED \(error.localizedDescription)")
            exit(EXIT_FAILURE)
        }
    }

    private static func capture(_ request: CaptureRequest) throws -> [URL] {
        guard let glContext = EAGLContext(api: .openGLES3), EAGLContext.setCurrent(glContext) else {
            throw CaptureError.rendererUnavailable
        }
        defer { EAGLContext.setCurrent(nil) }
        return try autoreleasepool {
            let configuration = SVContextConfiguration()
            configuration.resourceURL = request.resourceURL
            let session = SVCaptureSession(
                configuration: configuration,
                presetURL: request.preset.url,
                wavURL: request.wavURL,
                frames: request.frames.map { NSNumber(value: $0) },
                outputDirectoryURL: request.outputDirectoryURL
            )
            log("CAPTURE START preset=\(request.preset.id ?? request.preset.name) frames=\(request.frames.map(String.init).joined(separator: ",")) out=\(request.outputDirectoryURL.path)")
            try session.run()
            return session.writtenFileURLs
        }
    }

    private static func log(_ line: String) {
        FileHandle.standardError.write(Data("SynesthesiaVibeSwiftUI: \(line)\n".utf8))
    }
}
