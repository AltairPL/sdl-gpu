// Hacks to fix compile errors due to polluted namespace
#ifdef _WIN32
#define _WINUSER_H
#define _WINGDI_H
#endif

#include "SDL_gpu_OpenGL_internal.h"
#include <math.h>
#include <strings.h>
int strcasecmp(const char*, const char *);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Uint8 isExtensionSupported(const char* extension_str)
{
    return (strstr((const char*)glGetString(GL_EXTENSIONS), extension_str ) != NULL);
}

static Uint8 checkExtension(const char* str)
{
    if(!isExtensionSupported(str))
    {
        GPU_LogError("OpenGL error: %s is not supported.\n", str);
        return 0;
    }
    return 1;
}

static GPU_FeatureEnum enabled_features = 0xFFFFFF; //GPU_FEATURE_ALL;

static void initNPOT(void)
{
#ifdef SDL_GPU_USE_OPENGL
    if(glewIsExtensionSupported("GL_ARB_texture_non_power_of_two"))
        enabled_features |= GPU_FEATURE_NON_POWER_OF_TWO;
    else
        enabled_features &= ~GPU_FEATURE_NON_POWER_OF_TWO;
#elif defined(SDL_GPU_USE_OPENGLES)
	if(isExtensionSupported("GL_OES_texture_npot"))
        enabled_features |= GPU_FEATURE_NON_POWER_OF_TWO;
    else
        enabled_features &= ~GPU_FEATURE_NON_POWER_OF_TWO;
#endif
}

static void initFBO(void)
{
#ifdef SDL_GPU_USE_OPENGL
    if(glewIsExtensionSupported("GL_EXT_framebuffer_object"))
        enabled_features |= GPU_FEATURE_RENDER_TARGETS;
    else
        enabled_features &= ~GPU_FEATURE_RENDER_TARGETS;
#elif defined(SDL_GPU_USE_OPENGLES)
	if(isExtensionSupported("GL_OES_framebuffer_object"))
        enabled_features |= GPU_FEATURE_RENDER_TARGETS;
    else
        enabled_features &= ~GPU_FEATURE_RENDER_TARGETS;
#endif
}

static void initBLEND(void)
{
#ifdef SDL_GPU_USE_OPENGL
    enabled_features |= GPU_FEATURE_BLEND_EQUATIONS;
    enabled_features |= GPU_FEATURE_BLEND_FUNC_SEPARATE;
#elif defined(SDL_GPU_USE_OPENGLES)
	if(isExtensionSupported("GL_OES_blend_subtract"))
        enabled_features |= GPU_FEATURE_BLEND_EQUATIONS;
    else
        enabled_features &= ~GPU_FEATURE_BLEND_EQUATIONS;
	if(isExtensionSupported("GL_OES_blend_func_separate"))
        enabled_features |= GPU_FEATURE_BLEND_FUNC_SEPARATE;
    else
        enabled_features &= ~GPU_FEATURE_BLEND_FUNC_SEPARATE;
#endif
}

void extBindFramebuffer(GLuint handle)
{
    if(enabled_features & GPU_FEATURE_RENDER_TARGETS)
        glBindFramebuffer(GL_FRAMEBUFFER, handle);
}


static inline Uint8 isPowerOfTwo(unsigned int x)
{
  return ((x != 0) && !(x & (x - 1)));
}

static inline unsigned int getNearestPowerOf2(unsigned int n)
{
        unsigned int x = 1;
        while(x < n)
        {
                x <<= 1;
        }
        return x;
}

void bindTexture(GPU_Renderer* renderer, GPU_Image* image)
{
    // Bind the texture to which subsequent calls refer
    if(image != ((RendererData_OpenGL*)renderer->data)->last_image)
    {
        GLuint handle = ((ImageData_OpenGL*)image->data)->handle;
        renderer->FlushBlitBuffer(renderer);
        
        glBindTexture( GL_TEXTURE_2D, handle );
        ((RendererData_OpenGL*)renderer->data)->last_image = image;
    }
}

static inline void flushAndBindTexture(GPU_Renderer* renderer, GLuint handle)
{
    // Bind the texture to which subsequent calls refer
    renderer->FlushBlitBuffer(renderer);
    
    glBindTexture( GL_TEXTURE_2D, handle );
    ((RendererData_OpenGL*)renderer->data)->last_image = NULL;
}

// Returns false if it can't be bound
Uint8 bindFramebuffer(GPU_Renderer* renderer, GPU_Target* target)
{
    if(enabled_features & GPU_FEATURE_RENDER_TARGETS)
    {
        // Bind the FBO
        if(target != ((RendererData_OpenGL*)renderer->data)->last_target)
        {
            GLuint handle = ((TargetData_OpenGL*)target->data)->handle;
            renderer->FlushBlitBuffer(renderer);
            
            extBindFramebuffer(handle);
            ((RendererData_OpenGL*)renderer->data)->last_target = target;
        }
        return 1;
    }
    else
    {
        return (target == renderer->display);
    }
}

static inline void flushAndBindFramebuffer(GPU_Renderer* renderer, GLuint handle)
{
    // Bind the FBO
    renderer->FlushBlitBuffer(renderer);
    
    extBindFramebuffer(handle);
    ((RendererData_OpenGL*)renderer->data)->last_target = NULL;
}

static inline void flushBlitBufferIfCurrentTexture(GPU_Renderer* renderer, GPU_Image* image)
{
    if(image == ((RendererData_OpenGL*)renderer->data)->last_image)
    {
        renderer->FlushBlitBuffer(renderer);
    }
}

static inline void flushAndClearBlitBufferIfCurrentTexture(GPU_Renderer* renderer, GPU_Image* image)
{
    if(image == ((RendererData_OpenGL*)renderer->data)->last_image)
    {
        renderer->FlushBlitBuffer(renderer);
        ((RendererData_OpenGL*)renderer->data)->last_image = NULL;
    }
}

static inline void flushBlitBufferIfCurrentFramebuffer(GPU_Renderer* renderer, GPU_Target* target)
{
    if(target == ((RendererData_OpenGL*)renderer->data)->last_target
       || ((RendererData_OpenGL*)renderer->data)->last_target == NULL)
    {
        renderer->FlushBlitBuffer(renderer);
    }
}

static inline void flushAndClearBlitBufferIfCurrentFramebuffer(GPU_Renderer* renderer, GPU_Target* target)
{
    if(target == ((RendererData_OpenGL*)renderer->data)->last_target
       || ((RendererData_OpenGL*)renderer->data)->last_target == NULL)
    {
        renderer->FlushBlitBuffer(renderer);
        ((RendererData_OpenGL*)renderer->data)->last_target = NULL;
    }
}

static GPU_Target* Init(GPU_Renderer* renderer, Uint16 w, Uint16 h, Uint32 flags)
{
#ifdef SDL_GPU_USE_SDL2
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#else
    Uint8 useDoubleBuffering = flags & SDL_DOUBLEBUF;
    if(useDoubleBuffering)
    {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        flags &= ~SDL_DOUBLEBUF;
    }
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
    flags |= SDL_OPENGL;
#endif


    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

#ifdef SDL_GPU_USE_SDL2
    SDL_Window* window = ((RendererData_OpenGL*)renderer->data)->window;
    if(window == NULL)
    {
        window = SDL_CreateWindow("",
                                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                              w, h,
                                              SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

        ((RendererData_OpenGL*)renderer->data)->window = window;
        if(window == NULL)
        {
            GPU_LogError("Window creation failed.\n");
            return NULL;
        }
        
        SDL_GLContext context = SDL_GL_CreateContext(window);
        ((RendererData_OpenGL*)renderer->data)->context = context;
    }

    SDL_GetWindowSize(window, &renderer->window_w, &renderer->window_h);

#else
    SDL_Surface* screen = SDL_SetVideoMode(w, h, 0, flags);

    if(screen == NULL)
        return NULL;

    renderer->window_w = screen->w;
    renderer->window_h = screen->h;
#endif

#ifdef SDL_GPU_USE_OPENGL
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        GPU_LogError("Failed to initialize: %s\n", glewGetErrorString(err));
    }

    checkExtension("GL_EXT_framebuffer_object");
    checkExtension("GL_ARB_framebuffer_object"); // glGenerateMipmap
    checkExtension("GL_EXT_framebuffer_blit");
    
#elif defined(SDL_GPU_USE_OPENGLES)
	
    checkExtension("GL_OES_framebuffer_object");
    checkExtension("GL_OES_blend_func_separate");
    checkExtension("GL_OES_blend_subtract");  // for glBlendEquationOES
	
#endif
	
    initNPOT();
    initFBO();
    initBLEND();
    
    glEnable( GL_TEXTURE_2D );
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    glViewport( 0, 0, renderer->window_w, renderer->window_h);

    glClear( GL_COLOR_BUFFER_BIT );
    glColor4ub(255, 255, 255, 255);

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    glOrtho(0.0f, renderer->window_w, renderer->window_h, 0.0f, -1.0f, 1.0f);

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    glEnable( GL_TEXTURE_2D );
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderer->SetBlending(renderer, 1);


    if(renderer->display == NULL)
        renderer->display = (GPU_Target*)malloc(sizeof(GPU_Target));

    renderer->display->image = NULL;
    renderer->display->data = (TargetData_OpenGL*)malloc(sizeof(TargetData_OpenGL));

    ((TargetData_OpenGL*)renderer->display->data)->handle = 0;
    renderer->display->renderer = renderer;
    renderer->display->w = renderer->window_w;
    renderer->display->h = renderer->window_h;

    renderer->display->useClip = 0;
    renderer->display->clipRect.x = 0;
    renderer->display->clipRect.y = 0;
    renderer->display->clipRect.w = renderer->display->w;
    renderer->display->clipRect.h = renderer->display->h;
    
    RendererData_OpenGL* rdata = (RendererData_OpenGL*)renderer->data;
    rdata->blit_buffer_max_size = GPU_BLIT_BUFFER_INIT_MAX_SIZE;
    rdata->blit_buffer_size = 0;
    int numFloatsPerVertex = 5;  // x, y, z, s, t
    rdata->blit_buffer = (float*)malloc(sizeof(float)*rdata->blit_buffer_max_size*numFloatsPerVertex);

    return renderer->display;
}


