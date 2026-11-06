//
//  AudioSource.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import Foundation

enum AudioSource: String, CaseIterable, Identifiable {

    case none
    case wav
    case microphone
    case system

    var id: Self { self }

    var label: String {
        switch self {
        case .none: return "None"
        case .wav: return "Bundled WAV"
        case .microphone: return "Microphone"
        case .system: return "System sound"
        }
    }
}
