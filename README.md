	A Space Invaders emulator, written in C using the SDL2 and SDL mixer libraries.
	
	## Features
	Full Intel 8080 implementation, complete with cycle counting.
	Colour & sound.
	2 player mode.
	
	## Controls
	C - Insert coin
	ENTER - Play single player game.
	2 - Play two player game.
	T - Tilt (causes a game over).
	Left/Right arrow keys - Move left/right (works for both player 1 & 2).
	SPACE - Fire
	
	## How to play
	Upon loading the emulator, insert a coin using the c key, and press either ENTER or 2, depending on if you want a single player or two player game. Once the game has started, use the arrow keys to move and the space bar to shoot.
	
	## Installation
	If you're on 64-bit Windows, download the zip archive on the releases page. Create a folder in your preferred directory, and unzip the zip archive inside said folder. Put your game ROMs in the ROM folder, and sounds in the Sounds folder if you want sound. Run invemu.exe.
	Alternatively, you can follow the build instructions and create the .exe yourself.
	
	If you're on another platform, you will need to build the project yourself. I'm on Windows so was not able to create an .exe file for other platforms.
	
	## Building
	If you want to build the project (i.e to generate an .exe yourself), you will need to have the following installed:
	-A C compiler, such as GCC.
	-SDL2
	-SDL Mixer
	
	I used the following for my build:
	WinLibs GCC version 13.2.0, with POSIX threads, with LLVM/Clang/LLD/LLDB.
	SDL 2.30.3 MinGW
	SDL mixer 2.8.0 MinGW
	
	## Possible Improvements
	While I created this emulator with learning as my main goal and consider it "done", no project is ever truly finished. The emulator could perhaps be improved with the following, for anyone who may wish to make improvements:
	-Game volume control
	-Full screen mode/Window resizing
	-Controller support
	-Saving high scores
	
	## CPU tests
	The processor emulation passes the following CPU tests:
	cpudiag
	TST8080
	CPUTEST
	8080PRE
	8080EXM/8080EXER
	
	## Credits
	I'd like to thank the following individuals/groups, without which this emulator would not have been possible:
	
	[Emulator101](http://www.emulator101.com/) For providing an excellent start to writing the emulator.
	[superzazu's Intel 8080 implementation](https://github.com/superzazu/8080), which was an immensely useful for output comparison when trying to fix my auxiliary carry.
	The emudev discord server and [subreddit](https://new.reddit.com/r/EmuDev/), for providing many resources as well as helping me with a few questions.
	[Computer Archeology](https://computerarcheology.com/Arcade/SpaceInvaders/) for detailed documentation on the game and its hardware.
	Taito for creating such an amazing game.
	You for playing!
