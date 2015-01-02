
import { NSObject } from '$pirouette/foundation';
import { NSWindow, NSApplicationDelegate, NSApplication, NSSlider, NSTextField } from '$pirouette/appkit';
import { instanceSelector, outlet, sig } from '$pirouette/objc';

class Track {
    constructor() {
        this.volume = 0;
    }
}

let AppDelegate = NSObject.extendClass("AppDelegate", () => ({
    didFinishLaunching: function () {
        let track = new Track();
        this.track = track;
        this.updateUserInterface();
    },

    mute: instanceSelector("mute:")
        .impl(function () {
            this.track.volume = 0;
            this.updateUserInterface();
        }),

    takeValueForVolumeFrom: instanceSelector("takeValueForVolumeFrom:")
        .returns(sig.Void)
        .params([ NSObject ])
        .impl(function(source) {
            this.track.volume = source.floatValue;
            this.updateUserInterface();
        }),

    updateUserInterface: function() {
        let volume = this.track.volume;
        this.textField.floatValue = volume;
        this.slider.floatValue = volume;
    },

    shouldTerminateAfterLastWindowClosed: function () {
        return true;
    },

    slider: outlet(NSSlider),
    textField: outlet(NSTextField),
    window: outlet(NSWindow)
}), [
    NSApplicationDelegate 
]);

NSApplication.main (process.argv);

