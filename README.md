# shadert0y

A Frei0r filter plugin that runs Shadertoy GLSL shaders inside Kdenlive,
or other MLT-based hosts.

The shader is rendered offscreen using EGL + OpenGL 3.3 core, then written back
to the host video frame.

2 Examples of a generative shader and a shader used as an effect are given.

The main shader uses the normal Shadertoy entry point:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // ...
}
```

You do **not** need to add:

- `#version`
- `uniform` declarations
- `void main()`
- output color declarations

The plugin adds those automatically.

---

## Features

- Shadertoy-style `mainImage()` shader loading
- Shader file hot-reload by file modification time
- `iChannel0`, `iChannel1`, `iChannel2`, and `iChannel3` selectable per channel
- Channel sources:
  - `None`
  - `Current Clip`
  - `File 0`
  - `File 1`
  - `File 2`
- `File 0`, `File 1`, and `File 2` can be:
  - image files
  - basic standalone `.glsl` shader buffers
- Internal render resolution override:
  - shader can render at a fixed project resolution
  - result is then scaled to the preview/output frame
- Mouse position controlled by parameters
- Time speed control
- Optional input video vertical flip
- Default animated color shader when no shader file is selected
- Shader compile/link errors are printed to the terminal and fall back to the
  default color shader

---

## Current limitations

This is **not** a full Shadertoy runtime.

Not supported:

- Full Shadertoy Buffer A/B/C/D multipass system
- Buffer scripts reading other buffers
- Buffer script feedback / previous-frame ping-pong
- Buffer script iChannels
- Audio / music / sound textures
- Webcam / keyboard input
- Real host mouse input
- Grabbing arbitrary clips from the Kdenlive timeline
- Decoding video files inside `File 0/1/2`
- Linear-light / HDR color management

The plugin receives only the current input frame from the host. It cannot ask
Kdenlive for another timeline clip. For transition-style A/B behavior, the
plugin would need to be rewritten as a mixer/transition-style plugin, not a
normal filter.

---

## Dependencies

Debian/Ubuntu:

```bash
sudo apt install build-essential cmake pkg-config \
    frei0r-plugins-dev libegl1-mesa-dev libgl-dev
```

On some distributions the Frei0r development package may be called:

```bash
libfrei0r-dev
```

You also need `stb_image.h` in the source directory. This is already provided in the repo.

If you somehow do not already have it:

```bash
wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
```

---

## Build

```bash
mkdir build
cd build
cmake ..
make
```

This produces:

```txt
shadertoy.so
```

---

## Install the plugin

Copy the plugin into a Frei0r plugin directory.

Per-user, no root required:

```bash
mkdir -p ~/.frei0r-1/lib
cp shadertoy.so ~/.frei0r-1/lib/
```

Or system-wide:

```bash
sudo cp shadertoy.so /usr/lib/frei0r-1/
```

If Kdenlive does not see the plugin, launch Kdenlive with:

```bash
FREI0R_PATH=~/.frei0r-1/lib kdenlive
```

---

## Install the Kdenlive effect XML

The `shadert0y.xml` file provides the Kdenlive effect UI and parameter layout.

Copy it to your Kdenlive effects directory:

```bash
mkdir -p ~/.local/share/kdenlive/effects
cp shadert0y.xml ~/.local/share/kdenlive/effects/
```

Then restart Kdenlive.

Search the effects list for:

```txt
Shadert0y
```

If an old version of the effect is already on a clip, remove it and add it
again after updating the plugin/XML.

---

## Parameters

| Parameter | Type | Description |
|---|---:|---|
| `Script File` | file path | Main `.glsl` / `.frag` shader file |
| `Speed` | animated float | Time multiplier. Negative values reverse time |
| `Flip Video Y` | bool | Flip the input video texture vertically |
| `iChannel0` | list | Source for `iChannel0` |
| `iChannel1` | list | Source for `iChannel1` |
| `iChannel2` | list | Source for `iChannel2` |
| `iChannel3` | list | Source for `iChannel3` |
| `File 0` | file path | Image file or basic shader buffer |
| `File 1` | file path | Image file or basic shader buffer |
| `File 2` | file path | Image file or basic shader buffer |
| `Mouse X` | animated float | Normalized mouse X, `0.0` to `1.0` |
| `Mouse Y` | animated float | Normalized mouse Y, `0.0` to `1.0` |
| `Render Width` | int | Internal shader render width. `0` = automatic |
| `Render Height` | int | Internal shader render height. `0` = automatic |