static Uint8 IsFeatureEnabled(GPU_Renderer* renderer, GPU_FeatureEnum feature)
{
    return ((enabled_features & feature) == feature);
}


static void SetAsCurrent(GPU_Renderer* renderer)
{
#ifdef SDL_GPU_USE_SDL2
    SDL_GL_MakeCurrent(((RendererData_OpenGL*)renderer->data)->window, ((RendererData_OpenGL*)renderer->data)->context);
#endif
}

// TODO: Rename to SetWindowResolution
static int SetDisplayResolution(GPU_Renderer* renderer, Uint16 w, Uint16 h)
{
    if(renderer->display == NULL)
        return 0;

#ifdef SDL_GPU_USE_SDL2
    SDL_SetWindowSize(((RendererData_OpenGL*)renderer->data)->window, w, h);
    SDL_GetWindowSize(((RendererData_OpenGL*)renderer->data)->window, &renderer->window_w, &renderer->window_h);
#else
    SDL_Surface* surf = SDL_GetVideoSurface();
    Uint32 flags = surf->flags;


    SDL_Surface* screen = SDL_SetVideoMode(w, h, 0, flags);
    // There's a bug in SDL.  This is a workaround.  Let's resize again:
    screen = SDL_SetVideoMode(w, h, 0, flags);

    if(screen == NULL)
        return 0;

    renderer->window_w = screen->w;
    renderer->window_h = screen->h;
#endif

    Uint16 virtualW = renderer->display->w;
    Uint16 virtualH = renderer->display->h;

    glEnable( GL_TEXTURE_2D );
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    glViewport( 0, 0, w, h);

    glClear( GL_COLOR_BUFFER_BIT );
    glColor4ub(255, 255, 255, 255);

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    glOrtho(0.0f, virtualW, virtualH, 0.0f, -1.0f, 1.0f);

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    glEnable( GL_TEXTURE_2D );
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_BLEND);

    // Update display
    GPU_ClearClip(renderer->display);

    return 1;
}

static void SetVirtualResolution(GPU_Renderer* renderer, Uint16 w, Uint16 h)
{
    if(renderer->display == NULL)
        return;

    renderer->display->w = w;
    renderer->display->h = h;
    
    renderer->FlushBlitBuffer(renderer);

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    glOrtho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    glMatrixMode( GL_MODELVIEW );
}

static void Quit(GPU_Renderer* renderer)
{
    free(renderer->display);
    renderer->display = NULL;
}



static int ToggleFullscreen(GPU_Renderer* renderer)
{
#ifdef SDL_GPU_USE_SDL2
    Uint8 enable = !(SDL_GetWindowFlags(((RendererData_OpenGL*)renderer->data)->window) & SDL_WINDOW_FULLSCREEN);

    if(SDL_SetWindowFullscreen(((RendererData_OpenGL*)renderer->data)->window, enable) < 0)
        return 0;

    return 1;
#else
    SDL_Surface* surf = SDL_GetVideoSurface();

    if(SDL_WM_ToggleFullScreen(surf))
        return 1;

    Uint16 w = surf->w;
    Uint16 h = surf->h;
    surf->flags ^= SDL_FULLSCREEN;
    return SetDisplayResolution(renderer, w, h);
#endif
}



static GPU_Camera SetCamera(GPU_Renderer* renderer, GPU_Target* screen, GPU_Camera* cam)
{
    if(screen == NULL)
        return renderer->camera;

    GPU_Camera result = renderer->camera;

    if(cam == NULL)
    {
        renderer->camera.x = 0.0f;
        renderer->camera.y = 0.0f;
        renderer->camera.z = -10.0f;
        renderer->camera.angle = 0.0f;
        renderer->camera.zoom = 1.0f;
    }
    else
    {
        renderer->camera = *cam;
    }
    
    renderer->FlushBlitBuffer(renderer);

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    // I want to use x, y, and z
    // The default z for objects is 0
    // The default z for the camera is -10. (should be neg)

    /*float fieldOfView = 60.0f;
    float fW = screen->w/2;
    float fH = screen->h/2;
    float aspect = fW/fH;
    float zNear = atan(fH)/((float)(fieldOfView / 360.0f * 3.14159f));
    float zFar = 255.0f;
    glFrustum( 0.0f + renderer->camera.x, 2*fW + renderer->camera.x, 2*fH + renderer->camera.y, 0.0f + renderer->camera.y, zNear, zFar );*/

    glFrustum(0.0f + renderer->camera.x, screen->w + renderer->camera.x, screen->h + renderer->camera.y, 0.0f + renderer->camera.y, 0.01f, 1.01f);

    //glMatrixMode( GL_MODELVIEW );
    //glLoadIdentity();


    float offsetX = screen->w/2.0f;
    float offsetY = screen->h/2.0f;
    glTranslatef(offsetX, offsetY, -0.01);
    glRotatef(renderer->camera.angle, 0, 0, 1);
    glTranslatef(-offsetX, -offsetY, 0);

    glTranslatef(renderer->camera.x + offsetX, renderer->camera.y + offsetY, 0);
    glScalef(renderer->camera.zoom, renderer->camera.zoom, 1.0f);
    glTranslatef(-renderer->camera.x - offsetX, -renderer->camera.y - offsetY, 0);

    return result;
}


static GPU_Image* CreateUninitializedImage(GPU_Renderer* renderer, Uint16 w, Uint16 h, Uint8 channels)
{
    if(channels < 3 || channels > 4)
    {
        GPU_LogError("GPU_CreateUninitializedImage() could not create an image with %d color channels.  Try 3 or 4 instead.\n", channels);
        return NULL;
    }
        
        GLuint handle;
        GLenum format;
        if(channels == 3)
                format = GL_RGB;
        else
                format = GL_RGBA;
        
        glGenTextures( 1, &handle );
    if(handle == 0)
    {
        GPU_LogError("GPU_CreateUninitializedImage() failed to generate a texture handle.\n");
        return NULL;
    }
    
    flushAndBindTexture( renderer, handle );

    // Set the texture's stretching properties
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

    GPU_Image* result = (GPU_Image*)malloc(sizeof(GPU_Image));
    ImageData_OpenGL* data = (ImageData_OpenGL*)malloc(sizeof(ImageData_OpenGL));
    result->target = NULL;
    result->data = data;
    result->renderer = renderer;
    result->channels = channels;
    data->handle = handle;
    data->format = format;
    data->hasMipmaps = 0;

    result->w = w;
    result->h = h;
    // POT textures will change this later
    data->tex_w = w;
    data->tex_h = h;

    return result;
}


static GPU_Image* CreateImage(GPU_Renderer* renderer, Uint16 w, Uint16 h, Uint8 channels)
{
    if(channels < 3 || channels > 4)
    {
        GPU_LogError("GPU_CreateImage() could not create an image with %d color channels.  Try 3 or 4 instead.\n", channels);
        return NULL;
    }

    GPU_Image* result = CreateUninitializedImage(renderer, w, h, channels);
    
    if(result == NULL)
    {
        GPU_LogError("GPU_CreateImage() could not create %ux%ux%u image.\n", w, h, channels);
        return NULL;
    }
    
    glEnable(GL_TEXTURE_2D);
    bindTexture(renderer, result);
    
    GLenum internal_format = ((ImageData_OpenGL*)(result->data))->format;
    w = result->w;
    h = result->h;
    if(!(enabled_features & GPU_FEATURE_NON_POWER_OF_TWO))
    {
        if(!isPowerOfTwo(w))
            w = getNearestPowerOf2(w);
        if(!isPowerOfTwo(h))
            h = getNearestPowerOf2(h);
    }
    
    // Initialize texture
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, 
                                   internal_format, GL_UNSIGNED_BYTE, NULL);
    // Tell SDL_gpu what we got.
    ((ImageData_OpenGL*)(result->data))->tex_w = w;
    ((ImageData_OpenGL*)(result->data))->tex_h = h;
    
    return result;
}

static GPU_Image* LoadImage(GPU_Renderer* renderer, const char* filename)
{
    SDL_Surface* surface = GPU_LoadSurface(filename);
    if(surface == NULL)
    {
        GPU_LogError("Failed to load image \"%s\"\n", filename);
        return NULL;
    }
    
    GPU_Image* result = renderer->CopyImageFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    return result;
}


static void readTexPixels(GPU_Renderer* renderer, GPU_Target* source, unsigned int width, unsigned int height, GLint format, GLubyte* pixels)
{
    flushBlitBufferIfCurrentFramebuffer(renderer, source);
    if(bindFramebuffer(renderer, source))
        glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, pixels);
}

