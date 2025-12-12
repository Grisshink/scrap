# v0.5-beta *(Upcoming)*

## What's new?
- Added new experimental LLVM compiler backend. It will be available as a separate download option in the releases
- Terminal contents are now cropped when the terminal is resized
- Added block previews when trying to attach blockchains together

## Fixes
- Fixed numpad Enter key not working in certain scenarios
- Fixed race condition with thread running state after calling `pthread_create`. This should theoretically fix code not stopping its execution on Windows
- Fixed code area being able to scroll while window is open
- Fixed memory leak when copying blockdef with instantiated blocks
- Fixed block search menu being able to open when code is running

# v0.4.2-beta *(12-02-2025)*

## Fixes
- Fixed loaded custom blocks having undefined hints
- Fixed textboxed being interactable while vm is running
- Fixed code area floating away when editing panel while the block is selected
- Minor translation fixes

# v0.4.1-beta *(07-02-2025)*

## What's new?
- Added Ukrainian translation *(by @jackmophin)*

## Fixes
- Fixed localizations sometimes not working on Windows
- Fixed codebase scrolling with search menu at the same time

# v0.4-beta *(05-02-2025)*

## What's new?
- Added translations for 2 languages: Russian *(by @Grisshink)* and Kazakh *(by @unknownkeyNB)*
- The sidebar (Now named as block palette to remove ambiguity) is now split up into various categories to make finding blocks easier
- The terminal's background color has been changed to match with the color of other panels
- All of the text boxes were upgraded to allow inserting or deleting at any position
- Now if any block input is empty, it will show a small hint of what it needs
- Added codebase movement through keyboard keys, see `README.md` for more details
- Added block search menu. You can open it by pressing `S` in code area

## Fixes
- Fixed "crash" when vm overflows/underflows one its stacks
- Fixed crash when trying to divide by zero in code
- Fixed scrollbars sometimes appearing behind scroll container
- Fixed text in the settings going out of bounds when the text is too large
- Fixed codespace occasionally jumping far away when it is being dragged outside of the window
- Fixed code renderer not checking with proper culling bounds. This should slightly improve performance of the renderer

# v0.3.1-beta *(28-01-2025)*

## Fixes
- Fixed a crash when attaching a block on a chain which overlaps on top of another block which is an argument to another block in different chain
- Fixed scroll containers not updating their lower bounds when resized

# v0.3-beta *(26-01-2025)*

## What's new?
- The whole gui system was reworked to remove cluttered code and to allow more flexible UI. As a result some parts of the UI may have changed slightly
- Added `New project` button to quickly clear the workspace
- The code area has been split up into flexible, customizable panels. This gives a lot more choice for UI customization
- Now scrap only redraws the screen if its internal state is changed or an input is recieved
- Updated `actual_3d.scrp` example
- Now scrap config is saved in OS specific folder, instead of saving relative to the working directory
- Added terminal colors support
- Added `colors.scrp` example to demonstrate the use of colors

# v0.2-beta *(06-01-2025)*

## What's new?
- Added various string manipulation blocks
- Added shortcuts to run/stop project (they are bound to `F5` and `F6` buttons respectively)
- Added codebase movement through scroll wheel/touchpad
- Added another 3D example

## Fixes
- Fixed save corruption when saving unused custom blocks
- Fixed custom blocks breaking when deleting arguments
- Fixed some compilation issues with gcc-9
- Fixed AppImage paths issue again
- Fixed block highlighting with custom blocks

# v0.1.1-beta *(30-12-2024)*

## What's new?
- Added icon to the executable
- Added experimental MacOS release provided by @arducat

## Fixes
- Fixed AppImage paths issue
- Fixed some resizing issues on linux

# v0.1-beta *(29-12-2024)*
- First beta release!
