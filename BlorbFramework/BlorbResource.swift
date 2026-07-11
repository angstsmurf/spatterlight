//
//  Resource.swift
//  BlorbFramework
//
//  Created by C.W. Betts on 11/30/23.
//

import Cocoa

extension Blorb {
    @objcMembers @objc(BlorbResource) public class Resource : NSObject {
        public let usage: FourCharCode
        public let number: UInt32
        public let start: UInt32
        public var descriptiontext: String?
        public var chunkType: String?

        public init(usage: FourCharCode, number: UInt32, start: UInt32) {
            self.usage = usage
            self.number = number
            self.start = start
        }
    }
}
