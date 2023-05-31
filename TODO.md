### INFO
- Next version: 0.7.0

### NEED TO DO:
- Figure out how to build the Android APK from the command line when using `make`
- Make server executable work again
- Palette compression
    - Each 16x16x16 section of a chunk has its own palette
    - `uint32_t` index
        - Top 20 bits contain lighting info
        - bottom 12 bits contain the index
    - Add palette sharing as an option
        - May not reduce memory usage by a lot and may use a lot of CPU in return
- CCScript
- Decrustify server message UUID and pdata validity checks
- Implement server chunk cache/pool
- Add back block placement
- Heavy mesher optimization
    - Don't mesh culled chunks
        - Skip individual sections if possible
- Worldgen optimization
    - Redo code to build a chunk in stages
        - Add structures to worldgen (trees, buildings, etc)
        - Generate values for height, spikes, details, moisture, and structures
    - Cache and interpolate noise to save CPU
    - Generate multiple noise values at once using SIMD
- Add physics in `src/physics/`
    - Use [ticks](https://gafferongames.com/post/fix_your_timestep/) and add interpolation
    - Add AABB collision in `collision.(c|h)`
    - Add raycasting in `raycasting.(c|h)`
- Add some server events
    - Add event for collecting player info
    - Add event for running physics
    - Add event for handling game logic
- Make it so that timers are not pushed again until an acknowledge (prevents overflow)
- Clean up some cave culling crust in renderer
- Implement CCDB in `src/common/ccdb.(c|h)`
- Implement world saves in `src/server/saves.(c|h)`
- Make server ask for and handle player's positions
- Implement CCM in `src/common/ccm.(c|h)`
- 3D model rendering in `src/renderer/renderer.c`
- Audio (probably miniaudio or SDL Mixer) in `src/audio/`
- Implement gameplay
    - 40ms tick speed (25 ticks/sec)
- Add a sky (sun, moon, and stars)
- Smoothen out head tilts
- Make a translator to allow connecting to older servers (once there is a stable protocol)

### MIGHT DO:
- Implement bunny hopping after physics redo (resize AABB at bottom instead of top if in-air crouch)
- Make mesher greedy
- OG Xbox port
- Make updateChunks() use a message list
- Render thread

### IN-PROGRESS:
- UI rewrite

### DONE:
- Made natural light monochrome to save bits
- Improved mesher performance
- Improved server memory management
- Server now also disconnects on a write error
- New worldgen
- Memory management fixes
- Renderer improvements
- RNG improvements
- Update stb_image
- Improve image resource loading
- Improved mouse input when using SDL2
- Added Android support