static unsigned char* getRawImageData(GPU_Renderer* renderer, GPU_Image* image)
{
    unsigned char* data = (unsigned char*)malloc(image->w * image->h * image->channels);

    GPU_Target* tgt = GPU_LoadTarget(image);
    readTexPixels(renderer, tgt, image->w, image->h, ((ImageData_OpenGL*)image->data)->format, data);
    GPU_FreeTarget(tgt);

    return data;
}


static Uint8 SaveImage(GPU_Renderer* renderer, GPU_Image* image, const char* filename)
{
    const char* extension;
    Uint8 result;
    unsigned char* data;
    
	if(image == NULL || filename == NULL ||
       image->w < 1 || image->h < 1 || image->channels < 1 || image->channels > 4)
	{
		return 0;
	}

    if(strlen(filename) < 5)
    {
        GPU_LogError("GPU_SaveImage() failed: Unsupported format.\n");
        return 0;
    }

    extension = filename + strlen(filename)-1 - 3;

    /* FIXME: Doesn't support length 4 extensions yet */
    if(extension[0] != '.')
    {
        GPU_LogError("GPU_SaveImage() failed: Unsupported format.\n");
        return 0;
    }

    data = getRawImageData(renderer, image);
    
    if(data == NULL)
    {
        GPU_LogError("GPU_SaveImage() failed: Could not retrieve image data.\n");
        return 0;
    }

    extension++;
    if(strcasecmp(extension, "png") == 0)
        result = stbi_write_png(filename, image->w, image->h, image->channels, (const unsigned char *const)data, 0);
    else if(strcasecmp(extension, "bmp") == 0)
        result = stbi_write_bmp(filename, image->w, image->h, image->channels, (void*)data);
    else if(strcasecmp(extension, "tga") == 0)
        result = stbi_write_tga(filename, image->w, image->h, image->channels, (void*)data);
    //else if(strcasecmp(extension, "dds") == 0)
    //    result = stbi_write_dds(filename, image->w, image->h, image->channels, (const unsigned char *const)data);
    else
    {
        GPU_LogError("GPU_SaveImage() failed: Unsupported format (%s).\n", extension);
        result = 0;
    }

    free(data);
    return result;
}

static GPU_Image* CopyImage(GPU_Renderer* renderer, GPU_Image* image)
{
    GLboolean old_blend;
    glGetBooleanv(GL_BLEND, &old_blend);
    if(old_blend)
        renderer->SetBlending(renderer, 0);

    GPU_Image* result = renderer->CreateImage(renderer, image->w, image->h, image->channels);

    GPU_Target* tgt = renderer->LoadTarget(renderer, result);

    // Clear the clipRect
    SDL_Rect clip;
    Uint8 useClip = tgt->useClip;
    if(useClip)
    {
        clip = tgt->clipRect;
        GPU_ClearClip(tgt);
    }

    Uint16 w = renderer->display->w;
    Uint16 h = renderer->display->h;

    renderer->SetVirtualResolution(renderer, renderer->window_w, renderer->window_h);
    renderer->Blit(renderer, image, NULL, tgt, tgt->w/2, tgt->h/2);
    renderer->SetVirtualResolution(renderer, w, h);

    renderer->FreeTarget(renderer, tgt);

    if(useClip)
    {
        tgt->useClip = 1;
        tgt->clipRect = clip;
    }


    if(old_blend)
        renderer->SetBlending(renderer, 1);

    return result;
}




// Returns 0 if a direct conversion is safe.  Returns 1 if a copy is needed.  Returns -1 on error.
static int compareFormats(GLenum glFormat, SDL_Surface* surface, GLenum* surfaceFormatResult)
{
    SDL_PixelFormat* format = surface->format;
    switch(glFormat)
    {
        case GL_RGB:
            #ifdef GL_BGR
        case GL_BGR:
            #endif
            //GPU_LogError("Wanted 3 channels, got %d\n", format->BytesPerPixel);
            if(format->BytesPerPixel != 3)
                                return 1;
			
			if(format->Rmask == 0x0000FF && 
				format->Gmask == 0x00FF00 && 
				format->Bmask == 0xFF0000)
			{
				if(surfaceFormatResult != NULL)
                                        *surfaceFormatResult = GL_RGB;
                                return 0;
                        }
            #ifdef GL_BGR
            if(format->Rmask == 0xFF0000 && 
                                format->Gmask == 0x00FF00 && 
                                format->Bmask == 0x0000FF)
			{
				if(surfaceFormatResult != NULL)
                                        *surfaceFormatResult = GL_BGR;
                                return 0;
                        }
                        #endif
            //GPU_LogError("Masks don't match\n");
                        return 1;
                        
        case GL_RGBA:
            #ifdef GL_BGRA
        case GL_BGRA:
            #endif
            //GPU_LogError("Wanted 4 channels, got %d\n", format->BytesPerPixel);
            if(format->BytesPerPixel != 4)
                                return 1;
			
			if(format->Rmask == 0x000000FF && 
				format->Gmask == 0x0000FF00 && 
				format->Bmask == 0x00FF0000)
			{
				if(surfaceFormatResult != NULL)
					*surfaceFormatResult = GL_RGBA;
				return 0;
			}
			if(format->Rmask == 0x0000FF && 
				format->Gmask == 0x00FF00 && 
				format->Bmask == 0xFF0000)
			{
				if(surfaceFormatResult != NULL)
                                        *surfaceFormatResult = GL_RGBA;
                                return 0;
                        }
            #ifdef GL_BGRA
                        if(format->Rmask == 0xFF000000 && 
                                format->Gmask == 0x00FF0000 && 
                                format->Bmask == 0x0000FF00)
			{
				if(surfaceFormatResult != NULL)
					*surfaceFormatResult = GL_BGRA;
				return 0;
			}
			if(format->Rmask == 0xFF0000 && 
				format->Gmask == 0x00FF00 && 
				format->Bmask == 0x0000FF)
			{
				if(surfaceFormatResult != NULL)
                                        *surfaceFormatResult = GL_BGRA;
                                return 0;
                        }
                        #endif
            //GPU_LogError("Masks don't match: %X, %X, %X\n", format->Rmask, format->Gmask, format->Bmask);
                        return 1;
        default:
            GPU_LogError("GPU_UpdateImage() was passed an image with an invalid format.\n");
            return -1;
    }
}


// From SDL_AllocFormat()
static SDL_PixelFormat* AllocFormat(GLenum glFormat)
{
    // Yes, I need to do the whole thing myself... :(
    int channels;
    Uint32 Rmask, Gmask, Bmask, Amask, mask;
    
    switch(glFormat)
    {
        case GL_RGB:
            channels = 3;
            Rmask = 0x0000FF;
            Gmask = 0x00FF00;
            Bmask = 0xFF0000;
            break;
            #ifdef GL_BGR
        case GL_BGR:
            channels = 3;
            Rmask = 0xFF0000;
            Gmask = 0x00FF00;
            Bmask = 0x0000FF;
            break;
            #endif
        case GL_RGBA:
            channels = 4;
            Rmask = 0x000000FF;
            Gmask = 0x0000FF00;
            Bmask = 0x00FF0000;
            Amask = 0xFF000000;
            break;
            #ifdef GL_BGRA
        case GL_BGRA:
            channels = 4;
            Rmask = 0xFF000000;
            Gmask = 0x00FF0000;
            Bmask = 0x0000FF00;
            Amask = 0x000000FF;
            break;
            #endif
        default:
            return NULL;
    }
    
    SDL_PixelFormat* result = (SDL_PixelFormat*)malloc(sizeof(SDL_PixelFormat));
    memset(result, 0, sizeof(SDL_PixelFormat));
    
	result->BitsPerPixel = 8*channels;
	result->BytesPerPixel = channels;
	
    result->Rmask = Rmask;
    result->Rshift = 0;
    result->Rloss = 8;
    if (Rmask) {
        for (mask = Rmask; !(mask & 0x01); mask >>= 1)
            ++result->Rshift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Rloss;
    }

    result->Gmask = Gmask;
    result->Gshift = 0;
    result->Gloss = 8;
    if (Gmask) {
        for (mask = Gmask; !(mask & 0x01); mask >>= 1)
            ++result->Gshift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Gloss;
    }

    result->Bmask = Bmask;
    result->Bshift = 0;
    result->Bloss = 8;
    if (Bmask) {
        for (mask = Bmask; !(mask & 0x01); mask >>= 1)
            ++result->Bshift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Bloss;
    }

    result->Amask = Amask;
    result->Ashift = 0;
    result->Aloss = 8;
    if (Amask) {
        for (mask = Amask; !(mask & 0x01); mask >>= 1)
            ++result->Ashift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Aloss;
    }

    return result;
}

static Uint8 hasColorkey(SDL_Surface* surface)
{
    #ifdef SDL_GPU_USE_SDL2
    return (SDL_GetColorKey(surface, NULL) == 0);
    #else
    return (surface->flags & SDL_SRCCOLORKEY);
    #endif
}

static void FreeFormat(SDL_PixelFormat* format)
{
    free(format);
}

// Returns NULL on failure.  Returns the original surface if no copy is needed.  Returns a new surface converted to the right format otherwise.
static SDL_Surface* copySurfaceIfNeeded(GLenum glFormat, SDL_Surface* surface, GLenum* surfaceFormatResult)
{
	// If format doesn't match, we need to do a copy
    int format_compare = compareFormats(glFormat, surface, surfaceFormatResult);
	
	// There's a problem
    if(format_compare < 0)
		return NULL;
    
    // Copy it
    if(format_compare > 0)
    {
		// Convert to the right format
        SDL_PixelFormat* dst_fmt = AllocFormat(glFormat);
        surface = SDL_ConvertSurface(surface, dst_fmt, 0);
        FreeFormat(dst_fmt);
		if(surfaceFormatResult != NULL && surface != NULL)
			*surfaceFormatResult = glFormat;
    }
    
    // No copy needed
    return surface;
}

