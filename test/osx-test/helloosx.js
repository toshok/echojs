let foundation = require('foundation'),
    appkit = require('appkit'),
    objc = require( 'objc');

let AppDelegate = foundation.NSObject.extendClass("AppDelegate", {

    // we don't need to specify the selector here, since the
    // conforms-to-protocol machinery picks it up (based on field name)
    // and fills it in for us.
    didFinishLaunching: function() {
      let window = this.window;

      window.title = "Hello OSX, Love CoffeeKit";

      // newWith$Stuff = ctor + call initWith$Stuff
      let button = button = appkit.NSButton.newWithFrame({ x: 100, y: window.contentView.bounds.height - 50, width: 200, height: 50 });
      button.bezelStyle = appkit.NSBezelStyle.RoundedBezelStyle;
      button.title = "Click me... please?";

      button.clicked = function () {
        let newbutton = appkit.NSButton.newWithFrame({ x: 100, y: window.contentView.bounds.height - 120, width: 200, height: 50 });
        newbutton.bezelStyle = appkit.NSBezelStyle.RoundedBezelStyle;
        newbutton.title = "you clicked me!";
        window.contentView.addSubview(newbutton);

        this.xmlhttp = new XMLHttpRequest();
	let that = this;
        this.xmlhttp.onreadystatechange = function() {
	    console.log ("readyState == " + that.xmlhttp.readyState);
            if (that.xmlhttp.readyState === 4){
                console.log ("woohoo!");
		console.log (that.xmlhttp.responseText);
            }
        };
	console.log (this.xmlhttp.onreadystatechange);
        this.xmlhttp.open('GET', 'http://www.google.com/', true);
        this.xmlhttp.send();
      };

      window.contentView.addSubview(button);
    },

    // we don't need the selector here either.  objc.override walks the
    // prototype chain at registration time to find the method we're overriding,
    // and uses its selector/type signature.
    //dealloc: objc.override (function () {
			    //}),

    /* XXX we shouldn't need this, but without the selector registered the outlet doesn't get set for some reason... */
    setWindow: objc.instanceSelector("setWindow:").returns(objc.sig.Void)
                                                  .params([ appkit.NSWindow ])
                                                  .impl(function (v) {
                                                     this.window = v;
                                                  }),
    window: objc.outlet (appkit.NSWindow)
  }, [
  /* protocols this type conforms to */
  appkit.NSApplicationDelegate
]);

appkit.NSApplication.main(process.argv);
