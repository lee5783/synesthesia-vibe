//
//  PresetLibrary.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import Foundation

struct Preset: Hashable {
    let id: String?
    let url: URL

    var name: String {
        url.deletingPathExtension().lastPathComponent
    }
}

struct PresetLibrary {

    let resourceURL: URL
    let presets: [Preset]
    let capturePresets: [Preset]
    let defaultWavURL: URL?

    init(bundle: Bundle) {
        resourceURL = bundle.resourceURL ?? bundle.bundleURL
        let ids = Self.captureSetIDs(at: resourceURL.appendingPathComponent("capture-set.json"))
        presets = Self.presets(in: resourceURL.appendingPathComponent("presets", isDirectory: true), ids: [:])
        capturePresets = Self.presets(in: resourceURL.appendingPathComponent("capture-presets", isDirectory: true), ids: ids)
        let wavURL = resourceURL.appendingPathComponent("audio/deterministic-test.wav")
        defaultWavURL = FileManager.default.fileExists(atPath: wavURL.path) ? wavURL : nil
    }

    func capturePreset(matching key: String) -> Preset? {
        if let preset = (capturePresets + presets).first(where: { $0.id == key || $0.url.lastPathComponent == key || $0.name == key }) {
            return preset
        }
        guard FileManager.default.fileExists(atPath: key) else { return nil }
        return Preset(id: nil, url: URL(fileURLWithPath: key))
    }

    private static func presets(in directory: URL, ids: [String: String]) -> [Preset] {
        let files = (try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)) ?? []
        return files
            .filter { $0.pathExtension == "milk" }
            .sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
            .map { Preset(id: ids[$0.lastPathComponent], url: $0) }
    }

    private static func captureSetIDs(at url: URL) -> [String: String] {
        guard let data = try? Data(contentsOf: url),
              let captureSet = try? JSONDecoder().decode(CaptureSet.self, from: data) else { return [:] }
        return Dictionary(captureSet.presets.map { ($0.file, $0.id) }, uniquingKeysWith: { first, _ in first })
    }
}

private struct CaptureSet: Decodable {
    struct Entry: Decodable {
        let id: String
        let file: String
    }

    let presets: [Entry]
}
