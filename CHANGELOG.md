# v0.6-beta *(31-01-2026)*

## What's new?
- Scrap now shows confirmation window when trying to close scrap with unsaved changes
- Blocks inside block pallete are now split into subcategories
- Added new color type and integrated it with color related blocks
- Added new example (neon.scrp) featuring new color system
- Added ability to change colors of custom blocks
- Added browse button for path text inputs
- Added various icons to buttons and windows
- Added multiple block pickup system. You can now pickup and drop multiple block chains using `shift` button
- Added a settings option to toggle block previews
- UI size and Font settings can now be applied without reloading the app

## Fixes
- Fixed block definitions being able to be edited in block palette
- Fixed pow block not working with float values in compiler
- Dropdowns now close when dragging code area with middle mouse button. This prevents a bunch of crashes related to dropdowns

# v0.5.1-beta *(27-12-2025)*

## What's new?
- Updated Raylib version to latest commit (fc843dc557 as of time writing)
- Added visual feedback when settings are applied
- Text inputs in blocks are now rendered as slightly tinted to block color

## Fixes
- Fixed search menu spawning incomplete control blocks
- Fixed text inputs being editable in block palette
- Projects built with LLVM now switch to UTF-8 code page on windows. This fixes garbled output when printing characters outside ASCII range

# v0.5-beta *(20-12-2025)*

## What's new?
- Added new experimental LLVM compiler backend. It is available as a separate download option in the releases (tagged with `-llvm`)
- (LLVM only) Added option to build scrap project into executable. Windows users are expected to have gcc (Specifically `x86_64-w64-mingw32-gcc`) installed and added to PATH, Linux users only need a linker to build, such as `ld` (Configurable in build options)
- All variables declared under `When |> clicked` are now considered global and can be accessed anywhere in the code. The rest of the variables are considered local and only visible in the scope they are declared
- Added a lot of runtime/compile time errors, such as out of scope variable access, mismatched types and etc.
- Shortened list and variable blocks to make code a bit more concise
- Implemented text selection to all text inputs, as well as more useful shortcuts such as Shift selection (Shift-<arrows>), copy (Ctrl-C), paste (Ctrl-V), cut (Ctrl-X), delete all (Ctrl-U) and select all (Ctrl-A)
- Terminal contents are now cropped when the terminal is resized
- Added block previews when trying to attach blockchains together
- Save files are no longer limited to 32 KB

## Fixes
- Fixed numpad Enter key not working in certain scenarios
- Fixed race condition with thread running state after calling `pthread_create`. This should theoretically fix code not stopping its execution on Windows
- Fixed code area being able to scroll while window is open
- Fixed memory leak when copying blockdef with instantiated blocks
- Fixed block search menu being able to open when code is running
- Control blocks (like while, if) now render correctly in the block palette
- Fixed code area being able to be dragged outside the code panel when code is running

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
