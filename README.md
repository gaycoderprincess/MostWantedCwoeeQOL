# Chloe's Smol Quality-Of-Life Bundle

A mod for Need for Speed: Most Wanted that fixes a couple of small issues

## Installation

- Make sure you have v1.3 of the game, as this is the only version this plugin is compatible with. (exe size of 6029312 bytes)
- Plop the files into your game folder.
- Enjoy, nya~ :3

## Features

- An in-game toggle for motion blur
- Auto-detection for PAL/NTSC movies, fixing the issue with FMVs not playing
- Basic physics framerate override
- Removed the check for an Underground 2 savefile, always giving you the 10k bonus (without a popup message)
- Ability to move savegames from the Documents folder to the game folder
- Fixed a crash if you hit objects with an AI car
- Fixed a crash if your car gets totaled in freeroam
- Fixed some modded cars showing up as locked in the career garage

## Building

Building is done on an Arch Linux system with CLion and vcpkg being used for the build process. 

Before you begin, clone [nya-common](https://github.com/gaycoderprincess/nya-common) and [nya-common-nfsmw](https://github.com/gaycoderprincess/nya-common-nfsmw) to folders next to this one, so they can be found.

Required packages: `mingw-w64-gcc`

You should be able to build the project now in CLion.
