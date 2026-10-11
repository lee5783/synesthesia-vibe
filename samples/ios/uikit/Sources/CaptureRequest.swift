//
//  CaptureRequest.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import Foundation

enum CaptureError: LocalizedError {
    case missingValue(String)
    case unknownPreset(String)
    case invalidFrames(String)
    case missingWav(String)
    case rendererUnavailable

    var errorDescription: String? {
        switch self {
        case .missingValue(let flag): "missing value for \(flag)"
        case .unknownPreset(let key): "no bundled preset matches \(key)"
        case .invalidFrames(let value): "invalid --frames list \(value)"
        case .missingWav(let path): "WAV not found at \(path)"
        case .rendererUnavailable: "could not create a current OpenGL ES 3.0 context"
        }
    }
}

struct CaptureRequest {

    static let standardFrames = [0, 30, 60, 150, 300, 450, 599]
    static let valueFlags: Set<String> = ["--capture", "--wav", "--frames", "--out"]

    let preset: Preset
    let wavURL: URL?
    let frames: [Int]
    let outputDirectoryURL: URL
    let resourceURL: URL

    init?(arguments: [String], library: PresetLibrary) throws {
        guard arguments.contains("--capture") else { return nil }
        let options = try Self.values(in: arguments)
        guard let presetKey = options["--capture"] else { throw CaptureError.missingValue("--capture") }
        guard let preset = library.capturePreset(matching: presetKey) else { throw CaptureError.unknownPreset(presetKey) }
        self.preset = preset
        resourceURL = library.resourceURL
        wavURL = try Self.wavURL(for: options["--wav"], library: library)
        frames = try options["--frames"].map(Self.frames(from:)) ?? Self.standardFrames
        outputDirectoryURL = Self.outputDirectoryURL(for: options["--out"] ?? "capture")
    }

    private static func values(in arguments: [String]) throws -> [String: String] {
        var values: [String: String] = [:]
        var index = arguments.startIndex
        while index < arguments.endIndex {
            let flag = arguments[index]
            index += 1
            guard valueFlags.contains(flag) else { continue }
            guard index < arguments.endIndex, !arguments[index].hasPrefix("--") else { throw CaptureError.missingValue(flag) }
            values[flag] = arguments[index]
            index += 1
        }
        return values
    }

    private static func frames(from list: String) throws -> [Int] {
        let frames = list.split(separator: ",").compactMap { Int($0.trimmingCharacters(in: .whitespaces)) }
        guard !frames.isEmpty, frames.count == list.split(separator: ",").count, frames.allSatisfy({ $0 >= 0 }) else {
            throw CaptureError.invalidFrames(list)
        }
        return frames.sorted()
    }

    private static func wavURL(for value: String?, library: PresetLibrary) throws -> URL? {
        guard let value else { return library.defaultWavURL }
        guard value != "none" else { return nil }
        let url = value.hasPrefix("/") ? URL(fileURLWithPath: value) : library.resourceURL.appendingPathComponent(value)
        guard FileManager.default.fileExists(atPath: url.path) else { throw CaptureError.missingWav(url.path) }
        return url
    }

    private static func outputDirectoryURL(for value: String) -> URL {
        guard !value.hasPrefix("/") else { return URL(fileURLWithPath: value, isDirectory: true) }
        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return documents.appendingPathComponent(value, isDirectory: true)
    }
}
