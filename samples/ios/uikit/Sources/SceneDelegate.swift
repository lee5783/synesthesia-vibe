//
//  SceneDelegate.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import UIKit

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        guard let windowScene = scene as? UIWindowScene else { return }
        let window = UIWindow(windowScene: windowScene)
        window.rootViewController = makeRootViewController(library: PresetLibrary(bundle: .main))
        window.makeKeyAndVisible()
        self.window = window
    }

    private func makeRootViewController(library: PresetLibrary) -> UIViewController {
        do {
            guard let request = try CaptureRequest(arguments: ProcessInfo.processInfo.arguments, library: library) else {
                return UINavigationController(rootViewController: VisualizerViewController(library: library))
            }
            return CaptureViewController(request: request)
        } catch {
            CaptureViewController.finish(with: .failure(error))
        }
    }
}
