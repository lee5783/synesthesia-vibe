// swift-tools-version: 5.9
//
//  Package.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import PackageDescription

let package = Package(
    name: "SynesthesiaVibe",
    platforms: [
        .iOS(.v13),
        .macOS(.v12)
    ],
    products: [
        .library(name: "SynesthesiaVibe", targets: ["SynesthesiaVibe"])
    ],
    targets: [
        .binaryTarget(
            name: "SynesthesiaVibeCore",
            path: "../../build/xcframework/SynesthesiaVibeCore.xcframework"
        ),
        .target(
            name: "SynesthesiaVibe",
            dependencies: ["SynesthesiaVibeCore"],
            path: "swift/Sources/SynesthesiaVibe",
            linkerSettings: [
                .linkedFramework("UIKit", .when(platforms: [.iOS])),
                .linkedFramework("OpenGLES", .when(platforms: [.iOS])),
                .linkedFramework("AppKit", .when(platforms: [.macOS])),
                .linkedFramework("OpenGL", .when(platforms: [.macOS])),
                .linkedFramework("CoreAudio", .when(platforms: [.macOS])),
                .linkedFramework("CoreVideo", .when(platforms: [.macOS])),
                .linkedFramework("AVFoundation"),
                .linkedFramework("QuartzCore"),
                .linkedLibrary("c++")
            ]
        )
    ]
)
