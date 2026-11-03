Ultima VIII: Pagan, portable
============================

Ultima VIII made to run on today's Windows and Linux, from the decompiled game source. It
needs your own copy of the game: nothing of the game itself is included.

Starting
--------

Point it at the folder of an installed Ultima VIII, the one that holds STATIC, USECODE, SOUND
and SAVEGAME (in the GOG version, the ENGLISH folder):

    Windows:  Ultima8.exe --data "C:\GOG Games\Ultima 8\ENGLISH"
    Linux:    ./Ultima8 --data "/path/to/Ultima 8/ENGLISH"

The folder is remembered, so later a double-click starts it. Placed inside the game folder
itself, it finds the data on its own. The game folder is only read: settings, saved games and
recordings go to the home folder (%APPDATA%\u8port on Windows, ~/.u8port on Linux).

  F11        the settings: sound, display, game
  Alt+Enter  fullscreen

Music plays on the emulated Roland MT-32 when its ROM files (not included) are in the game
folder or the folder named in the settings, and on the emulated AdLib otherwise.

Every session is recorded (the newest five are kept) and can be replayed exactly:

    Ultima8 --replay <home folder>/recordings/<date and time>

Needs
-----

Windows 10 or 11, 64-bit, with a graphics driver that has Vulkan (current drivers do).

Linux, 64-bit, with glibc 2.38 or newer (Ubuntu 24.04, Debian 13, Fedora 39 or later), X11 or
XWayland, Vulkan or OpenGL, and ALSA (or the ALSA plug-in of PipeWire or PulseAudio).

More, and where to report problems: https://github.com/KarlClinckspoor/Ultima8_DecompPort
(ultima8-portable/README.md).