### Notes

- `iResolution` is the internal render resolution.
- If `Render Width` and `Render Height` are `0`, `iResolution` matches the
  incoming frame size.
- If `Render Width` and `Render Height` are set, the shader renders at that
  size and the result is scaled to the host frame.
- `iMouse.xy` are in internal render-resolution pixels.
- `iMouse.zw` are currently always `-1.0`.
- `iChannelTime` is currently the same as `iTime` for all channels.
- `iChannelResolution` depends on the selected channel source:
  - `None`: `1x1`
  - `Current Clip`: incoming host frame size
  - image file: image size
  - shader buffer file: internal render resolution

---

## Shader file format

Your shader file should contain only:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = vec4(uv, 0.5 + 0.5 * sin(iTime), 1.0);
}
```

You may also include helper functions, constants, and structs.

Do not include:

```glsl
#version
```

The plugin strips `#version` lines automatically, but it is cleaner not to
include them.

Do not include:

```glsl
void main()
{
    ...
}
```

The plugin generates the final `main()` wrapper automatically.

Also remove WebGL/GLES precision qualifiers if present:

```glsl
precision mediump float;
precision highp float;
```

Desktop OpenGL 3.3 core does not use those.

---

## Default shader

If `Script File` is empty, missing, or fails to compile, the plugin uses a
small default animated color shader:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = vec4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
```

If you want any other default shader instead, change `kDefaultShader` in the C++ source and recompile.

---

## Using image files

`File 0`, `File 1`, and `File 2` support common image formats through
`stb_image`, for example:

```txt
.png
.jpg
.jpeg
.bmp
.tga
```

Example:

```txt
iChannel0 = File 0
File 0 = /home/user/Pictures/texture.png
```

Then in the shader:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = texture(iChannel0, uv);
}
```

Image files are flipped on upload so they should appear upright when using
normal Shadertoy-style UV coordinates.

---

## Using basic shader buffer files

`File 0`, `File 1`, and `File 2` can also be standalone shader scripts.

Supported shader extensions:

```txt
.glsl
.frag
.vert
.comp
.fs
.shader
.txt
```

Example buffer file:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;

    vec3 col;
    col.r = uv.x;
    col.g = uv.y;
    col.b = 0.5 + 0.5 * sin(iTime);

    fragColor = vec4(col, 1.0);
}
```

Save it as:

```txt
buffer0.glsl
```

Then set:

```txt
iChannel0 = File 0
File 0 = /home/user/shaders/buffer0.glsl
```

Main shader:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = texture(iChannel0, uv);
}
```

Important:

- Buffer scripts are rendered before the main shader.
- Buffer scripts receive:
  - `iResolution`
  - `iTime`
  - `iTimeDelta`
  - `iFrame`
  - `iMouse`
- Buffer scripts do **not** receive real iChannels.
- Inside buffer scripts, `iChannel0..3` are bound to black.
- Buffer scripts cannot read previous frames.
- Buffer scripts cannot read other buffers.

This is useful for simple generated textures, but it is not a full Shadertoy
Buffer A/B/C/D system.

---

## Internal render resolution

Kdenlive may give the plugin a preview-sized frame. The plugin cannot
automatically know the real project resolution unless the host provides it.

Leave both at:

```txt
Render Width = 0
Render Height = 0
```

to use the incoming frame size automatically.

---

## Hot reload

The main shader file is watched by modification time.

You can edit the shader in a text editor and scrub the Kdenlive timeline to see
changes.

File buffers and image files are also reloaded when their modification time
changes.

---

## Example shaders

