### NEED TO DO:
- Implement server chunk cache/pool
- Implement world saves in `src/server/saves.(c|h)`
- Redo almost everything to do with chunks/blocks
- Ensure that every function that does block operations takes a non-inverted z
- Flesh out UI
    - More UI elements (buttons, text boxes, etc)
    - UI interaction (elem states, tooltips, etc)
    - Add `fit_width_to_text` and `fit_height_to_text` attrib
    - Add `text_fmt` attrib
    - Add items to hotbar
- Add physics in `src/physics/`
    - Use ticks https://gafferongames.com/post/fix_your_timestep/ and add interpolation
    - Add AABB collision in `collision.(c|h)`
    - Add raycasting in `raycasting.(c|h)`
- Redo doGame() and remove loopDelay
    - Remove collision spaghetti
    - Use physics code for collision and raycasting
- Add some server events
    - Add event for collecting player info
    - Add event for running physics
    - Add event for handling game logic
- Make it so that timers are not pushed again until an acknowledge (prevents overflow)
- Make server ask for and handle player's positions
- Redo BMD to CCM (delete `src/bmd/` and add `src/ccm/`)
    - Add parts, model types (3D, BLOCK, 2D, etc), and data types (VERT, ANI, AABB, etc)
    - Block models use quads instead of triangles
- 3D model rendering in `src/renderer/renderer.c`
- Audio (probably miniaudio or SDL_Mixer) in `src/audio/`
- Redo worldgen in `src/game/worldgen.(c|h)`
    - Add structures to worldgen (trees, buildings, etc)
    - Generate values for height, spikes, details, moisture, and structures
- Implement gameplay in `src/server/game.(c|h)`
- Implement extensions in `src/main/extmgr.(c|h)` using libdl
- Smoothen out head tilts
- Make a translator to allow connecting to older servers (once there is a stable protocol)

### MIGHT DO:
- Implement bunny hopping after physics redo (resize AABB at bottom instead of top if in-air crouch)
- Direct3D support
- Greedy mesher in `src/renderer/renderer.c`
- Android support (requires that it be buildable from the command-line using `make` so it can be used in `mkrelease.sh`)
- Make updateChunks() use a message list

### IN-PROGRESS:

### DONE:
- Renderer improvements
- Added chunk compression using zlib
- Improved mesher latency
- Added transparency sorting
- Fixed OpenGLES
- Added tinted and stained glass
