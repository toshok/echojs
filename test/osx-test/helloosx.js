import { NSObject } from 'foundation';
import { NSButton, NSBezelStyle, NSWindow, NSApplicationDelegate, NSApplication } from 'appkit';
import { override, instanceSelector, outlet, sig } from 'objc';

let AppDelegate = NSObject.extendClass("AppDelegate", () => ({

    // we don't need to specify the selector here, since the
    // conforms-to-protocol machinery picks it up (based on field name)
    // and fills it in for us.
    didFinishLaunching: function() {
	let window = this.window;

	window.title = "Hello OSX, Love CoffeeKit";

	// newWith$Stuff = ctor + call initWith$Stuff
	let button        = NSButton.newWithFrame({ x: 100, y: window.contentView.bounds.height - 50,
						    width: 200, height: 50 });
	button.bezelStyle = NSBezelStyle.RoundedBezelStyle;
	button.title      = "Click me... please?";

	button.clicked = function () {
            let newbutton        = NSButton.newWithFrame({ x: 100, y: window.contentView.bounds.height - 120,
							   width: 200, height: 50 });
            newbutton.bezelStyle = NSBezelStyle.RoundedBezelStyle;
            newbutton.title      = "you clicked me!";
            window.contentView.addSubview(newbutton);

	    // need 'this.xmlhttp', not a local here, since XMLHttpRequest doesn't root itself
	    // while waiting for a response
            this.xmlhttp = new XMLHttpRequest();
            this.xmlhttp.onreadystatechange = () => {
		console.log ("readyState == " + this.xmlhttp.readyState);
		if (this.xmlhttp.readyState === 4) {
                    console.log ("woohoo!");
		    console.log (this.xmlhttp.responseText);
		}
            };
	    console.log (this.xmlhttp.onreadystatechange);
            this.xmlhttp.open('GET', 'http://www.google.com/', true);
            this.xmlhttp.send();
	};

	window.contentView.addSubview(button);
    },

    /* XXX we shouldn't need this (the outlet below should be enough), but without the selector
       explicitly registered the outlet doesn't get set for some reason... */
    setWindow: instanceSelector("setWindow:")
	       .returns(sig.Void)
               .params([ NSWindow ])
               .impl(function (v) { this.window = v; }),
    window: outlet (NSWindow)
}), [
    /* protocols this type conforms to */
    NSApplicationDelegate
]);

NSApplication.main(process.argv);
