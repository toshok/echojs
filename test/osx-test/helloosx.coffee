foundation = require 'foundation'
appkit = require 'appkit'
ck = require 'coffeekit'
objc = require 'objc'

ck.register class AppDelegate extends foundation.NSObject
        
        didFinishLaunching: @override ->
                # Insert code here to initialize your application
                @window.title = "Hello OSX, Love CoffeeKit"

                @button = appkit.NSButton.newWithFrame { x: 100, y: 200, width: 200, height: 50 }
                @button.bezelStyle = appkit.NSBezelStyle.RoundedBezelStyle
                @button.title = "Click me... please?"
                @button.clicked = -> console.log "yo!"
                
                @window.contentView.addSubview @button
                
        @outlet "window", appkit.NSWindow
        @conformsToProtocol appkit.NSApplicationDelegate

objc.NSApplicationMain process.argv
