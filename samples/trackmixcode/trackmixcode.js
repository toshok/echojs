
import { NSObject } from '$pirouette/foundation';
import { NSWindow, NSApplicationDelegate, NSApplication, NSButton, NSBezelStyle, NSSlider, NSTextField } from '$pirouette/appkit';
import { outlet } from '$pirouette/objc';

class Track {
    constructor() {
        this.volume = 0;
    }
}

let AppDelegate = NSObject.extendClass("AppDelegate", () => ({

    didFinishLaunching: function () {
        let window = this.window;
        let contentView = window.contentView;

        window.title = "Track Mix";
        window.frame = { x: 335, y: 390, width: 300, height: 480 };

        let button = NSButton.newWithFrame({ x: 107, y: 17, width: 82, height: 32 });
        button.title = "Mute";
        button.bezelStyle = NSBezelStyle.Rounded;
        button.clicked = () => this.mute();

        let slider = NSSlider.newWithFrame({ x:138, y: 62, width: 21, height: 321 });
        slider.minValue = 0;
        slider.maxValue = 11;
        slider.valueChanged = () => this.takeValueForVolumeFrom(this.slider);

        let textField = NSTextField.newWithFrame({ x: 80, y: 398, width: 136, height: 22});
        textField.alignment = 2;
        textField.textChanged = () => this.takeValueForVolumeFrom(this.textField);

        contentView.addSubview(button);
        contentView.addSubview(slider);
        contentView.addSubview(textField);

        this.button = button;
        this.slider = slider;
        this.textField = textField;
        this.track = new Track();
        this.updateUserInterface();
    },

    mute: function () {
        this.track.volume = 0;
        this.updateUserInterface();
    },

    takeValueForVolumeFrom: function(sender) {
        this.track.volume = sender.intValue; // TODO - Should be floatValue instead.
        this.updateUserInterface();
    },

    updateUserInterface: function() {
        let volume = this.track.volume;
        this.textField.floatValue = volume;
        this.slider.floatValue = volume;
    },

    window: outlet(NSWindow)
}), [
    NSApplicationDelegate 
]);

NSApplication.main (process.argv);

