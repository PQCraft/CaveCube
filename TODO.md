### NEED TO DO:
- Redo BMD to CCM (delete `src/bmd/` and add `src/ccm/`)
    - Add parts, model types (3D, BLOCK, 2D, etc), and data types (VERT, ANI, AABB, etc)
- 3D model rendering in `src/renderer/renderer.c`
- Audio (probably SDL_Mixer) in `src/audio/`
- Redo worldgen in `src/game/worldgen.(c|h)`
    - Add structures to worldgen (trees, buildings, etc)
    - Generate values for height, spikes, details, moisture, and structures
- Implement gameplay in `src/server/game.(c|h)`
- Implement extensions in `src/main/extmgr.(c|h)` using libdl
- Add AABB collision in `src/physics/collision.(c|h)`
    - Use ticks https://gafferongames.com/post/fix_your_timestep/ and add interpolation
- Redo doGame() and remove loopDelay
    - Remove collision spaghetti
    - Remove delta time crust
    - Clean up math stuff
    - Fix block ray cast
- Make updateChunks() use a message list
- Make server ask for and handle player's positions
- Implement world saves in `src/server/saves.(c|h)`
- Implement server chunk cache/pool
- Smoothen out head tilts
- Add some server events
    - Add event for collecting player info
    - Add event for running physics
    - Add event for handling game logic
- Make it so that events are not pushed again until an acknowledge (prevents overflow)

### MIGHT DO:
- Implement bunny hopping after physics redo (resize AABB at bottom instead of top if in-air crouch)
- Direct3D support
- Greedy mesher in `src/renderer/renderer.c`

### IN-PROGRESS:
- 2D UI

### DONE:
- Enabled GLFW hint to scale to DPI
- Set GLFW multisampling hint to 0
- Fixed OpenGLES
- Improved thread timing
