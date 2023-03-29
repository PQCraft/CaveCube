### INFO
- Next version: 0.7.0

### NEED TO DO:
- Implement server chunk cache/pool
- Add back block placement
- Add physics in `src/physics/`
    - Use ticks https://gafferongames.com/post/fix_your_timestep/ and add interpolation
    - Add AABB collision in `collision.(c|h)`
    - Add raycasting in `raycasting.(c|h)`
- Implement world saves in `src/server/saves.(c|h)`
- Add some server events
    - Add event for collecting player info
    - Add event for running physics
    - Add event for handling game logic
- Decrustify server message UUID and pdata validity checks
- Make it so that timers are not pushed again until an acknowledge (prevents overflow)
- Make server ask for and handle player's positions
- Clean up cave culling crust in renderer
- Redo BMD to CCM (delete `src/bmd/` and add `src/ccm/`)
    - Add parts, model types (3D, BLOCK, 2D, etc), and data types (VERT, ANI, AABB, etc)
    - Block models use quads instead of triangles
- 3D model rendering in `src/renderer/renderer.c`
- Audio (probably miniaudio or SDL Mixer) in `src/audio/`
- Redo worldgen in `src/game/worldgen.(c|h)`
    - Add structures to worldgen (trees, buildings, etc)
    - Generate values for height, spikes, details, moisture, and structures
- Integrate BCBASIC
- Implement gameplay
    - 40ms tick speed (25 ticks/sec)
- Add a sky (sun, moon, and stars)
- Smoothen out head tilts
- Make a translator to allow connecting to older servers (once there is a stable protocol)

### MIGHT DO:
- Implement bunny hopping after physics redo (resize AABB at bottom instead of top if in-air crouch)
- Direct3D support
- Greedy mesher in `src/renderer/renderer.c`
- Android support (requires that it be buildable from the command-line using `make` so it can be used in `mkrelease.sh`)
- Make updateChunks() use a message list
- Implement extensions in `src/main/extmgr.(c|h)` using libdl

### IN-PROGRESS:
- Make server executable work again
- Flesh out UI
    - More UI elements (text boxes, scrollbars, drop-down menus, sliders, toggles, etc)
    - UI interaction (elem states, tooltips, text input, etc)
    - Move text processing from meshUIElem to calcUIProperties
    - Add `text_fmt` attrib
    - Add escape code processing when `text_fmt` is `true`
        - `\eFCh` sets foreground color to 0xh
        - `\eBCh` sets foreground color to 0xh
        - `\eFAhh` sets foreground alpha to 0xhh
        - `\eBAhh` sets foreground alpha to 0xhh
        - `\eAh` sets text attributes to 0xh
            - 0x1 = bold
            - 0x2 = italic
            - 0x4 = underline
            - 0x8 = strikethrough
        - `\eR` resets all changes made by escape codes
    - Add `t` as a number suffix for size to fit to number \* text size
    - Add `c` as a number suffix for size to fit to number \* size of children
    - Add `minwidth` and `maxwidth` attribs
    - Add items to hotbar

### DONE:
- Made natural light monochrome to save bits
- Improved mesher performance
- Improved server memory management
- Server now also disconnects on a write error
- Fixed a bug that prevented Windows from connecting in -connect mode
- New worldgen
- Memory management fixes
- Renderer improvements
- RNG improvements
- Update stb_image
- Improve image resource loading
