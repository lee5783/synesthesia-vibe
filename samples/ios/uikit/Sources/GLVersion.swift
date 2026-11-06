//
//  GLVersion.swift
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

import Foundation
import OpenGLES

enum GLVersionReader {

    static func read() -> String {
        guard let raw = glGetString(GLenum(GL_VERSION)) else { return "" }
        return String(cString: UnsafeRawPointer(raw).assumingMemoryBound(to: CChar.self))
    }
}
