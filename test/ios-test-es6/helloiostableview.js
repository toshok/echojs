console.log ("inside helloiostableview.js");

import { NSBundle, NSObject, NSRect } from 'foundation';
import { chainCtor, instanceSelector, sig } from 'objc';
import { GLKViewController, GLKCanvasView, GLKViewDrawableDepthFormat } from 'glkit';

import * as ui from 'uikit';

let HelloIOSViewController = ui.UIViewController.extendClass("HelloIOSViewController", {});

let GLKCanvasViewController = GLKViewController.extendClass("GLKCanvasViewController", () => ({

    constructor: function (handle) {
      chainCtor(GLKCanvasViewController, this, arguments);
      if (!handle)
        this.initWithNibNameAndBundle(null, null);
    },

    loadView: instanceSelector().returns(() => sig.Void).impl(function() {
                this.view = new GLKCanvasView().initWithFrame(ui.UIScreen.mainScreen.bounds);
                this.view.drawableDepthFormat = GLKViewDrawableDepthFormat.depth16;
              })
}));

let TargetActionProxy1 = NSObject.extendClass("TargetActionProxy1", () => ({
    constructor: function(fn) {
      chainCtor(TargetActionProxy1, this);
      this.fn = fn;
    },

    proxyAction: instanceSelector("action").returns(() => sig.Void).impl (function (a1) { return this.fn(a1); })
}));