// From SDL_UpdateTexture()
static void UpdateImage(GPU_Renderer* renderer, GPU_Image* image, const SDL_Rect* rect, SDL_Surface* surface)
{
    if(renderer == NULL || image == NULL || surface == NULL)
        return;
    
    ImageData_OpenGL* data = (ImageData_OpenGL*)image->data;
    
    SDL_Surface* newSurface = copySurfaceIfNeeded(data->format, surface, NULL);
    if(newSurface == NULL)
    {
        GPU_LogError("GPU_UpdateImage() failed to convert surface to proper pixel format.\n");
        return;
    }
                
        
    SDL_Rect updateRect;
    if(rect != NULL)
        updateRect = *rect;
    else
    {
        updateRect.x = 0;
        updateRect.y = 0;
        updateRect.w = newSurface->w;
        updateRect.h = newSurface->h;
    }
        
    
    glEnable(GL_TEXTURE_2D);
    if(image->target != NULL)
        flushBlitBufferIfCurrentFramebuffer(renderer, image->target);
    bindTexture(renderer, image);
    int alignment = 1;
    if(newSurface->format->BytesPerPixel == 4)
        alignment = 4;
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    //glPixelStorei(GL_UNPACK_ROW_LENGTH,
    //                          (newSurface->pitch / newSurface->format->BytesPerPixel));
    glTexSubImage2D(GL_TEXTURE_2D, 0, updateRect.x, updateRect.y, updateRect.w,
                                updateRect.h, data->format, GL_UNSIGNED_BYTE,
                                newSurface->pixels);
    
        // Delete temporary surface
    if(surface != newSurface)
        SDL_FreeSurface(newSurface);
}

// From SDL_UpdateTexture()
static int InitImageWithSurface(GPU_Renderer* renderer, GPU_Image* image, SDL_Surface* surface)
{
    ImageData_OpenGL* data = (ImageData_OpenGL*)image->data;
    if(renderer == NULL || image == NULL || surface == NULL)
        return 0;
    
    GLenum internal_format = data->format;
    GLenum original_format = internal_format;
	
    SDL_Surface* newSurface = copySurfaceIfNeeded(internal_format, surface, &original_format);
    if(newSurface == NULL)
	{
        GPU_LogError("GPU_InitImageWithSurface() failed to convert surface to proper pixel format.\n");
                return 0;
        }
    
    Uint8 need_power_of_two_upload = 0;
    unsigned int w = newSurface->w;
    unsigned int h = newSurface->h;
    if(!(enabled_features & GPU_FEATURE_NON_POWER_OF_TWO))
    {
        if(!isPowerOfTwo(w))
        {
            w = getNearestPowerOf2(w);
            need_power_of_two_upload = 1;
        }
        if(!isPowerOfTwo(h))
        {
            h = getNearestPowerOf2(h);
            need_power_of_two_upload = 1;
        }
    }
    
    glEnable(GL_TEXTURE_2D);
    bindTexture(renderer, image);
    int alignment = 1;
    if(newSurface->format->BytesPerPixel == 4)
        alignment = 4;
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    //glPixelStorei(GL_UNPACK_ROW_LENGTH,
    //                          (newSurface->pitch / newSurface->format->BytesPerPixel));
    if(!need_power_of_two_upload)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, newSurface->w, newSurface->h, 0, 
                                       original_format, GL_UNSIGNED_BYTE, newSurface->pixels);
    }
    else
    {
        // Create POT texture
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, 
                                       original_format, GL_UNSIGNED_BYTE, NULL);
        
        // Upload NPOT data
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, newSurface->w, newSurface->h,
                                          original_format, GL_UNSIGNED_BYTE, newSurface->pixels);
        
        // Tell everyone what we did.
        ((ImageData_OpenGL*)(image->data))->tex_w = w;
        ((ImageData_OpenGL*)(image->data))->tex_h = h;
    }
    
        // Delete temporary surface
    if(surface != newSurface)
        SDL_FreeSurface(newSurface);
    return 1;
}

// From SDL_CreateTextureFromSurface
static GPU_Image* CopyImageFromSurface(GPU_Renderer* renderer, SDL_Surface* surface)
{
    const SDL_PixelFormat *fmt;
    Uint8 needAlpha;
    GPU_Image* image;
    int channels;

    if(renderer == NULL)
        return NULL;

    if (!surface) {
        GPU_LogError("GPU_CopyImageFromSurface() passed NULL surface.\n");
        return NULL;
    }

    /* See what the best texture format is */
    fmt = surface->format;
    if (fmt->Amask || hasColorkey(surface)) {
        needAlpha = 1;
    } else {
        needAlpha = 0;
    }
    
    // Get appropriate storage format
    // TODO: More options would be nice...
    if(needAlpha)
    {
        channels = 4;
    }
    else
    {
        channels = 3;
    }
    
    image = CreateUninitializedImage(renderer, surface->w, surface->h, channels);
    if(image == NULL)
        return NULL;

    if(SDL_MUSTLOCK(surface))
    {
        SDL_LockSurface(surface);
        InitImageWithSurface(renderer, image, surface);
        SDL_UnlockSurface(surface);
    }
    else
        InitImageWithSurface(renderer, image, surface);

    return image;
}


static void FreeImage(GPU_Renderer* renderer, GPU_Image* image)
{
    if(image == NULL)
        return;
    
    // Delete the attached target first
    if(image->target != NULL)
        renderer->FreeTarget(renderer, image->target);
    
    flushAndClearBlitBufferIfCurrentTexture(renderer, image);
    glDeleteTextures( 1, &((ImageData_OpenGL*)image->data)->handle);
    free(image->data);
    free(image);
}



static void SubSurfaceCopy(GPU_Renderer* renderer, SDL_Surface* src, SDL_Rect* srcrect, GPU_Target* dest, Sint16 x, Sint16 y)
{
    if(renderer == NULL || src == NULL || dest == NULL || dest->image == NULL)
        return;

    if(renderer != dest->renderer)
        return;

    SDL_Rect r;
    if(srcrect != NULL)
        r = *srcrect;
    else
    {
        r.x = 0;
        r.y = 0;
        r.w = src->w;
        r.h = src->h;
    }

    bindTexture(renderer, dest->image);

    //GLenum texture_format = GL_RGBA;//((ImageData_OpenGL*)image->data)->format;

    SDL_Surface* temp = SDL_CreateRGBSurface(SDL_SWSURFACE, r.w, r.h, src->format->BitsPerPixel, src->format->Rmask, src->format->Gmask, src->format->Bmask, src->format->Amask);

    if(temp == NULL)
    {
        GPU_LogError("GPU_SubSurfaceCopy(): Failed to create new %dx%d RGB surface.\n", r.w, r.h);
        return;
    }

    // Copy data to new surface
#ifdef SDL_GPU_USE_SDL2
    SDL_BlendMode blendmode;
    SDL_GetSurfaceBlendMode(src, &blendmode);
    SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
#else
    Uint32 srcAlpha = src->flags & SDL_SRCALPHA;
    SDL_SetAlpha(src, 0, src->format->alpha);
#endif

    SDL_BlitSurface(src, &r, temp, NULL);

#ifdef SDL_GPU_USE_SDL2
    SDL_SetSurfaceBlendMode(src, blendmode);
#else
    SDL_SetAlpha(src, srcAlpha, src->format->alpha);
#endif

    // Make surface into an image
    GPU_Image* image = GPU_CopyImageFromSurface(temp);
    if(image == NULL)
    {
        GPU_LogError("GPU_SubSurfaceCopy(): Failed to create new image texture.\n");
        return;
    }

    // Copy image to dest
    Uint8 blending = GPU_GetBlending();
    GPU_SetBlending(0);
    GPU_Blit(image, NULL, dest, x + r.w/2, y + r.h/2);
    GPU_SetBlending(blending);

    // Using glTexSubImage might be more efficient
    //glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, r.w, r.h, texture_format, GL_UNSIGNED_BYTE, buffer);

    GPU_FreeImage(image);

    SDL_FreeSurface(temp);
}

static GPU_Target* GetDisplayTarget(GPU_Renderer* renderer)
{
    return renderer->display;
}


static GPU_Target* LoadTarget(GPU_Renderer* renderer, GPU_Image* image)
{
    if(renderer == NULL || image == NULL)
        return NULL;

    if(image->target != NULL)
        return image->target;
        
    if(!(enabled_features & GPU_FEATURE_RENDER_TARGETS))
        return NULL;
    
    GLuint handle;
    // Create framebuffer object
    glGenFramebuffers(1, &handle);
    flushAndBindFramebuffer(renderer, handle);

    // Attach the texture to it
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ((ImageData_OpenGL*)image->data)->handle, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
        return NULL;

    GPU_Target* result = (GPU_Target*)malloc(sizeof(GPU_Target));
    TargetData_OpenGL* data = (TargetData_OpenGL*)malloc(sizeof(TargetData_OpenGL));
    result->data = data;
    data->handle = handle;
    data->format = ((ImageData_OpenGL*)image->data)->format;
    result->renderer = renderer;
    result->image = image;
    result->w = image->w;
    result->h = image->h;

    result->useClip = 0;
    result->clipRect.x = 0;
    result->clipRect.y = 0;
    result->clipRect.w = image->w;
    result->clipRect.h = image->h;
    
    image->target = result;
    return result;
}



