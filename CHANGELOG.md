# v0.3.1-beta
- Fixed a crash when attaching a block on a chain which overlaps on top of another block which is an argument to another block in different chain
- Fixed scroll containers not updating their lower bounds when resized

# v0.3-beta
- The whole gui system was reworked to remove cluttered code and to allow more flexible UI. As a result some parts of the UI may have changed slightly
- Added `New project` button to quickly clear the workspace
- The code area has been split up into flexible, customizable panels. This gives a lot more choice for UI customization
- Now scrap only redraws the screen if its internal state is changed or an input is recieved
- Updated `actual_3d.scrp` example
- Now scrap config is saved in OS specific folder, instead of saving relative to the working directory
- Added terminal colors support
- Added `colors.scrp` example to demonstrate the use of colors

# v0.2-beta
- Added various string manipulation blocks
- Added shortcuts to run/stop project (they are bound to `F5` and `F6` buttons respectively)
- Added codebase movement through scroll wheel/touchpad
- Added another 3D example

- Fixed save corruption when saving unused custom blocks
- Fixed custom blocks breaking when deleting arguments
- Fixed some compilation issues with gcc-9
- Fixed AppImage paths issue again
- Fixed block highlighting with custom blocks

# v0.1.1-beta
- Fixed AppImage paths issue

# v0.1-beta
- First beta release!
