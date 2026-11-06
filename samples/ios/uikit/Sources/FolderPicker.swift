//
//  FolderPicker.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import UIKit
import UniformTypeIdentifiers

protocol FolderPickerDelegate: AnyObject {
    func folderPicker(_ controller: UIDocumentPickerViewController, didPickFolder url: URL)
}

enum FolderPicker {

    static func make(delegate: FolderPickerDelegate) -> UIDocumentPickerViewController {
        let picker: UIDocumentPickerViewController
        if #available(iOS 14.0, *) {
            picker = UIDocumentPickerViewController(forOpeningContentTypes: [.folder], asCopy: false)
        } else {
            picker = UIDocumentPickerViewController(documentTypes: ["public.folder"], in: .open)
        }
        picker.allowsMultipleSelection = false
        picker.delegate = FolderPickerCoordinator.shared
        FolderPickerCoordinator.shared.delegate = delegate
        return picker
    }
}

final class FolderPickerCoordinator: NSObject, UIDocumentPickerDelegate {

    static let shared = FolderPickerCoordinator()
    weak var delegate: FolderPickerDelegate?

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first else { return }
        delegate?.folderPicker(controller, didPickFolder: url)
    }
}