### Solid color

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
```

### Animated gradient

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = vec4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
```

### Passthrough

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = texture(iChannel0, uv);
}
```

### Split channels

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;

    vec3 left  = texture(iChannel0, uv).rgb;
    vec3 right = texture(iChannel1, uv).rgb;

    float split = step(0.5, uv.x);

    fragColor = vec4(mix(left, right, split), 1.0);
}
```

Set:

```txt
iChannel0 = Current Clip
iChannel1 = File 0
File 0 = some_image.png
```

---

## Color handling

Colors are handled as plain 8-bit RGBA values.

The plugin does:

```txt
8-bit RGBA input -> shader -> 8-bit RGBA output
```

There is no linear-light conversion, no HDR handling, and no special color
management. This matches how Frei0r/MLT normally handles 8-bit frames.

The output alpha value is forced to `1.0`.

---

## Troubleshooting

### Plugin does not appear in Kdenlive

Check the Frei0r search path.

You can launch Kdenlive with:

```bash
FREI0R_PATH=~/.frei0r-1/lib kdenlive
```

Also check:

```txt
Kdenlive → Settings → Configure Kdenlive → Environment
```

Then restart Kdenlive.

---

### Effect UI looks wrong or parameters are missing

Remove the effect from the clip, restart Kdenlive, then add the effect again.

Make sure the installed `shadert0y.xml` matches the current plugin parameter
layout.

Current parameter layout:

```txt
0  Script File
1  Speed
2  Flip Video Y
3  iChannel0
4  iChannel1
5  iChannel2
6  iChannel3
7  File 0
8  File 1
9  File 2
10 Mouse X
11 Mouse Y
12 Render Width
13 Render Height
```

---

### Shader compiles on shadertoy.com but not here

Common causes:

- shader uses Shadertoy Buffer A/B/C/D passes
- shader uses sound/audio input
- shader uses cubemap input
- shader uses WebGL/GLES precision qualifiers
- shader uses `texture2D()` instead of `texture()`
- shader expects full Shadertoy buffer feedback

Remove precision qualifiers:

```glsl
precision mediump float;
precision highp float;
```

Replace WebGL-style texture lookups:

```glsl
texture2D(iChannel0, uv)
```

with desktop GLSL:

```glsl
texture(iChannel0, uv)
```

---

### Output is just the default animated gradient

That means one of these is true:

- `Script File` is empty
- `Script File` could not be opened
- shader failed to compile
- shader failed to link

Run Kdenlive from a terminal and look for messages starting with:

```txt
[frei0r-shadertoy]
```

Example:

```bash
kdenlive
```

or:

```bash
FREI0R_PATH=~/.frei0r-1/lib kdenlive
```

---

### File image does not appear

Check that:

- the file path is correct
- the file is a supported image format
- the matching iChannel is set to `File 0`, `File 1`, or `File 2`

Example:

```txt
iChannel0 = File 0
File 0 = /home/user/Pictures/image.png
```

Video and audio files are not decoded. If you point a file parameter at a video
or music file, it will simply fail to load and the channel will fall back to
black.

---

### Clearing the shader file path does not reset the shader

Some Kdenlive file widgets may not commit the cleared value immediately.

After clearing the path:

- press Enter, or
- click another parameter field

The plugin trims whitespace from paths and resets when it receives an empty
path.

---

### `eglCreateContext failed`

Your EGL driver may not support OpenGL 3.3 core.

On a headless system, VM, or GPU-less environment, try Mesa software rendering:

```bash
LIBGL_ALWAYS_SOFTWARE=1 kdenlive
```

---

### Preview resolution does not match project resolution

Set:

```txt
Render Width = project width
Render Height = project height
```

Example for 1080p:

```txt
Render Width = 1920
Render Height = 1080
```

The shader will render internally at that resolution and the result will be
scaled to the host frame.

---

## Project files

```txt
shadert0y.cpp    main plugin source
shadert0y.xml    Kdenlive effect UI / parameter metadata
stb_image.h      image loading library
README.md        this file
examples/        example shaders
```
