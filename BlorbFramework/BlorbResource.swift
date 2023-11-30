//
//  Resource.swift
//  BlorbFramework
//
//  Created by C.W. Betts on 11/30/23.
//

import Cocoa

extension Blorb {
    @objcMembers @objc(BlorbResource) public class Resource : NSObject {
        let usage: FourCharCode
        let number: UInt32
        let start: UInt32
        var descriptiontext: String?
        var chunkType: String?
        
        init(usage: FourCharCode, number: UInt32, start: UInt32) {
            self.usage = usage
            self.number = number
            self.start = start
        }
    }
}
