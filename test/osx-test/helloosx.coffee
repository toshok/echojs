foundation = require 'foundation'
appkit = require 'appkit'
objc = require 'objc'

class AppDelegate extends foundation.NSObject
      #didFinishLaunching: @override ->
      didFinishLaunching: @nativeSelector("applicationDidFinishLaunching:").impl ->
              # Insert code here to initialize your application
              @window.title = "Hello OSX, Love CoffeeKit"
              
              @button = appkit.NSButton.newWithFrame { x: 100, y: @window.contentView.bounds.height - 50, width: 200, height: 50 }
              @button.bezelStyle = appkit.NSBezelStyle.RoundedBezelStyle
              @button.title = "Click me... please?"
              @button.clicked = =>
                      @newbutton = appkit.NSButton.newWithFrame { x: 100, y: @window.contentView.bounds.height - 120, width: 200, height: 50 }
                      @newbutton.bezelStyle = appkit.NSBezelStyle.RoundedBezelStyle
                      @newbutton.title = "you clicked me!"
                      @window.contentView.addSubview @newbutton
                      console.log "yo!"

              @window.contentView.addSubview @button

      @outlet "window", appkit.NSWindow
      

new ck.ConformsToProtocolAttribute AppDelegate, appkit.NSApplicationDelegate
ck.register AppDelegate

appkit.NSApplication.main process.argv
