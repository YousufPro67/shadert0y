// Resolution-limited fork of "Protean clouds" 
// by nimitz: https://shadertoy.com/view/3l23Rh

// BufferA is the original shader, but with an upper limit on the
// resolution. In my real OpenGL 4.6 program at home, a maximum of
// 1024 pixels is good enough to upscale to 4K on a 77" TV without
// noticeable artifacts. If you're at normal viewing distance for such
// a large screen, even 512 is enough, although most shaders can't go
// that low.

// Match the MAXRES definitions here and in BufferA to play with
// other values. Obviously if the browser viewport is smaller than
// this value, no scaling will happen, and here I haven't bothered
// with the case where vertical resolution is greater than horizontal.

#define MAXRES 1024.0

// Comment this to see resolution-limited output (obviously you must
// either have the drawing canvas larger than MAXRES, or be running
// full-screen).

#define UPSCALE

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{	
    #ifdef UPSCALE
        float scaling = (iResolution.x > MAXRES)
            ? MAXRES / iResolution.x
            : 1.0;
    #else
        float scaling = 1.0;
    #endif
    
    vec2 uv = (fragCoord.xy / iResolution.xy) * scaling;
    fragColor = texture(iChannel0, uv);
}