static void FreeTarget(GPU_Renderer* renderer, GPU_Target* target)
{
    if(target == NULL || target == renderer->display)
        return;
    
    if(enabled_features & GPU_FEATURE_RENDER_TARGETS)
    {
        flushAndClearBlitBufferIfCurrentFramebuffer(renderer, target);
        glDeleteFramebuffers(1, &((TargetData_OpenGL*)target->data)->handle);
    }
    
    if(target->image != NULL)
        target->image->target = NULL;  // Remove reference to this object
    free(target->data);
    free(target);
}



static int Blit(GPU_Renderer* renderer, GPU_Image* src, SDL_Rect* srcrect, GPU_Target* dest, float x, float y)
{
    if(src == NULL || dest == NULL)
        return -1;
    if(renderer != src->renderer || renderer != dest->renderer)
        return -2;


    // Bind the texture to which subsequent calls refer
    bindTexture(renderer, src);

    // Bind the FBO
    if(bindFramebuffer(renderer, dest))
    {
        Uint16 tex_w = ((ImageData_OpenGL*)src->data)->tex_w;
        Uint16 tex_h = ((ImageData_OpenGL*)src->data)->tex_h;

        float x1, y1, x2, y2;
        float dx1, dy1, dx2, dy2;
        if(srcrect == NULL)
        {
            // Scale tex coords according to actual texture dims
            x1 = 0.1f/tex_w;
            y1 = 0.1f/tex_h;
            x2 = ((float)src->w - 0.1f)/tex_w;
            y2 = ((float)src->h - 0.1f)/tex_h;
            // Center the image on the given coords
            dx1 = x - src->w/2.0f;
            dy1 = y - src->h/2.0f;
            dx2 = x + src->w/2.0f;
            dy2 = y + src->h/2.0f;
        }
        else
        {
            // Scale srcrect tex coords according to actual texture dims
            x1 = (srcrect->x + 0.1f)/(float)tex_w;
            y1 = (srcrect->y + 0.1f)/(float)tex_h;
            x2 = (srcrect->x + srcrect->w - 0.1f)/(float)tex_w;
            y2 = (srcrect->y + srcrect->h - 0.1f)/(float)tex_h;
            // Center the image on the given coords
            dx1 = x - srcrect->w/2.0f;
            dy1 = y - srcrect->h/2.0f;
            dx2 = x + srcrect->w/2.0f;
            dy2 = y + srcrect->h/2.0f;
        }
        
        RendererData_OpenGL* rdata = (RendererData_OpenGL*)renderer->data;
        float* blit_buffer = rdata->blit_buffer;
        
        if(rdata->blit_buffer_size + 6 >= rdata->blit_buffer_max_size)
            renderer->FlushBlitBuffer(renderer);
            
        int vert_index = GPU_BLIT_BUFFER_VERTEX_OFFSET + rdata->blit_buffer_size*GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        int tex_index = GPU_BLIT_BUFFER_TEX_COORD_OFFSET + rdata->blit_buffer_size*GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;

        blit_buffer[vert_index] = dx1;
        blit_buffer[vert_index+1] = dy1;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x1;
        blit_buffer[tex_index+1] = y1;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx2;
        blit_buffer[vert_index+1] = dy1;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x2;
        blit_buffer[tex_index+1] = y1;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx2;
        blit_buffer[vert_index+1] = dy2;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x2;
        blit_buffer[tex_index+1] = y2;
        
        
        // Second tri
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx1;
        blit_buffer[vert_index+1] = dy1;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x1;
        blit_buffer[tex_index+1] = y1;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx2;
        blit_buffer[vert_index+1] = dy2;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x2;
        blit_buffer[tex_index+1] = y2;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx1;
        blit_buffer[vert_index+1] = dy2;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x1;
        blit_buffer[tex_index+1] = y2;
        
        rdata->blit_buffer_size += 6;
    }

    return 0;
}


static int BlitRotate(GPU_Renderer* renderer, GPU_Image* src, SDL_Rect* srcrect, GPU_Target* dest, float x, float y, float angle)
{
    if(src == NULL || dest == NULL)
        return -1;
    
    return renderer->BlitTransformX(renderer, src, srcrect, dest, x, y, src->w/2.0f, src->h/2.0f, angle, 1.0f, 1.0f);
}

static int BlitScale(GPU_Renderer* renderer, GPU_Image* src, SDL_Rect* srcrect, GPU_Target* dest, float x, float y, float scaleX, float scaleY)
{
    if(src == NULL || dest == NULL)
        return -1;
    
    return renderer->BlitTransformX(renderer, src, srcrect, dest, x, y, src->w/2.0f, src->h/2.0f, 0.0f, scaleX, scaleY);
}

static int BlitTransform(GPU_Renderer* renderer, GPU_Image* src, SDL_Rect* srcrect, GPU_Target* dest, float x, float y, float angle, float scaleX, float scaleY)
{
    if(src == NULL || dest == NULL)
        return -1;
    
    return renderer->BlitTransformX(renderer, src, srcrect, dest, x, y, src->w/2.0f, src->h/2.0f, angle, scaleX, scaleY);
}

static int BlitTransformX(GPU_Renderer* renderer, GPU_Image* src, SDL_Rect* srcrect, GPU_Target* dest, float x, float y, float pivot_x, float pivot_y, float angle, float scaleX, float scaleY)
{
    if(src == NULL || dest == NULL)
        return -1;
    if(renderer != src->renderer || renderer != dest->renderer)
        return -2;


    // Bind the texture to which subsequent calls refer
    bindTexture(renderer, src);

    // Bind the FBO
    if(bindFramebuffer(renderer, dest))
    {
        Uint16 tex_w = ((ImageData_OpenGL*)src->data)->tex_w;
        Uint16 tex_h = ((ImageData_OpenGL*)src->data)->tex_h;

        float x1, y1, x2, y2;
        /*
            1,1 --- 3,3
             |       |
             |       |
            4,4 --- 2,2
        */
        float dx1, dy1, dx2, dy2, dx3, dy3, dx4, dy4;
        if(srcrect == NULL)
        {
            // Scale tex coords according to actual texture dims
            x1 = 0.1f/tex_w;
            y1 = 0.1f/tex_h;
            x2 = ((float)src->w - 0.1f)/tex_w;
            y2 = ((float)src->h - 0.1f)/tex_h;
            // Center the image on the given coords
            dx1 = -src->w/2.0f;
            dy1 = -src->h/2.0f;
            dx2 = src->w/2.0f;
            dy2 = src->h/2.0f;
        }
        else
        {
            // Scale srcrect tex coords according to actual texture dims
            x1 = (srcrect->x + 0.1f)/(float)tex_w;
            y1 = (srcrect->y + 0.1f)/(float)tex_h;
            x2 = (srcrect->x + srcrect->w - 0.1f)/(float)tex_w;
            y2 = (srcrect->y + srcrect->h - 0.1f)/(float)tex_h;
            // Center the image on the given coords
            dx1 = -srcrect->w/2.0f;
            dy1 = -srcrect->h/2.0f;
            dx2 = srcrect->w/2.0f;
            dy2 = srcrect->h/2.0f;
        }
        
        // Apply transforms
        
        // Scale
        if(scaleX != 1.0f || scaleY != 1.0f)
        {
            float w = (dx2 - dx1)*scaleX;
            float h = (dy2 - dy1)*scaleY;
            dx1 = (dx2 + dx1)/2 - w/2;
            dx2 = dx1 + w;
            dy1 = (dy2 + dy1)/2 - h/2;
            dy2 = dy1 + h;
        }
        
        // Shift away from the center (these are relative to the image corner)
        pivot_x -= src->w/2.0f;
        pivot_y -= src->h/2.0f;

        // Translate origin to pivot
        dx1 -= pivot_x*scaleX;
        dy1 -= pivot_y*scaleY;
        dx2 -= pivot_x*scaleX;
        dy2 -= pivot_y*scaleY;
        
        // Get extra vertices for rotation
        dx3 = dx2;
        dy3 = dy1;
        dx4 = dx1;
        dy4 = dy2;
        
        // Rotate about origin (the pivot)
        if(angle != 0.0f)
        {
            float cosA = cos(angle*M_PI/180);
            float sinA = sin(angle*M_PI/180);
            float tempX = dx1;
            dx1 = dx1*cosA - dy1*sinA;
            dy1 = tempX*sinA + dy1*cosA;
            tempX = dx2;
            dx2 = dx2*cosA - dy2*sinA;
            dy2 = tempX*sinA + dy2*cosA;
            tempX = dx3;
            dx3 = dx3*cosA - dy3*sinA;
            dy3 = tempX*sinA + dy3*cosA;
            tempX = dx4;
            dx4 = dx4*cosA - dy4*sinA;
            dy4 = tempX*sinA + dy4*cosA;
        }
        
        // Translate to pos
        dx1 += x;
        dx2 += x;
        dx3 += x;
        dx4 += x;
        dy1 += y;
        dy2 += y;
        dy3 += y;
        dy4 += y;
        
        RendererData_OpenGL* rdata = (RendererData_OpenGL*)renderer->data;
        float* blit_buffer = rdata->blit_buffer;
        
        if(rdata->blit_buffer_size + 6 >= rdata->blit_buffer_max_size)
            renderer->FlushBlitBuffer(renderer);
            
        int vert_index = GPU_BLIT_BUFFER_VERTEX_OFFSET + rdata->blit_buffer_size*GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        int tex_index = GPU_BLIT_BUFFER_TEX_COORD_OFFSET + rdata->blit_buffer_size*GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;

        blit_buffer[vert_index] = dx1;
        blit_buffer[vert_index+1] = dy1;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x1;
        blit_buffer[tex_index+1] = y1;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx3;
        blit_buffer[vert_index+1] = dy3;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x2;
        blit_buffer[tex_index+1] = y1;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx2;
        blit_buffer[vert_index+1] = dy2;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x2;
        blit_buffer[tex_index+1] = y2;
        
        
        // Second tri
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx1;
        blit_buffer[vert_index+1] = dy1;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x1;
        blit_buffer[tex_index+1] = y1;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx2;
        blit_buffer[vert_index+1] = dy2;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x2;
        blit_buffer[tex_index+1] = y2;
        
        vert_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        tex_index += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
        blit_buffer[vert_index] = dx4;
        blit_buffer[vert_index+1] = dy4;
        blit_buffer[vert_index+2] = 0.0f;
        blit_buffer[tex_index] = x1;
        blit_buffer[tex_index+1] = y2;
        
        rdata->blit_buffer_size += 6;
    }
    
    return 0;
}

