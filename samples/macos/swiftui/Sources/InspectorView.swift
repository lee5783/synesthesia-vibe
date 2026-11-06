//
//  InspectorView.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import SwiftUI

struct InspectorView: View {

    @Binding var isAutoChangeOn: Bool
    @Binding var autoChangeInterval: Double
    @Binding var audioSource: AudioSource
    @Binding var frameRate: Float
    @Binding var debugOverlayOn: Bool

    let systemAudioSupported: Bool

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                playlistGroup
                audioGroup
                playbackGroup
                developerGroup
            }
            .padding(16)
        }
        .background(Color.svBackgroundPrimary)
    }

    private var playlistGroup: some View {
        group("Playlist") {
            card {
                row {
                    Text("Auto-change")
                        .foregroundStyle(Color.svTextPrimary)
                    Spacer(minLength: 8)
                    Toggle("", isOn: $isAutoChangeOn)
                        .labelsHidden()
                        .toggleStyle(.switch)
                        .tint(Color.svAccentPrimary)
                        .accessibilityLabel("Auto-change")
                        .accessibilityIdentifier("auto-change-toggle")
                }
                divider
                row {
                    Text("Every")
                        .foregroundStyle(isAutoChangeOn ? Color.svTextPrimary : Color.svTextDisabled)
                    Spacer(minLength: 8)
                    Picker("Every", selection: $autoChangeInterval) {
                        Text("30 seconds").tag(30.0)
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                    .fixedSize()
                    .disabled(!isAutoChangeOn)
                    .accessibilityIdentifier("auto-change-interval")
                }
            }
            footer("Click a preset in the sidebar to play it.")
        }
    }

    private var audioGroup: some View {
        group("Audio") {
            card {
                row(alignment: .top) {
                    Text("Source")
                        .foregroundStyle(Color.svTextPrimary)
                        .padding(.top, 2)
                    Spacer(minLength: 8)
                    Picker("Source", selection: $audioSource) {
                        Text("None").tag(AudioSource.none)
                        Text("Bundled WAV").tag(AudioSource.wav)
                        Text("Microphone").tag(AudioSource.microphone)
                        Text("System sound").tag(AudioSource.system)
                    }
                    .labelsHidden()
                    .pickerStyle(.radioGroup)
                    .fixedSize()
                    .accessibilityIdentifier("audio-source-picker")
                }
            }
            footer(audioFooter)
        }
    }

    private var playbackGroup: some View {
        group("Playback") {
            card {
                row {
                    Text("Frame rate")
                        .foregroundStyle(Color.svTextPrimary)
                    Spacer(minLength: 8)
                    Picker("Frame rate", selection: $frameRate) {
                        ForEach(frameRateOptions, id: \.self) { rate in
                            Text(frameRateLabel(rate)).tag(rate)
                        }
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                    .fixedSize()
                    .accessibilityIdentifier("frame-rate-picker")
                }
                divider
                row {
                    Text("Transition")
                        .foregroundStyle(Color.svTextPrimary)
                    Spacer(minLength: 8)
                    Picker("Transition", selection: .constant("Blend")) {
                        Text("Blend").tag("Blend")
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                    .fixedSize()
                    .accessibilityIdentifier("transition-picker")
                }
            }
            footer("Frame rate applies live through preferredFramesPerSecond.")
        }
    }

    private var developerGroup: some View {
        group("Developer") {
            card {
                row {
                    Text("Debug overlay")
                        .foregroundStyle(Color.svTextPrimary)
                    Spacer(minLength: 8)
                    Toggle("", isOn: $debugOverlayOn)
                        .labelsHidden()
                        .toggleStyle(.switch)
                        .tint(Color.svAccentPrimary)
                        .accessibilityLabel("Debug overlay")
                        .accessibilityIdentifier("debug-overlay-toggle")
                }
            }
            footer("Stays on in full screen. ⇧⌘D toggles it.")
        }
    }

    private var audioFooter: String {
        systemAudioSupported
            ? "System sound excludes this app’s own audio."
            : "System sound needs macOS 14.2 or later."
    }

    private var frameRateOptions: [Float] {
        let options: [Float] = [30, 60, 120, 0]
        return options.contains(frameRate) ? options : options + [frameRate]
    }

    private func frameRateLabel(_ rate: Float) -> String {
        rate > 0 ? String(format: "%.0f fps", rate) : "Uncapped"
    }

    private func group<Content: View>(_ title: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(Color.svTextMuted)
                .padding(.horizontal, 8)
            content()
        }
    }

    private func card<Content: View>(@ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            content()
        }
        .background(RoundedRectangle(cornerRadius: 8, style: .continuous).fill(Color.svSurfaceCard))
        .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).stroke(Color.svBorderSubtle, lineWidth: 1))
    }

    private func row<Content: View>(alignment: VerticalAlignment = .center, @ViewBuilder content: () -> Content) -> some View {
        HStack(alignment: alignment, spacing: 8) {
            content()
        }
        .font(.system(size: 13))
        .padding(.vertical, 10)
        .padding(.horizontal, 12)
    }

    private func footer(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(Color.svTextMuted)
            .padding(.horizontal, 8)
    }

    private var divider: some View {
        Rectangle()
            .fill(Color.svBorderSubtle)
            .frame(height: 1)
    }
}
