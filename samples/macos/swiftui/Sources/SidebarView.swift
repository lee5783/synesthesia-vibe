//
//  SidebarView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SwiftUI
import UniformTypeIdentifiers

struct SidebarView: View {

    let library: PresetLibrary
    let onFolderPicked: (URL) -> Void
    @Binding var selection: String?
    @State private var isChoosingFolder = false

    var body: some View {
        VStack(spacing: 0) {
            header
            presetList
            footer
        }
        .background(Color.svBackgroundSecondary)
        .fileImporter(isPresented: $isChoosingFolder, allowedContentTypes: [.folder]) { result in
            guard case .success(let url) = result else { return }
            onFolderPicked(url)
        }
    }

    private var header: some View {
        HStack(spacing: 6) {
            Text("\(library.folderName) · \(library.presets.count)")
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(Color.svTextMuted)
            Spacer(minLength: 0)
            Button {
                isChoosingFolder = true
            } label: {
                Image(systemName: "folder.badge.plus")
                    .font(.system(size: 15))
                    .foregroundStyle(Color.svTextSecondary)
                    .frame(width: 24, height: 22)
            }
            .buttonStyle(.plain)
            .help("Choose a presets folder")
            .accessibilityLabel("Choose presets folder")
        }
        .padding(.leading, 18)
        .padding(.trailing, 12)
        .padding(.vertical, 6)
    }

    private var presetList: some View {
        List(selection: $selection) {
            ForEach(library.presets, id: \.name) { preset in
                row(for: preset)
            }
        }
        .listStyle(.sidebar)
        .scrollContentBackground(.hidden)
        .background(Color.svBackgroundSecondary)
        .accessibilityIdentifier("preset-list")
    }

    private func row(for preset: Preset) -> some View {
        HStack(spacing: 8) {
            Image(systemName: "waveform")
                .font(.system(size: 14))
                .foregroundStyle(isCurrent(preset) ? Color.svTextOnAccent : Color.svTextMuted)
            Text(preset.name)
                .font(.system(size: 13))
                .lineLimit(1)
                .truncationMode(.tail)
                .foregroundStyle(isCurrent(preset) ? Color.svTextOnAccent : Color.svTextPrimary)
            Spacer(minLength: 0)
        }
        .frame(height: 28)
        .padding(.horizontal, 8)
        .background(isCurrent(preset) ? Color.svAccentPrimary : Color.clear)
        .clipShape(RoundedRectangle(cornerRadius: 6, style: .continuous))
        .tag(preset.name)
        .accessibilityAddTraits(isCurrent(preset) ? [.isSelected] : [])
    }

    private func isCurrent(_ preset: Preset) -> Bool {
        selection == preset.name
    }

    private var footer: some View {
        HStack(spacing: 6) {
            Image(systemName: "folder")
                .font(.system(size: 12))
                .foregroundStyle(Color.svInfo)
            Text(library.folderPath)
                .font(.system(size: 11))
                .lineLimit(1)
                .truncationMode(.head)
                .foregroundStyle(Color.svTextMuted)
            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 16)
        .overlay(alignment: .top) {
            Rectangle().fill(Color.svBorderSubtle).frame(height: 1)
        }
    }
}