static int BlitTransformMatrix(GPU_Renderer* renderer, GPU_Image* src, SDL_Rect* srcrect, GPU_Target* dest, float x, float y, float* matrix3x3)
{
    if(src == NULL || dest == NULL)
        return -1;
    if(renderer != src->renderer || renderer != dest->renderer)
        return -2;

    glPushMatrix();

    // column-major 3x3 to column-major 4x4 (and scooting the translations to the homogeneous column)
    float matrix[16] = {matrix3x3[0], matrix3x3[1], matrix3x3[2], 0,
                        matrix3x3[3], matrix3x3[4], matrix3x3[5], 0,
                        0,            0,            matrix3x3[8], 0,
                        matrix3x3[6], matrix3x3[7], 0,            1
                       };
    glTranslatef(x, y, 0);
    glMultMatrixf(matrix);

    int result = GPU_Blit(src, srcrect, dest, 0, 0);

    glPopMatrix();

    return result;
}

static float SetZ(GPU_Renderer* renderer, float z)
{
    if(renderer == NULL)
        return 0.0f;

    float oldZ = ((RendererData_OpenGL*)(renderer->data))->z;
    ((RendererData_OpenGL*)(renderer->data))->z = z;

    return oldZ;
}

static float GetZ(GPU_Renderer* renderer)
{
    if(renderer == NULL)
        return 0.0f;
    return ((RendererData_OpenGL*)(renderer->data))->z;
}

