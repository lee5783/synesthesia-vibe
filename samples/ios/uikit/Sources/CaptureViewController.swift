//
//  CaptureViewController.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import OpenGLES
import SynesthesiaVibe
import UIKit

final class CaptureViewController: UIViewController {

    private let request: CaptureRequest
    private let statusLabel = UILabel()

    init(request: CaptureRequest) {
        self.request = request
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        statusLabel.textColor = .white
        statusLabel.numberOfLines = 0
        statusLabel.textAlignment = .center
        statusLabel.text = "Capturing \(request.preset.name)\n\(request.frames.count) frames"
        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(statusLabel)
        NSLayoutConstraint.activate([
            statusLabel.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            statusLabel.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            statusLabel.widthAnchor.constraint(lessThanOrEqualTo: view.widthAnchor, constant: -32)
        ])
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        let request = self.request
        let thread = Thread {
            let result = Result { try Self.capture(request) }
            DispatchQueue.main.async { Self.finish(with: result) }
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
        FileHandle.standardError.write(Data("SynesthesiaVibeUIKit: \(line)\n".utf8))
    }
}