let HelloIOSAppDelegate = NSObject.extendClass("HelloIOSAppDelegate", () => ({

    constructor: function () {
      console.log ("made it to HelloIOSAppDelegate.ctor");
      chainCtor(HelloIOSAppDelegate, this, arguments);
    },

    runGLDemo: function(demoName, demoPath) {
      function uiAlertOnException(f) {
        try {
          f();
          return true;
        } catch (e) {
          new ui.UIAlertView().init("Exception in demo script", e, null, "Ok", null).show();
          return false;
        }
      };

      let demo;

      let glkcontroller = new GLKCanvasViewController;
      let canvas = glkcontroller.view;

      let demoValid = uiAlertOnException(() => demo = require(demoPath));
      if (demoValid) {

	glkcontroller.delegate = {
	  update: typeof(demo.update) === "function" ? demo.update : () => void 0
	};

	canvas.delegate = {
	  drawInRect: demo.draw
	};

	if (demo.tap) {
	  let tapProxy = new TargetActionProxy1(demo.tap);
          canvas.addGestureRecognizer(new ui.UITapGestureRecognizer().initWithTarget(tapProxy, tapProxy.proxyAction));
	}

	demo.run(canvas);
      }

      glkcontroller.title = "" + demoName;
      this.window.rootViewController.pushViewController(glkcontroller, true);
    },

    runJ3DDemo: function(demoName) {
      // XXX this should be this.runGLDemo(demoName, "j3d/Hello" + demoName);
      this.runGLDemo(demoName, "Hello" + demoName);
    },

    runMDNDemo: function(demoName) {
      this.runGLDemo(demoName, "webgl-samples/" + demoName + "/sample");
    },

    workerDemo: function() {
      let screenBounds = ui.UIScreen.mainScreen.bounds;
      let newcontroller = new HelloIOSViewController("HelloIOSViewController", null);
      let primeButton = ui.UIButton.buttonWithType(ui.UIButtonType.roundedRect);
      primeButton.title = "Click to generate primes";
      primeButton.frame = new NSRect(screenBounds.width / 2 - 100, screenBounds.height / 2 - 50, 200, 50);

      let primeTextField = new ui.UITextField().initWithFrame(new NSRect(0, screenBounds.height / 2 + 50, screenBounds.width, 50));
      primeTextField.textAlignment = ui.UITextAlignment.center;

      let primeCount = 0;

      let worker;

      primeButton.clicked = () => {
        if (worker) {
          primeButton = "Click to generate primes";
          primeTextField.text = "";
          worker.close();
          worker = null;
        } else {
          primeButton = "Click to stop";
/*
          worker = new Worker('./prime-worker');
          worker.onmessage = (msg) => {
            primeTextField.text = "Prime " + (++primeCount) + ": " + msg;
          };
*/
        }
      };

      newcontroller.title = "Web Workers";
      newcontroller.view.addSubviews(primeButton, primeTextField);
      this.window.rootViewController.pushViewController(newcontroller, true);
    },

    xhrDemo: function() {
      let screenBounds = ui.UIScreen.mainScreen.bounds;
      let newcontroller = new ui.UIViewController();
      newcontroller.view.backgroundColor = ui.UIColor.whiteColor;
      newcontroller.title = "XMLHttpRequest";

      let xhrButton = ui.UIButton.buttonWithType(ui.UIButtonType.roundedRect);
      xhrButton.title = "Click to fetch home.mcom.com";
      xhrButton.frame = new NSRect(screenBounds.width / 2 - 100, screenBounds.height / 2 - 50, 250, 50);

      let xhrTextView = new ui.UITextView().initWithFrame(new NSRect(0, screenBounds.height / 2 + 50, screenBounds.width, screenBounds.height - (screenBounds.height / 2 + 50)), null);
      xhrTextView.textAlignment = ui.UITextAlignment.center;

      xhrButton.clicked = () => {
	console.log ("clicked!");
        let xhr = new XMLHttpRequest;
        xhr.open("GET", "http://home.mcom.com/");
        xhr.onreadystatechange = () => {
	  console.log ("onreadystatechange!");
          if (xhr.readyState === 4) {
	      console.log ("response = " + xhr.responseText);
            xhrTextView.text = "response = " + xhr.responseText;
	  }
        };
	  console.log ("sending!");
        xhr.send();
      };
      newcontroller.view.addSubview(xhrButton);
      newcontroller.view.addSubview(xhrTextView);
      this.window.rootViewController.pushViewController(newcontroller, true);
    },

    didFinishLaunching: instanceSelector("application:didFinishLaunchingWithOptions:").impl(function() {
      console.log ("inside HelloIOAppDelegate.didFinishLaunching");
      this.window = new ui.UIWindow().initWithFrame(ui.UIScreen.mainScreen.bounds);

      let tableviewcontroller = new ui.UITableViewController().initWithStyle(ui.UITableViewStyle.plain);
      tableviewcontroller.title = "Pirouette Demos";

      let tableData = [ { title: "Web Workers",
                           rows: [ { title: "Primes in a button",  clicked: () => this.workerDemo() } ] },
                        { title: "XMLHttpRequest",
                           rows: [ { title: "Simple fetch",        clicked: () => this.xhrDemo() } ] },
                        { title: "WebGL",
                           rows: [ { title: "MDN sample 2", 	   clicked: () => this.runMDNDemo("sample2") },
                                   { title: "MDN sample 3", 	   clicked: () => this.runMDNDemo("sample3") },
                                   { title: "MDN sample 4", 	   clicked: () => this.runMDNDemo("sample4") },
                                   { title: "MDN sample 5", 	   clicked: () => this.runMDNDemo("sample5") },
                                   { title: "J3D Engine: Cube",    clicked: () => this.runJ3DDemo("Cube") },
                                   { title: "J3D Engine: Lights",  clicked: () => this.runJ3DDemo("Lights") },
                                   { title: "J3D Engine: Scene",   clicked: () => this.runJ3DDemo("Scene") },
                                   { title: "J3D Engine: Head",    clicked: () => this.runJ3DDemo("Head") },
                                   { title: "J3D Engine: Cubemap", clicked: () => this.runJ3DDemo("Cubemap") },
                                   { title: "J3D Engine: Plasma",  clicked: () => this.runJ3DDemo("Plasma") } ] }
                      ];

      let pathToItem = (arr, pos, path) => {
        let item = arr[path.indexAtPosition(pos)];
        if (pos === (path.length - 1)) {
          return item;
        } else {
          return pathToItem(item.rows, pos + 1, path);
        }
      };
      tableviewcontroller.tableView.delegate = {
        didSelectRow:               (tv, path) => pathToItem(tableData, 0, path).clicked()
      };
      tableviewcontroller.tableView.dataSource = {
        numberOfSections:                   () => tableData.length,
        numberOfRowsInSection:   (tv, section) => tableData[section].rows.length,
        titleForHeaderInSection: (tv, section) => tableData[section].title,
        cellForRow:                 (tv, path) => {
          let cell = tv.dequeueReusableCellWithIdentifier("reuse") || new ui.UITableViewCell().initWithStyle(ui.UITableViewCellStyle.value1, "reuse");
          cell.textLabel.text = pathToItem(tableData, 0, path).title;
          return cell;
        }
      };

      let navController = new ui.UINavigationController().initWithRootViewController(tableviewcontroller);
      navController.toolbarHidden = ui.UIDevice.currentDevice.userInterfaceIdiom !== ui.UIUserInterfaceIdiom.pad;
      this.window.rootViewController = navController;

      this.window.makeKeyAndVisible();
      return true;
    })
}));

console.log ("before chdir");
process.chdir(NSBundle.mainBundle.bundlePath);
console.log ("after chdir");

console.log ("before UIApplication.main, delegate name = " + HelloIOSAppDelegate._ck_register);
ui.UIApplication.main(process.argv, null, "HelloIOSAppDelegate" /*HelloIOSAppDelegate._ck_register*/);
console.log ("UIApplication.main returned, wtf");
