//
//  SceneDelegate.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import SynesthesiaVibe
import UIKit

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        guard let windowScene = scene as? UIWindowScene else { return }
        let window = UIWindow(windowScene: windowScene)
        window.overrideUserInterfaceStyle = .dark
        window.tintColor = DesignTokens.accentText
        window.rootViewController = makeRootViewController(library: PresetLibrary(bundle: .main))
        window.makeKeyAndVisible()
        self.window = window
    }

    private func makeRootViewController(library: PresetLibrary) -> UIViewController {
        do {
            guard let request = try CaptureRequest(arguments: ProcessInfo.processInfo.arguments, library: library) else {
                return makeVisualizerNavigationController(library: library)
            }
            return CaptureViewController(request: request)
        } catch {
            CaptureViewController.finish(with: .failure(error))
        }
    }

    private func makeVisualizerNavigationController(library: PresetLibrary) -> UIViewController {
        let maxFps = Self.launchMaxFps(arguments: ProcessInfo.processInfo.arguments) ?? 60
        let player = SynesthesiaPlayer(configuration: Self.configuration(library: library, maxFps: maxFps))
        let session = VisualizerSession(
            library: library,
            player: player,
            folderName: VisualizerSession.bundledFolderName,
            fps: maxFps
        )
        let controller = VisualizerViewController(session: session)
        let navigation = UINavigationController(rootViewController: controller)
        controller.attachPlayer()
        return navigation
    }

    private static func configuration(library: PresetLibrary, maxFps: Float) -> SVContextConfiguration {
        let configuration = SVContextConfiguration()
        configuration.resourceURL = library.resourceURL
        configuration.maxFps = maxFps
        return configuration
    }

    private static func launchMaxFps(arguments: [String]) -> Float? {
        guard let index = arguments.firstIndex(of: "--max-fps"), index + 1 < arguments.endIndex else { return nil }
        return Float(arguments[index + 1]) ?? 0
    }
}
