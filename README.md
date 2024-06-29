# MPGL

MIDI Player OpenGL (MPGL) was created in very early 2019 to try replace Piano From Above (PFA) in terms of better performance and less RAM use,
but instead it ended up turning into its own thing over time.

MPGL uses a modified Morshu-mmidi player core, which was a revolutionary player and decoder engine at the time, so it had superior playback performance compared to the rest of the players at the time.  
However, due to its single-threaded nature, and its note-ordering system, it doesn't quite hold up anymore today as it did back then, as the track scanning ends up taking more CPU time than actual decoding.  
But that's okay, it did its job on our 2nd-gen i7 and Core 2 Quad and Pentium CPUs :P

The Morshu-mmidi player core is equipped with the precise mmidi adaptive timer code, which adapts to the CPU load, unpredictable kernel times, and other unpredictable factors, and tries its best to stabilize playback performance, and create precise playback excellence if the hardware and software configuration allow for it.

MPGL has a wide feature set, almost all of which can be individually enabled or disabled for the chosen fine-tuned experience.

# System requirements
- OpenGL 3.0 (or depending on the chosen shaders, OpenGL 3.3) support
- Windows with said OpenGL drivers
  - Technically works down to Windows XP, but I doubt there are compatible drivers for it
  - Works perfectly in wine too!
- OmniMIDI 7.1 (and up)
  - Note: newer versions have slightly incompatible interface, so it might freeze with those.  
    This is my fault.

# Building

> Note: due to the age of the code, the standards are extremely sub-par; it's not representative of what I'd write today.

> Note: Khronos OpenGL headers (GLKHR) are included in the `inc` folder, so it's not necessary to download those separately.

- Install `gcc` (minimum gcc 5.3.0, either Windows native, or cross-compiling works)
- Edit `Makefile` for compile flags (optional)
- Run `make`
- `out\MPGL.exe` is built


# License
MIDI Player OpenGL  
Copyright (C) 2019-2024 Sono  ([https://github.com/SonoSooS](https://github.com/SonoSooS))  
All Rights Reserved.  
