//
//  ElectronDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation
import AudioToolbox

class ElectronDocument: MachineDocument {

	private lazy var electron = CSElectron()

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)
		self.intendedCyclesPerSecond = 2000000
		aController.window?.contentAspectRatio = NSSize(width: 11.0, height: 10.0)
		openGLView.performWithGLContext({
			if let osPath = NSBundle.mainBundle().pathForResource("os", ofType: "rom") {
				self.electron.setOSROM(NSData(contentsOfFile: osPath)!)
			}
			if let basicPath = NSBundle.mainBundle().pathForResource("basic", ofType: "rom") {
				self.electron.setBASICROM(NSData(contentsOfFile: basicPath)!)
			}
			self.electron.setView(self.openGLView, aspectRatio: 11.0 / 10.0)
			self.electron.audioQueue = self.audioQueue
		})
		establishStoredOptions()
	}

	override var windowNibName: String? {
		return "ElectronDocument"
	}

	override func readFromURL(url: NSURL, ofType typeName: String) throws {
		print(url)
		print(typeName)

		if let pathExtension = url.pathExtension {
			switch pathExtension.lowercaseString {
				case "uef":
					electron.openUEFAtURL(url)
					return
				default: break;
			}
		}

		let fileWrapper = try NSFileWrapper(URL: url, options: NSFileWrapperReadingOptions(rawValue: 0))
		try self.readFromFileWrapper(fileWrapper, ofType: typeName)
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		if let plus1Path = NSBundle.mainBundle().pathForResource("plus1", ofType: "rom") {
			electron.setROM(NSData(contentsOfFile: plus1Path)!, slot: 12)
		}
		electron.setROM(data, slot: 15)
	}

	lazy var actionLock = NSLock()
	lazy var drawLock = NSLock()
	override func close() {
		actionLock.lock()
		drawLock.lock()
		openGLView.invalidate()
		openGLView.openGLContext!.makeCurrentContext()
		actionLock.unlock()
		drawLock.unlock()

		super.close()
	}

	// MARK: IBActions
	@IBOutlet var displayTypeButton: NSPopUpButton!
	@IBAction func setDisplayType(sender: NSPopUpButton!) {
		electron.useTelevisionOutput = (sender.indexOfSelectedItem == 1)
		NSUserDefaults.standardUserDefaults().setInteger(sender.indexOfSelectedItem, forKey: self.displayTypeUserDefaultsKey)
	}

	@IBOutlet var fastLoadingButton: NSButton!
	@IBAction func setFastLoading(sender: NSButton!) {
		electron.useFastLoadingHack = sender.state == NSOnState
		NSUserDefaults.standardUserDefaults().setBool(electron.useFastLoadingHack, forKey: self.fastLoadingUserDefaultsKey)
	}

	private let displayTypeUserDefaultsKey = "electron.displayType"
	private let fastLoadingUserDefaultsKey = "electron.fastLoading"
	private func establishStoredOptions() {
		let standardUserDefaults = NSUserDefaults.standardUserDefaults()
		standardUserDefaults.registerDefaults([
			displayTypeUserDefaultsKey: 0,
			fastLoadingUserDefaultsKey: true
		])

		let useFastLoadingHack = standardUserDefaults.boolForKey(self.fastLoadingUserDefaultsKey)
		electron.useFastLoadingHack = useFastLoadingHack
		self.fastLoadingButton.state = useFastLoadingHack ? NSOnState : NSOffState

		let displayType = standardUserDefaults.integerForKey(self.displayTypeUserDefaultsKey)
		electron.useTelevisionOutput = (displayType == 1)
		self.displayTypeButton.selectItemAtIndex(displayType)
	}

	// MARK: CSOpenGLViewDelegate
	override func runForNumberOfCycles(numberOfCycles: Int32) {
		if actionLock.tryLock() {
			electron.runForNumberOfCycles(numberOfCycles)
			actionLock.unlock()
		}
	}

	override func openGLView(view: CSOpenGLView, drawViewOnlyIfDirty onlyIfDirty: Bool) {
		if drawLock.tryLock() {
			electron.drawViewForPixelSize(view.backingSize, onlyIfDirty: onlyIfDirty)
			drawLock.unlock()
		}
	}

	// MARK: CSOpenGLViewResponderDelegate
	override func keyDown(event: NSEvent) {
		electron.setKey(event.keyCode, isPressed: true)
	}

	override func keyUp(event: NSEvent) {
		electron.setKey(event.keyCode, isPressed: false)
	}

	override func flagsChanged(newModifiers: NSEvent) {
		electron.setKey(VK_Shift, isPressed: newModifiers.modifierFlags.contains(.ShiftKeyMask))
		electron.setKey(VK_Control, isPressed: newModifiers.modifierFlags.contains(.ControlKeyMask))
		electron.setKey(VK_Command, isPressed: newModifiers.modifierFlags.contains(.CommandKeyMask))
		electron.setKey(VK_Option, isPressed: newModifiers.modifierFlags.contains(.AlternateKeyMask))
	}
}
