# wincon
Resizing and Selection for the Native Console

## Features
- Resize the console like any other window.
- Row based selection of text like in a normal editor.
- Select whole words or lines by double or tripple clicking.
- Maximize to largest available desktop size.

## Installation
It is a 120kb single file executable, with no dependencies.

So [download][release], save and run.

## Running
Double-clicking from the Explorer, wincon will launch %comspec% in a new console window and attach itself to it.

Running from an existing console, will cause wincon to attach itself to that console.

## Notes
- Resizing using the left side borders is not implemented yet.
- Column based selection is not implemented yet (native console's box selection is still available via the menu).
- The selection will be automatically copied to the clipboard.
- Using the override key (ctrl) enables wincon's selection even for applications that usually want to handle the mouse by themselves (like Far Manager for example).

## Screenshots

![Screenshot][ss]

<sub>This project is licensed under the terms of the GNU GPL v2.0 license</sub>

[release]: https://github.com/kobilutil/wincon/releases/latest
[ss]: https://github.com/kobilutil/wincon/raw/master/images/screenshot.png "Screenshot"