static void GenerateMipmaps(GPU_Renderer* renderer, GPU_Image* image)
{
    if(image == NULL)
        return;
        
    if(image->target != NULL)
        flushBlitBufferIfCurrentFramebuffer(renderer, image->target);
    bindTexture(renderer, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    ((ImageData_OpenGL*)image->data)->hasMipmaps = 1;

    GLint filter;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    if(filter == GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
}




static SDL_Rect SetClip(GPU_Renderer* renderer, GPU_Target* target, Sint16 x, Sint16 y, Uint16 w, Uint16 h)
{
	if(target == NULL)
	{
		SDL_Rect r = {0,0,0,0};
		return r;
	}
	
	flushBlitBufferIfCurrentFramebuffer(renderer, target);
	target->useClip = 1;
	
	SDL_Rect r = target->clipRect;
	
	target->clipRect.x = x;
	target->clipRect.y = y;
	target->clipRect.w = w;
	target->clipRect.h = h;
	
	return r;
}

static void ClearClip(GPU_Renderer* renderer, GPU_Target* target)
{
	if(target == NULL)
		return;
    
	flushBlitBufferIfCurrentFramebuffer(renderer, target);
	target->useClip = 0;
	target->clipRect.x = 0;
	target->clipRect.y = 0;
	target->clipRect.w = target->w;
	target->clipRect.h = target->h;
}




static Uint8 GetBlending(GPU_Renderer* renderer)
{
    return ((RendererData_OpenGL*)renderer->data)->blending;
}



static void SetBlending(GPU_Renderer* renderer, Uint8 enable)
{
    renderer->FlushBlitBuffer(renderer);
    
    if(enable)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    ((RendererData_OpenGL*)renderer->data)->blending = enable;
}


static void SetRGBA(GPU_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    renderer->FlushBlitBuffer(renderer);
    glColor4f(r/255.01f, g/255.01f, b/255.01f, a/255.01f);
}


static void ReplaceRGB(GPU_Renderer* renderer, GPU_Image* image, Uint8 from_r, Uint8 from_g, Uint8 from_b, Uint8 to_r, Uint8 to_g, Uint8 to_b)
{
    if(image == NULL || image->channels < 3)
        return;
    if(renderer != image->renderer)
        return;

    if(image->target != NULL)
        flushBlitBufferIfCurrentFramebuffer(renderer, image->target);
    bindTexture(renderer, image);

    GLint textureWidth, textureHeight;

    textureWidth = ((ImageData_OpenGL*)image->data)->tex_w;
    textureHeight = ((ImageData_OpenGL*)image->data)->tex_h;

    GLenum texture_format = ((ImageData_OpenGL*)image->data)->format;

    // FIXME: Does not take into account GL_PACK_ALIGNMENT
    GLubyte *buffer = (GLubyte *)malloc(textureWidth*textureHeight*image->channels);

    GPU_Target* tgt = GPU_LoadTarget(image);
    readTexPixels(renderer, tgt, textureWidth, textureHeight, texture_format, buffer);
    GPU_FreeTarget(tgt);

    int x,y,i;
    for(y = 0; y < textureHeight; y++)
    {
        for(x = 0; x < textureWidth; x++)
        {
            i = ((y*textureWidth) + x)*image->channels;
            if(buffer[i] == from_r && buffer[i+1] == from_g && buffer[i+2] == from_b)
            {
                buffer[i] = to_r;
                buffer[i+1] = to_g;
                buffer[i+2] = to_b;
            }
        }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, texture_format, GL_UNSIGNED_BYTE, buffer);


    free(buffer);
}

static void MakeRGBTransparent(GPU_Renderer* renderer, GPU_Image* image, Uint8 r, Uint8 g, Uint8 b)
{
    if(image == NULL || image->channels < 4)
        return;
    if(renderer != image->renderer)
        return;

    if(image->target != NULL)
        flushBlitBufferIfCurrentFramebuffer(renderer, image->target);
    bindTexture(renderer, image);

    GLint textureWidth, textureHeight;

    textureWidth = ((ImageData_OpenGL*)image->data)->tex_w;
    textureHeight = ((ImageData_OpenGL*)image->data)->tex_h;

    GLenum texture_format = ((ImageData_OpenGL*)image->data)->format;

    // FIXME: Does not take into account GL_PACK_ALIGNMENT
    GLubyte *buffer = (GLubyte *)malloc(textureWidth*textureHeight*4);

    GPU_Target* tgt = GPU_LoadTarget(image);
    readTexPixels(renderer, tgt, textureWidth, textureHeight, texture_format, buffer);
    GPU_FreeTarget(tgt);

    int x,y,i;
    for(y = 0; y < textureHeight; y++)
    {
        for(x = 0; x < textureWidth; x++)
        {
            i = ((y*textureWidth) + x)*4;
            if(buffer[i] == r && buffer[i+1] == g && buffer[i+2] == b)
                buffer[i+3] = 0;
        }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, texture_format, GL_UNSIGNED_BYTE, buffer);

    free(buffer);
}

#define MIN(a,b,c) (a<b && a<c? a : (b<c? b : c))
#define MAX(a,b,c) (a>b && a>c? a : (b>c? b : c))


static void rgb_to_hsv(int red, int green, int blue, int* hue, int* sat, int* val)
{
    float r = red/255.0f;
    float g = green/255.0f;
    float b = blue/255.0f;

    float h, s, v;

    float min, max, delta;
    min = MIN( r, g, b );
    max = MAX( r, g, b );

    v = max;				// v
    delta = max - min;
    if( max != 0 && min != max)
    {
        s = delta / max;		// s

        if( r == max )
            h = ( g - b ) / delta;		// between yellow & magenta
        else if( g == max )
            h = 2 + ( b - r ) / delta;	// between cyan & yellow
        else
            h = 4 + ( r - g ) / delta;	// between magenta & cyan
        h *= 60;				// degrees
        if( h < 0 )
            h += 360;
    }
    else {
        // r = g = b = 0		// s = 0, v is undefined
        s = 0;
        h = 0;// really undefined: -1
    }

    *hue = h * 256.0f/360.0f;
    *sat = s * 255;
    *val = v * 255;
}

static void hsv_to_rgb(int hue, int sat, int val, int* r, int* g, int* b)
{
    float h = hue/255.0f;
    float s = sat/255.0f;
    float v = val/255.0f;

    int H = floor(h*5.999f);
    float chroma = v*s;
    float x = chroma * (1 - fabs(fmod(h*5.999f, 2) - 1));

    unsigned char R = 0, G = 0, B = 0;
    switch(H)
    {
    case 0:
        R = 255*chroma;
        G = 255*x;
        break;
    case 1:
        R = 255*x;
        G = 255*chroma;
        break;
    case 2:
        G = 255*chroma;
        B = 255*x;
        break;
    case 3:
        G = 255*x;
        B = 255*chroma;
        break;
    case 4:
        R = 255*x;
        B = 255*chroma;
        break;
    case 5:
        R = 255*chroma;
        B = 255*x;
        break;
    }

    unsigned char m = 255*(v - chroma);

    *r = R+m;
    *g = G+m;
    *b = B+m;
}

static int clamp(int value, int low, int high)
{
    if(value <= low)
        return low;
    if(value >= high)
        return high;
    return value;
}


static void ShiftHSV(GPU_Renderer* renderer, GPU_Image* image, int hue, int saturation, int value)
{
    if(image == NULL || image->channels < 3)
        return;
    if(renderer != image->renderer)
        return;
    
    if(image->target != NULL)
        flushBlitBufferIfCurrentFramebuffer(renderer, image->target);
    bindTexture(renderer, image);

    GLint textureWidth, textureHeight;

    textureWidth = ((ImageData_OpenGL*)image->data)->tex_w;
    textureHeight = ((ImageData_OpenGL*)image->data)->tex_h;

    // FIXME: Does not take into account GL_PACK_ALIGNMENT
    GLubyte *buffer = (GLubyte *)malloc(textureWidth*textureHeight*image->channels);

    GLenum texture_format = ((ImageData_OpenGL*)image->data)->format;

    GPU_Target* tgt = GPU_LoadTarget(image);
    readTexPixels(renderer, tgt, textureWidth, textureHeight, texture_format, buffer);
    GPU_FreeTarget(tgt);

    int x,y,i;
    for(y = 0; y < textureHeight; y++)
    {
        for(x = 0; x < textureWidth; x++)
        {
            i = ((y*textureWidth) + x)*image->channels;

            if(image->channels == 4 && buffer[i+3] == 0)
                continue;

            int r = buffer[i];
            int g = buffer[i+1];
            int b = buffer[i+2];
            int h, s, v;
            rgb_to_hsv(r, g, b, &h, &s, &v);
            h += hue;
            s += saturation;
            v += value;
            // Wrap hue
            while(h < 0)
                h += 256;
            while(h > 255)
                h -= 256;

            // Clamp
            s = clamp(s, 0, 255);
            v = clamp(v, 0, 255);

            hsv_to_rgb(h, s, v, &r, &g, &b);

            buffer[i] = r;
            buffer[i+1] = g;
            buffer[i+2] = b;
        }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, texture_format, GL_UNSIGNED_BYTE, buffer);


    free(buffer);
}


static void ShiftHSVExcept(GPU_Renderer* renderer, GPU_Image* image, int hue, int saturation, int value, int notHue, int notSat, int notVal, int range)
{
    if(image == NULL || image->channels < 3)
        return;
    if(renderer != image->renderer)
        return;

    if(image->target != NULL)
        flushBlitBufferIfCurrentFramebuffer(renderer, image->target);
    bindTexture(renderer, image);

    GLint textureWidth, textureHeight;

    textureWidth = ((ImageData_OpenGL*)image->data)->tex_w;
    textureHeight = ((ImageData_OpenGL*)image->data)->tex_h;

    // FIXME: Does not take into account GL_PACK_ALIGNMENT
    GLubyte *buffer = (GLubyte *)malloc(textureWidth*textureHeight*image->channels);

    GLenum texture_format = ((ImageData_OpenGL*)image->data)->format;

    GPU_Target* tgt = GPU_LoadTarget(image);
    readTexPixels(renderer, tgt, textureWidth, textureHeight, texture_format, buffer);
    GPU_FreeTarget(tgt);

    int x,y,i;
    for(y = 0; y < textureHeight; y++)
    {
        for(x = 0; x < textureWidth; x++)
        {
            i = ((y*textureWidth) + x)*image->channels;

            if(image->channels == 4 && buffer[i+3] == 0)
                continue;

            int r = buffer[i];
            int g = buffer[i+1];
            int b = buffer[i+2];

            int h, s, v;
            rgb_to_hsv(r, g, b, &h, &s, &v);

            if(notHue - range <= h && notHue + range >= h
                    && notSat - range <= s && notSat + range >= s
                    && notVal - range <= v && notVal + range >= v)
                continue;

            h += hue;
            s += saturation;
            v += value;
            // Wrap hue
            while(h < 0)
                h += 256;
            while(h > 255)
                h -= 256;

            // Clamp
            s = clamp(s, 0, 255);
            v = clamp(v, 0, 255);

            hsv_to_rgb(h, s, v, &r, &g, &b);

            buffer[i] = r;
            buffer[i+1] = g;
            buffer[i+2] = b;
        }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, texture_format, GL_UNSIGNED_BYTE, buffer);


    free(buffer);
}




static SDL_Color GetPixel(GPU_Renderer* renderer, GPU_Target* target, Sint16 x, Sint16 y)
{
    SDL_Color result = {0,0,0,0};
    if(target == NULL)
        return result;
    if(renderer != target->renderer)
        return result;
    if(x < 0 || y < 0 || x >= target->w || y >= target->h)
        return result;

    flushBlitBufferIfCurrentFramebuffer(renderer, target);
    if(bindFramebuffer(renderer, target))
    {
        unsigned char pixels[4];
        glReadPixels(x, y, 1, 1, ((TargetData_OpenGL*)target->data)->format, GL_UNSIGNED_BYTE, pixels);

        result.r = pixels[0];
        result.g = pixels[1];
        result.b = pixels[2];
        #ifdef SDL_GPU_USE_SDL2
        result.a = pixels[3];
        #else
        result.unused = pixels[3];
        #endif
    }

    return result;
}

static void SetImageFilter(GPU_Renderer* renderer, GPU_Image* image, GPU_FilterEnum filter)
{
    if(image == NULL)
        return;
    if(renderer != image->renderer)
        return;

    bindTexture(renderer, image);

    GLenum minFilter = GL_NEAREST;
    GLenum magFilter = GL_NEAREST;

    if(filter == GPU_LINEAR)
    {
        if(((ImageData_OpenGL*)image->data)->hasMipmaps)
            minFilter = GL_LINEAR_MIPMAP_NEAREST;
        else
            minFilter = GL_LINEAR;

        magFilter = GL_LINEAR;
    }
    else if(filter == GPU_LINEAR_MIPMAP)
    {
        if(((ImageData_OpenGL*)image->data)->hasMipmaps)
            minFilter = GL_LINEAR_MIPMAP_LINEAR;
        else
            minFilter = GL_LINEAR;

        magFilter = GL_LINEAR;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
}


static void SetBlendMode(GPU_Renderer* renderer, GPU_BlendEnum mode)
{
    renderer->FlushBlitBuffer(renderer);
    
    if(mode == GPU_BLEND_NORMAL)
    {
    	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;  // TODO: Return false so we can avoid depending on it if it fails
    	glBlendEquation(GL_FUNC_ADD);
    }
    else if(mode == GPU_BLEND_MULTIPLY)
    {
        if(!(enabled_features & GPU_FEATURE_BLEND_FUNC_SEPARATE))
            return;
    	glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
    	glBlendEquation(GL_FUNC_ADD);
    }
    else if(mode == GPU_BLEND_ADD)
    {
    	glBlendFunc(GL_ONE, GL_ONE);
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
    	glBlendEquation(GL_FUNC_ADD);
    }
    else if(mode == GPU_BLEND_SUBTRACT)
    {
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
    	glBlendFunc(GL_ONE, GL_ONE);
    	glBlendEquation(GL_FUNC_SUBTRACT);
    }
    else if(mode == GPU_BLEND_ADD_COLOR)
    {
        if(!(enabled_features & GPU_FEATURE_BLEND_FUNC_SEPARATE))
            return;
    	glBlendFuncSeparate(GL_ONE, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
    	glBlendEquation(GL_FUNC_ADD);
    }
    else if(mode == GPU_BLEND_SUBTRACT_COLOR)
    {
        if(!(enabled_features & GPU_FEATURE_BLEND_FUNC_SEPARATE))
            return;
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
        glBlendEquation(GL_FUNC_SUBTRACT);
    }
    else if(mode == GPU_BLEND_DIFFERENCE)
    {
        if(!(enabled_features & GPU_FEATURE_BLEND_FUNC_SEPARATE))
            return;
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
    	glBlendEquation(GL_FUNC_SUBTRACT);
    }
    else if(mode == GPU_BLEND_PUNCHOUT)
    {
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
    	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
    }
    else if(mode == GPU_BLEND_CUTOUT)
    {
        if(!(enabled_features & GPU_FEATURE_BLEND_EQUATIONS))
            return;
    	glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
    	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
    }
}




SDL_Rect getViewport()
{
    int v[4];
    glGetIntegerv(GL_VIEWPORT, v);
    SDL_Rect r = {v[0], v[1], v[2], v[3]};
    return r;
}

void setViewport(SDL_Rect rect)
{
    glViewport(rect.x, rect.y, rect.w, rect.h);
}


static void Clear(GPU_Renderer* renderer, GPU_Target* target)
{
    if(target == NULL)
        return;
    if(renderer != target->renderer)
        return;

    flushBlitBufferIfCurrentFramebuffer(renderer, target);
    if(bindFramebuffer(renderer, target))
    {
        SDL_Rect viewport = getViewport();
        glViewport(0,0,target->w, target->h);

        if(target->useClip)
        {
            glEnable(GL_SCISSOR_TEST);
            int y = (renderer->display == target? renderer->display->h - (target->clipRect.y + target->clipRect.h) : target->clipRect.y);
            float xFactor = ((float)renderer->window_w)/renderer->display->w;
            float yFactor = ((float)renderer->window_h)/renderer->display->h;
            glScissor(target->clipRect.x * xFactor, y * yFactor, target->clipRect.w * xFactor, target->clipRect.h * yFactor);
        }

        //glPushAttrib(GL_COLOR_BUFFER_BIT);

        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);
        //glPopAttrib();
        glColor4ub(255, 255, 255, 255);

        if(target->useClip)
        {
            glDisable(GL_SCISSOR_TEST);
        }

        setViewport(viewport);
    }
}


static void ClearRGBA(GPU_Renderer* renderer, GPU_Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if(target == NULL)
        return;
    if(renderer != target->renderer)
        return;

    flushBlitBufferIfCurrentFramebuffer(renderer, target);
    if(bindFramebuffer(renderer, target))
    {
        SDL_Rect viewport = getViewport();
        glViewport(0,0,target->w, target->h);

        if(target->useClip)
        {
            glEnable(GL_SCISSOR_TEST);
            int y = (renderer->display == target? renderer->display->h - (target->clipRect.y + target->clipRect.h) : target->clipRect.y);
            float xFactor = ((float)renderer->window_w)/renderer->display->w;
            float yFactor = ((float)renderer->window_h)/renderer->display->h;
            glScissor(target->clipRect.x * xFactor, y * yFactor, target->clipRect.w * xFactor, target->clipRect.h * yFactor);
        }

        glClearColor(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glColor4ub(255, 255, 255, 255);

        if(target->useClip)
        {
            glDisable(GL_SCISSOR_TEST);
        }

        setViewport(viewport);
    }
}

static void FlushBlitBuffer(GPU_Renderer* renderer)
{
    RendererData_OpenGL* rdata = (RendererData_OpenGL*)renderer->data;
    if(rdata->blit_buffer_size > 0 && rdata->last_target != NULL && rdata->last_image != NULL)
    {
        Uint8 isRTT = (renderer->display != rdata->last_target);
        
        glEnable(GL_TEXTURE_2D);
        GPU_Target* dest = rdata->last_target;
        
        // Modify the viewport and projection matrix if rendering to a texture
        GLint vp[4];
        if(isRTT)
        {
            glGetIntegerv(GL_VIEWPORT, vp);

            unsigned int w = dest->w;
            unsigned int h = dest->h;

            glViewport( 0, 0, w, h);

            glMatrixMode( GL_PROJECTION );
            glPushMatrix();
            glLoadIdentity();

            glOrtho(0.0f, w, 0.0f, h, -1.0f, 1.0f); // Special inverted orthographic projection because tex coords are inverted already.

            glMatrixMode( GL_MODELVIEW );
        }


        if(dest->useClip)
        {
            glEnable(GL_SCISSOR_TEST);
            int y = (renderer->display == dest? renderer->display->h - (dest->clipRect.y + dest->clipRect.h) : dest->clipRect.y);
            float xFactor = ((float)renderer->window_w)/renderer->display->w;
            float yFactor = ((float)renderer->window_h)/renderer->display->h;
            glScissor(dest->clipRect.x * xFactor, y * yFactor, dest->clipRect.w * xFactor, dest->clipRect.h * yFactor);
        }
        
        
    
        #ifdef SDL_GPU_USE_OPENGLv1
        
        float* vertex_pointer = rdata->blit_buffer + GPU_BLIT_BUFFER_VERTEX_OFFSET;
        float* texcoord_pointer = rdata->blit_buffer + GPU_BLIT_BUFFER_TEX_COORD_OFFSET;
        int i;
        for(i = 0; i < rdata->blit_buffer_size; i++)
        {
            glBegin( GL_TRIANGLES );
            
            glTexCoord2f( *texcoord_pointer, *(texcoord_pointer+1) );
            glVertex3f( *vertex_pointer, *(vertex_pointer+1), *(vertex_pointer+2) );
            texcoord_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            vertex_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            
            glTexCoord2f( *texcoord_pointer, *(texcoord_pointer+1) );
            glVertex3f( *vertex_pointer, *(vertex_pointer+1), *(vertex_pointer+2) );
            texcoord_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            vertex_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            
            glTexCoord2f( *texcoord_pointer, *(texcoord_pointer+1) );
            glVertex3f( *vertex_pointer, *(vertex_pointer+1), *(vertex_pointer+2) );
            texcoord_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            vertex_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            
            glTexCoord2f( *texcoord_pointer, *(texcoord_pointer+1) );
            glVertex3f( *vertex_pointer, *(vertex_pointer+1), *(vertex_pointer+2) );
            texcoord_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            vertex_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            
            glTexCoord2f( *texcoord_pointer, *(texcoord_pointer+1) );
            glVertex3f( *vertex_pointer, *(vertex_pointer+1), *(vertex_pointer+2) );
            texcoord_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            vertex_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            
            glTexCoord2f( *texcoord_pointer, *(texcoord_pointer+1) );
            glVertex3f( *vertex_pointer, *(vertex_pointer+1), *(vertex_pointer+2) );
            texcoord_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            vertex_pointer += GPU_BLIT_BUFFER_FLOATS_PER_VERTEX;
            
            glEnd();
        }
        #else
        
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, GPU_BLIT_BUFFER_STRIDE, rdata->blit_buffer + GPU_BLIT_BUFFER_VERTEX_OFFSET);
        glTexCoordPointer(2, GL_FLOAT, GPU_BLIT_BUFFER_STRIDE, rdata->blit_buffer + GPU_BLIT_BUFFER_TEX_COORD_OFFSET);

        glDrawArrays(GL_TRIANGLES, 0, rdata->blit_buffer_size);
        
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        
        #endif
        
        rdata->blit_buffer_size = 0;
        
        if(dest->useClip)
        {
            glDisable(GL_SCISSOR_TEST);
        }

        glMatrixMode( GL_PROJECTION );
        glPopMatrix();
        glMatrixMode( GL_MODELVIEW );

        // restore viewport and projection
        if(isRTT)
        {
            glViewport(vp[0], vp[1], vp[2], vp[3]);

            glMatrixMode( GL_PROJECTION );
            glPopMatrix();
            glMatrixMode( GL_MODELVIEW );
        }
        
    }
}

static void Flip(GPU_Renderer* renderer)
{
    renderer->FlushBlitBuffer(renderer);
    
#ifdef SDL_GPU_USE_SDL2
    SDL_GL_SwapWindow(((RendererData_OpenGL*)renderer->data)->window);
#else
    SDL_GL_SwapBuffers();
#endif
}





GPU_Renderer* GPU_CreateRenderer_OpenGL(void)
{
    GPU_Renderer* renderer = (GPU_Renderer*)malloc(sizeof(GPU_Renderer));
    if(renderer == NULL)
        return NULL;

    memset(renderer, 0, sizeof(GPU_Renderer));

    renderer->id = "OpenGL";
    renderer->display = NULL;
    renderer->camera = GPU_GetDefaultCamera();

    renderer->data = (RendererData_OpenGL*)malloc(sizeof(RendererData_OpenGL));
    memset(renderer->data, 0, sizeof(RendererData_OpenGL));

    renderer->Init = &Init;
    renderer->IsFeatureEnabled = &IsFeatureEnabled;
    renderer->SetAsCurrent = &SetAsCurrent;
    renderer->SetDisplayResolution = &SetDisplayResolution;
    renderer->SetVirtualResolution = &SetVirtualResolution;
    renderer->Quit = &Quit;

    renderer->ToggleFullscreen = &ToggleFullscreen;
    renderer->SetCamera = &SetCamera;

    renderer->CreateImage = &CreateImage;
    renderer->LoadImage = &LoadImage;
    renderer->SaveImage = &SaveImage;
    renderer->CopyImage = &CopyImage;
    renderer->UpdateImage = &UpdateImage;
    renderer->CopyImageFromSurface = &CopyImageFromSurface;
    renderer->SubSurfaceCopy = &SubSurfaceCopy;
    renderer->FreeImage = &FreeImage;

    renderer->GetDisplayTarget = &GetDisplayTarget;
    renderer->LoadTarget = &LoadTarget;
    renderer->FreeTarget = &FreeTarget;

    renderer->Blit = &Blit;
    renderer->BlitRotate = &BlitRotate;
    renderer->BlitScale = &BlitScale;
    renderer->BlitTransform = &BlitTransform;
    renderer->BlitTransformX = &BlitTransformX;
    renderer->BlitTransformMatrix = &BlitTransformMatrix;

    renderer->SetZ = &SetZ;
    renderer->GetZ = &GetZ;
    renderer->GenerateMipmaps = &GenerateMipmaps;

    renderer->SetClip = &SetClip;
    renderer->ClearClip = &ClearClip;
    renderer->GetBlending = &GetBlending;
    renderer->SetBlending = &SetBlending;
    renderer->SetRGBA = &SetRGBA;

    renderer->ReplaceRGB = &ReplaceRGB;
    renderer->MakeRGBTransparent = &MakeRGBTransparent;
    renderer->ShiftHSV = &ShiftHSV;
    renderer->ShiftHSVExcept = &ShiftHSVExcept;
    renderer->GetPixel = &GetPixel;
    renderer->SetImageFilter = &SetImageFilter;
    renderer->SetBlendMode = &SetBlendMode;

    renderer->Clear = &Clear;
    renderer->ClearRGBA = &ClearRGBA;
    renderer->FlushBlitBuffer = &FlushBlitBuffer;
    renderer->Flip = &Flip;

    return renderer;
}

void GPU_FreeRenderer_OpenGL(GPU_Renderer* renderer)
{
    if(renderer == NULL)
        return;

    free(renderer->data);
    free(renderer);
}