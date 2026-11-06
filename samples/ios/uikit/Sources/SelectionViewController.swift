//
//  SelectionViewController.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import UIKit

final class SelectionViewController: UITableViewController {

    private let headerTitle: String
    private let options: [String]
    private let selectedIndex: Int
    private let onSelect: (Int) -> Void
    private let cellIdentifier = "SelectionCell"

    init(title: String, options: [String], selectedIndex: Int, onSelect: @escaping (Int) -> Void) {
        self.headerTitle = title
        self.options = options
        self.selectedIndex = selectedIndex
        self.onSelect = onSelect
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = headerTitle
        tableView.register(SettingsCell.self, forCellReuseIdentifier: cellIdentifier)
        tableView.backgroundColor = DesignTokens.backgroundSecondary
        tableView.tintColor = DesignTokens.accentText
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        options.count
    }

    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: cellIdentifier, for: indexPath)
        cell.textLabel?.text = options[indexPath.row]
        cell.accessoryType = indexPath.row == selectedIndex ? .checkmark : .none
        return cell
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        onSelect(indexPath.row)
        navigationController?.popViewController(animated: true)
    }
}
