// main.cpp: initialisation & main loop

#include "engine.h"
#include "rpc/rpc_sb_compat.h"
#include "ui/cefapp.h"
#include "ui/cefsettings.h"
#include "ui/cefclienthandler.h"
#include "ui/cefrenderhandler.h"
#include "ui/cefrequestcontexthandler.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"

// extern functions and data here
extern void cleargamma();
extern void writeinitcfg();
extern int vsync, vsynctear;

// local player
dynent *player = NULL;

SDL_Window *screen = NULL;
SDL_GLContext glcontext = NULL;

// screen resolution management integers
int screenw = 0, screenh = 0, desktopw = 0, desktoph = 0;

// time management integers
int curtime = 0, lastmillis = 1, elapsedtime = 0, totalmillis = 1;
int initing = NOT_INITING;


// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// game exit


// cleans up game memory and SDL at exit
void cleanup()
{
    recorder::stop();
    cleanupserver();
    if(screen) SDL_SetWindowGrab(screen, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_TRUE);
    cleargamma();
    freeocta(worldroot);
    extern void clear_command(); clear_command();
    extern void clear_console(); clear_console();
    extern void clear_mdls();    clear_mdls();
    extern void clear_sound();   clear_sound();
    closelogfile();
    SDL_Quit();
}

// normal game quit
void quit()
{
    writeinitcfg();
    writeservercfg();
	writehistory();
    abortconnect();
    disconnect();
    localdisconnect();
    writecfg();
    cleanup();
    exit(EXIT_SUCCESS);
}
COMMAND(quit, "");

// failure exit
void fatal(const char *s, ...)   
{
    static int errors = 0;
    errors++;
    if(errors <= 2) // print up to one extra recursive error
    {
        defvformatstring(msg,s,s);
        logoutf("%s", msg);

        if(errors <= 1) // avoid recursion
        {
            if(SDL_WasInit(SDL_INIT_VIDEO))
            {
                if(screen) SDL_SetWindowGrab(screen, SDL_FALSE); // free SDL context
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_TRUE);
                cleargamma();
            }
            #ifdef WIN32
                MessageBox(NULL, msg, "Inexor fatal error", MB_OK|MB_SYSTEMMODAL);
            #endif
            SDL_Quit();
        }
    }
    exit(EXIT_FAILURE);
}

// print initialisation warning in game console
bool initwarning(const char *desc, int level, int type)
{
    if(initing < level) 
    {
        addchange(desc, type);
        return true;
    }
    return false;
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// screen setup and window settings


// hardcoded macros for screen settings
#define SCR_MINW 320
#define SCR_MINH 200
#define SCR_MAXW 10000
#define SCR_MAXH 10000
#define SCR_DEFAULTW 1024
#define SCR_DEFAULTH 768

// function to change screen resolution (forward)
void screenres(int w, int h);
ICOMMAND(screenres, "ii", (int *w, int *h), screenres(*w, *h));

// change screen width and height
VARF(scr_w, SCR_MINW, -1, SCR_MAXW, screenres(scr_w, -1));
VARF(scr_h, SCR_MINH, -1, SCR_MAXH, screenres(-1, scr_h));

// buffer precision (in bits) and AA
VAR(colorbits, 0, 0, 32);
VARF(depthbits, 0, 0, 32, initwarning("depth-buffer precision"));
VARF(stencilbits, 0, 0, 32, initwarning("stencil-buffer precision"));
VARF(fsaa, -1, -1, 16, initwarning("anti-aliasing"));

// use this function to set the swap interval for the current OpenGL context (VSYNC)
void restorevsync()
{
    if(glcontext) SDL_GL_SetSwapInterval(vsync ? (vsynctear ? -1 : 1) : 0);
}

// VSYNC settings
VARFP(vsync, 0, 0, 1, restorevsync());
VARFP(vsynctear, 0, 0, 1, { if(vsync) restorevsync(); });

// write most important settings to init.cfg using UTF-8 streams
void writeinitcfg()
{
    stream *f = openutf8file("init.cfg", "w");
    if(!f) return;
    f->printf("// automatically written on exit, DO NOT MODIFY\n// modify settings in game\n");
    extern int fullscreen;
    f->printf("fullscreen %d\n", fullscreen);
    f->printf("screenres %d %d\n", scr_w, scr_h);
    f->printf("colorbits %d\n", colorbits);
    f->printf("depthbits %d\n", depthbits);
    f->printf("stencilbits %d\n", stencilbits);
    f->printf("fsaa %d\n", fsaa);
    f->printf("vsync %d\n", vsync);
	f->printf("vsynctear %d\n", vsynctear);
    extern int useshaders, shaderprecision, forceglsl;
    f->printf("shaders %d\n", useshaders);
    f->printf("shaderprecision %d\n", shaderprecision);
    f->printf("forceglsl %d\n", forceglsl);
    extern int soundchans, soundfreq, soundbufferlen;
    f->printf("soundchans %d\n", soundchans);
    f->printf("soundfreq %d\n", soundfreq);
    f->printf("soundbufferlen %d\n", soundbufferlen);
    delete f;
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// main menu background and loading screen renderer

// Main Directory where files depending the ui are stored
SVARP(interfacedir, "media/interface");

// correct background resolution "suggestion" 
// from the user by scaling down the larger side
// of the 2 screen dimensions. (call by reference!)
static void getbackgroundres(int &w, int &h)
{
    float wk = 1, hk = 1;
    if(w < 1024) wk = 1024.0f/w;
    if(h < 768) hk = 768.0f/h;
    wk = hk = max(wk, hk);
    w = int(ceil(w*wk));
    h = int(ceil(h*hk));
}

string backgroundcaption = "";
string backgroundmapname = "";
char *backgroundmapinfo = NULL;
Texture *backgroundmapshot = NULL;

// this function is called to render the main menu background once
// so loading progress can be rendered over it continuously
void restorebackground()
{
    if(renderedframe) return;
    renderbackground(backgroundcaption[0] ? backgroundcaption : NULL, backgroundmapshot, backgroundmapname[0] ? backgroundmapname : NULL, backgroundmapinfo, true);
}

// render map loading progress background including map name and game mode info
void renderbackground(const char *caption, Texture *mapshot, const char *mapname, const char *mapinfo, bool restore, bool force)
{
    if(!inbetweenframes && !force) return;

    stopsounds(); // stop sounds while loading
 
    int w = screenw, h = screenh;
    if(forceaspect) w = int(ceil(h*forceaspect));
    getbackgroundres(w, h);
    gettextres(w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    defaultshader->set();
    glEnable(GL_TEXTURE_2D);

    static int lastupdate = -1, lastw = -1, lasth = -1;
    static float backgroundu = 0, backgroundv = 0, detailu = 0, detailv = 0;
    static int numdecals = 0;
    static struct decal { float x, y, size; int side; } decals[12];
    if((renderedframe && !mainmenu && lastupdate != lastmillis) || lastw != w || lasth != h)
    {
        lastupdate = lastmillis;
        lastw = w;
        lasth = h;

        backgroundu = rndscale(1);
        backgroundv = rndscale(1);
        detailu = rndscale(1);
        detailv = rndscale(1);
        numdecals = sizeof(decals)/sizeof(decals[0]);
        numdecals = numdecals/3 + rnd((numdecals*2)/3 + 1);
        float maxsize = min(w, h)/16.0f;
        loopi(numdecals)
        {
            decal d = { rndscale(w), rndscale(h), maxsize/2 + rndscale(maxsize/2), rnd(2) };
            decals[i] = d;
        }
    }
    else if(lastupdate != lastmillis) lastupdate = lastmillis;

    loopi(restore ? 1 : 3)
    {
        glColor3f(1, 1, 1);
		
        settexture(tempformatstring("%s/background.png", interfacedir), 0);
        float bu = w*0.67f/256.0f + backgroundu, bv = h*0.67f/256.0f + backgroundv;
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0,  0);  glVertex2f(0, 0);
        glTexCoord2f(bu, 0);  glVertex2f(w, 0);
        glTexCoord2f(0,  bv); glVertex2f(0, h);
        glTexCoord2f(bu, bv); glVertex2f(w, h);
        glEnd();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);

		settexture(tempformatstring("%s/background_detail.png", interfacedir), 0);
        float du = w*0.8f/512.0f + detailu, dv = h*0.8f/512.0f + detailv;
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0,  0);  glVertex2f(0, 0);
        glTexCoord2f(du, 0);  glVertex2f(w, 0);
        glTexCoord2f(0,  dv); glVertex2f(0, h);
        glTexCoord2f(du, dv); glVertex2f(w, h);
        glEnd();

        settexture(tempformatstring("%s/background_decal.png", interfacedir), 3);
        glBegin(GL_QUADS);
        loopj(numdecals)
        {
            float hsz = decals[j].size, hx = clamp(decals[j].x, hsz, w-hsz), hy = clamp(decals[j].y, hsz, h-hsz), side = decals[j].side;
            glTexCoord2f(side,   0); glVertex2f(hx-hsz, hy-hsz);
            glTexCoord2f(1-side, 0); glVertex2f(hx+hsz, hy-hsz);
            glTexCoord2f(1-side, 1); glVertex2f(hx+hsz, hy+hsz);
            glTexCoord2f(side,   1); glVertex2f(hx-hsz, hy+hsz);
        }
        glEnd();
        float lh = 0.5f*min(w, h), lw = lh*2,
              lx = 0.5f*(w - lw), ly = 0.5f*(h*0.5f - lh);
		defformatstring(interfacetex) ("%s/logo%s.png", interfacedir, (maxtexsize ? min(maxtexsize, hwtexsize) : hwtexsize) >= 1024 && (screenw > 1280 || screenh > 800) ? "_1024" : ""); //HQ logo if settings allow
		settexture(interfacetex, 3);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0); glVertex2f(lx,    ly);
        glTexCoord2f(1, 0); glVertex2f(lx+lw, ly);
        glTexCoord2f(0, 1); glVertex2f(lx,    ly+lh);
        glTexCoord2f(1, 1); glVertex2f(lx+lw, ly+lh);
        glEnd();

        if(caption)
        {
            int tw = text_width(caption);
            float tsz = 0.04f*min(w, h)/FONTH,
                  tx = 0.5f*(w - tw*tsz), ty = h - 0.075f*1.5f*min(w, h) - 1.25f*FONTH*tsz;
            glPushMatrix();
            glTranslatef(tx, ty, 0);
            glScalef(tsz, tsz, 1);
            draw_text(caption, 0, 0);
            glPopMatrix();
        }
        if(mapshot || mapname)
        {
            int infowidth = 12*FONTH;
            float sz = 0.35f*min(w, h), msz = (0.75f*min(w, h) - sz)/(infowidth + FONTH), x = 0.5f*(w-sz), y = ly+lh - sz/15;
            if(mapinfo)
            {
                int mw, mh;
                text_bounds(mapinfo, mw, mh, infowidth);
                x -= 0.5f*(mw*msz + FONTH*msz);
            }
            if(mapshot && mapshot!=notexture)
            {
                glBindTexture(GL_TEXTURE_2D, mapshot->id);
                glBegin(GL_TRIANGLE_STRIP);
                glTexCoord2f(0, 0); glVertex2f(x,    y);
                glTexCoord2f(1, 0); glVertex2f(x+sz, y);
                glTexCoord2f(0, 1); glVertex2f(x,    y+sz);
                glTexCoord2f(1, 1); glVertex2f(x+sz, y+sz);
                glEnd();
            }
            else
            {
                int qw, qh;
                text_bounds("?", qw, qh);
                float qsz = sz*0.5f/max(qw, qh);
                glPushMatrix();
                glTranslatef(x + 0.5f*(sz - qw*qsz), y + 0.5f*(sz - qh*qsz), 0);
                glScalef(qsz, qsz, 1);
                draw_text("?", 0, 0);
                glPopMatrix();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }        

			settexture(tempformatstring("%s/mapshot_frame.png", interfacedir), 3);
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0, 0); glVertex2f(x,    y);
            glTexCoord2f(1, 0); glVertex2f(x+sz, y);
            glTexCoord2f(0, 1); glVertex2f(x,    y+sz);
            glTexCoord2f(1, 1); glVertex2f(x+sz, y+sz);
            glEnd();
            if(mapname)
            {
                int tw = text_width(mapname);
                float tsz = sz/(8*FONTH),
                      tx = 0.9f*sz - tw*tsz, ty = 0.9f*sz - FONTH*tsz;
                if(tx < 0.1f*sz) { tsz = 0.1f*sz/tw; tx = 0.1f; }
                glPushMatrix();
                glTranslatef(x+tx, y+ty, 0);
                glScalef(tsz, tsz, 1);
                draw_text(mapname, 0, 0);
                glPopMatrix();
            }
            if(mapinfo)
            {
                glPushMatrix();
                glTranslatef(x+sz+FONTH*msz, y, 0);
                glScalef(msz, msz, 1);
                draw_text(mapinfo, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, -1, infowidth);
                glPopMatrix();
            }
        }
        glDisable(GL_BLEND);
        if(!restore) swapbuffers(false);
    }
    glDisable(GL_TEXTURE_2D);

    if(!restore)
    {
        renderedframe = false;
        copystring(backgroundcaption, caption ? caption : "");
        backgroundmapshot = mapshot;
        copystring(backgroundmapname, mapname ? mapname : "");
        if(mapinfo != backgroundmapinfo)
        {
            DELETEA(backgroundmapinfo);
            if(mapinfo) backgroundmapinfo = newstring(mapinfo);
        }
    }
}

float loadprogress = 0;

// render progress bar and map screenshot
void renderprogress(float bar, const char *text, GLuint tex, bool background)
{
    if(!inbetweenframes || envmapping) return;

    clientkeepalive();      // make sure our connection doesn't time out while loading maps etc.
    
    #ifdef __APPLE__
    interceptkey(SDLK_UNKNOWN); // keep the event queue awake to avoid 'beachball' cursor
    #endif

    if(background) restorebackground();

    int w = screenw, h = screenh;
    if(forceaspect) w = int(ceil(h*forceaspect));
    getbackgroundres(w, h);
    gettextres(w, h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();
    glColor3f(1, 1, 1);

    float fh = 0.075f*min(w, h), fw = fh*10,
          fx = renderedframe ? w - fw - fh/4 : 0.5f*(w - fw), 
          fy = renderedframe ? fh/4 : h - fh*1.5f,
          fu1 = 0/512.0f, fu2 = 511/512.0f,
          fv1 = 0/64.0f, fv2 = 52/64.0f;

	settexture(tempformatstring("%s/loading_frame.png", interfacedir), 3);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(fu1, fv1); glVertex2f(fx,    fy);
    glTexCoord2f(fu2, fv1); glVertex2f(fx+fw, fy);
    glTexCoord2f(fu1, fv2); glVertex2f(fx,    fy+fh);
    glTexCoord2f(fu2, fv2); glVertex2f(fx+fw, fy+fh);
    glEnd();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float bw = fw*(511 - 2*17)/511.0f, bh = fh*20/52.0f,
          bx = fx + fw*17/511.0f, by = fy + fh*16/52.0f,
          bv1 = 0/32.0f, bv2 = 20/32.0f,
          su1 = 0/32.0f, su2 = 7/32.0f, sw = fw*7/511.0f,
          eu1 = 23/32.0f, eu2 = 30/32.0f, ew = fw*7/511.0f,
          mw = bw - sw - ew,
          ex = bx+sw + max(mw*bar, fw*7/511.0f);
    if(bar > 0)
    {
		settexture(tempformatstring("%s/loading_bar.png", interfacedir), 3);
        glBegin(GL_QUADS);
        glTexCoord2f(su1, bv1); glVertex2f(bx,    by);
        glTexCoord2f(su2, bv1); glVertex2f(bx+sw, by);
        glTexCoord2f(su2, bv2); glVertex2f(bx+sw, by+bh);
        glTexCoord2f(su1, bv2); glVertex2f(bx,    by+bh);

        glTexCoord2f(su2, bv1); glVertex2f(bx+sw, by);
        glTexCoord2f(eu1, bv1); glVertex2f(ex,    by);
        glTexCoord2f(eu1, bv2); glVertex2f(ex,    by+bh);
        glTexCoord2f(su2, bv2); glVertex2f(bx+sw, by+bh);

        glTexCoord2f(eu1, bv1); glVertex2f(ex,    by);
        glTexCoord2f(eu2, bv1); glVertex2f(ex+ew, by);
        glTexCoord2f(eu2, bv2); glVertex2f(ex+ew, by+bh);
        glTexCoord2f(eu1, bv2); glVertex2f(ex,    by+bh);
        glEnd();
    }

    if(text)
    {
        int tw = text_width(text);
        float tsz = bh*0.8f/FONTH;
        if(tw*tsz > mw) tsz = mw/tw;
        glPushMatrix();
        glTranslatef(bx+sw, by + (bh - FONTH*tsz)/2, 0);
        glScalef(tsz, tsz, 1);
        draw_text(text, 0, 0);
        glPopMatrix();
    }

    glDisable(GL_BLEND);

    if(tex)
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        float sz = 0.35f*min(w, h), x = 0.5f*(w-sz), y = 0.5f*min(w, h) - sz/15;
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0); glVertex2f(x,    y);
        glTexCoord2f(1, 0); glVertex2f(x+sz, y);
        glTexCoord2f(0, 1); glVertex2f(x,    y+sz);
        glTexCoord2f(1, 1); glVertex2f(x+sz, y+sz);
        glEnd();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		settexture(tempformatstring("%s/mapshot_frame.png", interfacedir), 3);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0); glVertex2f(x,    y);
        glTexCoord2f(1, 0); glVertex2f(x+sz, y);
        glTexCoord2f(0, 1); glVertex2f(x,    y+sz);
        glTexCoord2f(1, 1); glVertex2f(x+sz, y+sz);
        glEnd();
        glDisable(GL_BLEND);
    }

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    swapbuffers(false);
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// mouse and keyboard management (input)


// controlling SDL input
bool shouldgrab = false, grabinput = false, minimized = false, canrelativemouse = true, relativemouse = false;
int keyrepeatmask = 0, textinputmask = 0;

// try to initialise mouse with relative coordinates instead of absolute coordinates
VARNP(relativemouse, userelativemouse, 0, 1, 1);

// check if key was pressed repeatedly using bit masking technique
void keyrepeat(bool on, int mask)
{
    if(on) keyrepeatmask |= mask;
    else keyrepeatmask &= ~mask;
}

// start and stop accepting Unicode text input events
void textinput(bool on, int mask)
{
    if(on) 
    {
        if(!textinputmask) SDL_StartTextInput();
        textinputmask |= mask;
    }
    else
    {
        textinputmask &= ~mask;
        if(!textinputmask) SDL_StopTextInput();
    }
}

// sets input grab mode for mouse and keyboard
void inputgrab(bool on)
{
    if(on)
    {
        SDL_ShowCursor(SDL_FALSE);
        if(canrelativemouse && userelativemouse)
        {
            if(SDL_SetRelativeMouseMode(SDL_TRUE) >= 0) 
            {
                SDL_SetWindowGrab(screen, SDL_TRUE);
                relativemouse = true;
            }
            else 
            {
                SDL_SetWindowGrab(screen, SDL_FALSE);
                canrelativemouse = false;
                relativemouse = false;
            }
        }
    }
    else
    {
        SDL_ShowCursor(SDL_TRUE); // show OS cursor
        if(relativemouse)
        {
            SDL_SetWindowGrab(screen, SDL_FALSE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            relativemouse = false;
        }
    }
    shouldgrab = false;
}

bool initwindowpos = false;

// switch fullscreen mode
void setfullscreen(bool enable)
{
    if(!screen) return;
    //initwarning(enable ? "fullscreen" : "windowed");
    SDL_SetWindowFullscreen(screen, enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if(!enable)
    {
        SDL_SetWindowSize(screen, scr_w, scr_h);
        if(initwindowpos)
        {
            SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            initwindowpos = false;
        }
    }
}

// do not go full screen in debug mode (doesn't work with MSVC)
#ifdef _DEBUG
VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));
#else
VARF(fullscreen, 0, 1, 1, setfullscreen(fullscreen!=0));
#endif

// implementation of screen resolution changer
// forward must be declared above in the code because of dependencies
void screenres(int w, int h)
{
    scr_w = w!=-1 ? clamp(w, SCR_MINW, SCR_MAXW) : scr_w;
    scr_h = h!=-1 ? clamp(h, SCR_MINH, SCR_MAXH) : scr_h;
    if(screen)
    {
        scr_w = min(scr_w, desktopw);
        scr_h = min(scr_h, desktoph);
        if(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN) gl_resize();
        else SDL_SetWindowSize(screen, scr_w, scr_h);
    }
    else 
    {
        initwarning("screen resolution");
    }
}

// gamma as float value
// do we even need this static here?
static int curgamma = 100;
VARFP(gamma, 30, 100, 300,
{
    if(gamma == curgamma) return;
    curgamma = gamma;
    if(SDL_SetWindowBrightness(screen, gamma/100.0f)==-1) conoutf(CON_ERROR, "Could not set gamma: %s", SDL_GetError());
});


// TODO: what do these functions do? 
void restoregamma()
{
    if(curgamma == 100) return;
    SDL_SetWindowBrightness(screen, curgamma/100.0f);
}
void cleargamma()
{
    if(curgamma != 100 && screen) SDL_SetWindowBrightness(screen, 1.0f);
}

// TODO: this has no reference at all!
VAR(dbgmodes, 0, 0, 1);

// setting up screen using various attempts with different options
void setupscreen(int &useddepthbits, int &usedfsaa)
{
    if(glcontext)
    {
        SDL_GL_DeleteContext(glcontext);
        glcontext = NULL;
    }
    if(screen)
	{
        SDL_DestroyWindow(screen);
        screen = NULL;
    }

    SDL_DisplayMode desktop;
    if(SDL_GetDesktopDisplayMode(0, &desktop) < 0) fatal("failed querying desktop display mode: %s", SDL_GetError());
    desktopw = desktop.w;
    desktoph = desktop.h;

    if(scr_h < 0) scr_h = SCR_DEFAULTH;
    if(scr_w < 0) scr_w = (scr_h*desktopw)/desktoph;
    scr_w = min(scr_w, desktopw);
    scr_h = min(scr_h, desktoph);

    int winw = scr_w, winh = scr_h, flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if(fullscreen)
    {
        winw = desktopw;
        winh = desktoph;
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        initwindowpos = true;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    static int configs[] =
    {
        0x7, /* try everything */
        0x6, 0x5, 0x3, /* try disabling one at a time */
        0x4, 0x2, 0x1, /* try disabling two at a time */
        0 /* try disabling everything */
    };
    int config = 0;
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    if(!depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    if(!fsaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }
    loopi(sizeof(configs)/sizeof(configs[0]))
    {
        config = configs[i];
        if(!depthbits && config&1) continue;
        if(!stencilbits && config&2) continue;
        if(fsaa<=0 && config&4) continue;
        if(depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, config&1 ? depthbits : 16);
        if(stencilbits)
        {
            hasstencil = config&2 ? stencilbits : 0;
            SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, hasstencil);
        }
        else hasstencil = 0;
        if(fsaa>0)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, config&4 ? 1 : 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, config&4 ? fsaa : 0);
        }
        screen = SDL_CreateWindow("Inexor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, winw, winh, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | flags);
        if(screen) break;
    }
    if(!screen) fatal("failed to create OpenGL window: %s", SDL_GetError());
    else
    {
        if(depthbits && (config&1)==0) conoutf(CON_WARN, "%d bit z-buffer not supported - disabling", depthbits);
        if(stencilbits && (config&2)==0) conoutf(CON_WARN, "Stencil buffer not supported - disabling");
        if(fsaa>0 && (config&4)==0) conoutf(CON_WARN, "%dx anti-aliasing not supported - disabling", fsaa);
    }

    SDL_SetWindowMinimumSize(screen, SCR_MINW, SCR_MINH);
    SDL_SetWindowMaximumSize(screen, SCR_MAXW, SCR_MAXH);

    glcontext = SDL_GL_CreateContext(screen);
    if(!glcontext) fatal("failed to create OpenGL context: %s", SDL_GetError());

    SDL_GetWindowSize(screen, &screenw, &screenh);

    useddepthbits = config&1 ? depthbits : 0;
    usedfsaa = config&4 ? fsaa : 0;
    restorevsync();
}

// resetting OpenGL is only used by resetgl command
// no other reference in the code
void resetgl()
{
    clearchanges(CHANGE_GFX);
    renderbackground("resetting OpenGL");

    extern void cleanupva();
    extern void cleanupparticles();
    extern void cleanupsky();
    extern void cleanupmodels();
    extern void cleanuptextures();
    extern void cleanuplightmaps();
    extern void cleanupblendmap();
    extern void cleanshadowmap();
    extern void cleanreflections();
    extern void cleanupglare();
    extern void cleanupdepthfx();
    extern void cleanupshaders();
    extern void cleanupgl();
    recorder::cleanup();
    cleanupva();
    cleanupparticles();
    cleanupsky();
    cleanupmodels();
    cleanuptextures();
    cleanuplightmaps();
    cleanupblendmap();
    cleanshadowmap();
    cleanreflections();
    cleanupglare();
    cleanupdepthfx();
    cleanupshaders();
    cleanupgl();
    
    int useddepthbits = 0, usedfsaa = 0;
    setupscreen(useddepthbits, usedfsaa);

    inputgrab(grabinput);

    gl_init(useddepthbits, usedfsaa);

    extern void reloadfonts();
    extern void reloadtextures();
    extern void reloadshaders();
    inbetweenframes = false;
    if(!reloadtexture(*notexture) || //todo
       !reloadtexture("media/interface/logo.png") ||
       !reloadtexture("media/interface/logo_1024.png") || 
       !reloadtexture("media/interface/background.png") ||
       !reloadtexture("media/interface/background_detail.png") ||
       !reloadtexture("media/interface/background_decal.png") ||
       !reloadtexture("media/interface/mapshot_frame.png") ||
       !reloadtexture("media/interface/loading_frame.png") ||
       !reloadtexture("media/interface/loading_bar.png"))
        fatal("failed to reload core texture");
    reloadfonts();
    inbetweenframes = true;
    renderbackground("initializing...");
	restoregamma();
    reloadshaders();
    reloadtextures();
    initlights();
    allchanged(true);
}
COMMAND(resetgl, "");

// global vector for (input) events
vector<SDL_Event> events;

// add a new event to event vector
void pushevent(const SDL_Event &e)
{
    events.add(e); 
}

// filter mouse motion events (?)
static bool filterevent(const SDL_Event &event)
{
    switch(event.type)
    {
        case SDL_MOUSEMOTION:
            if(grabinput && !relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
            {
                if(event.motion.x == screenw / 2 && event.motion.y == screenh / 2) 
                    return false;  // ignore any motion events generated by SDL_WarpMouse
                #ifdef __APPLE__
                if(event.motion.y == 0) 
                    return false;  // let mac users drag windows via the title bar
                #endif
            }
            break;
    }
    return true;
}

// poll and filter all events in the SDL (input) event queue
static inline bool pollevent(SDL_Event &event)
{
    while(SDL_PollEvent(&event))
    {
        if(filterevent(event)) return true;
    }
    return false;
}

// "abort" key (for calclight e.g.) ??
bool interceptkey(int sym)
{
    static int lastintercept = SDLK_UNKNOWN;
    int len = lastintercept == sym ? events.length() : 0;
    SDL_Event event;
    while(pollevent(event))
    {
        switch(event.type)
        {
            case SDL_MOUSEMOTION: break; // ignore mouse motion events during loading processes
            default: pushevent(event); break;
        }
    }
    lastintercept = sym;
    if(sym != SDLK_UNKNOWN) for(int i = len; i < events.length(); i++)
    {
        if(events[i].type == SDL_KEYDOWN && events[i].key.keysym.sym == sym) { events.remove(i); return true; }
    }
    return false;
}

// ignore mouse motion events
static void ignoremousemotion()
{
    SDL_Event e;
    SDL_PumpEvents(); // pumps the event loop, gathering events from the input devices. 
    while(SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION)); // check the event queue for messages and optionally return them
}

// move mouse to screen center
static void resetmousemotion()
{
    if(grabinput && !relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
    {
        SDL_WarpMouseInWindow(screen, screenw / 2, screenh / 2);
    }
}

// handle mouse motion coordinates
static void checkmousemotion(int &dx, int &dy)
{
    loopv(events)
    {
        SDL_Event &event = events[i];
        if(event.type != SDL_MOUSEMOTION)
        { 
            if(i > 0) events.remove(0, i); 
            return; 
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
    events.setsize(0);
    SDL_Event event;
    while(pollevent(event))
    {
        if(event.type != SDL_MOUSEMOTION)
        {
            events.add(event);
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
}

CefRefPtr<InexorCefApp> cef_app;
VAR(hudinput, 0, 1, 1);

// SDL input process handler also takes care of program exit
void checkinput()
{
    SDL_Event event;
    //int lasttype = 0, lastbut = 0;
    bool mousemoved = false; 
    while(events.length() || pollevent(event))
    {
        if(events.length()) event = events.remove(0);

        switch(event.type)
        {
            case SDL_QUIT:
                quit();
                return;

            case SDL_TEXTINPUT:
            {
                static uchar buf[SDL_TEXTINPUTEVENT_TEXT_SIZE+1];
                int len = decodeutf8(buf, int(sizeof(buf)-1), (const uchar *)event.text.text, strlen(event.text.text));
                if(len > 0) { buf[len] = '\0'; processtextinput((const char *)buf, len); }
                break;
            }

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if(keyrepeatmask || !event.key.repeat) {
                	if (hudinput) {
                		CefKeyEvent keyEvent;
            			keyEvent.modifiers = event.key.keysym.mod;
                		if (event.key.state == SDL_PRESSED) {
                    		keyEvent.windows_key_code = event.key.keysym.sym;
                    		keyEvent.native_key_code = event.key.keysym.sym;
                		} else if (event.key.state == SDL_RELEASED) {
                			keyEvent.type = KEYEVENT_CHAR;
                			keyEvent.character = event.key.keysym.sym;
                        }
                		cef_app->SendKeyEvent(keyEvent);
                	} else {
                        processkey(event.key.keysym.sym, event.key.state==SDL_PRESSED);
                	}
                }
                break;

            case SDL_WINDOWEVENT:
                switch(event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        quit();
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        shouldgrab = true;
                        break;
                    case SDL_WINDOWEVENT_ENTER:
                        inputgrab(grabinput = true);
                break;

                    case SDL_WINDOWEVENT_LEAVE:
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        inputgrab(grabinput = false);
                        break;

                    case SDL_WINDOWEVENT_MINIMIZED:
                        minimized = true;
                        break;

                    case SDL_WINDOWEVENT_MAXIMIZED:
                    case SDL_WINDOWEVENT_RESTORED:
                        minimized = false;
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                        break;

                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    {
                        SDL_GetWindowSize(screen, &screenw, &screenh);
                        if(!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
                        {
                            scr_w = clamp(screenw, SCR_MINW, SCR_MAXW);
                            scr_h = clamp(screenh, SCR_MINH, SCR_MAXH);
                        }
                        gl_resize();
                    }
                        break;
                }
                break;

            case SDL_MOUSEMOTION:
                if(grabinput)
                {
                    int dx = event.motion.xrel, dy = event.motion.yrel;
                    checkmousemotion(dx, dy);
                    if(!g3d_movecursor(dx, dy)) mousemove(dx, dy);
                    mousemoved = true;
                    // CEF Mouse Movement
                    CefMouseEvent mouse_move_event;
                    mouse_move_event.x = event.motion.x;
                    mouse_move_event.y = event.motion.y;
                    cef_app->SendMouseMoveEvent(mouse_move_event, false);
                }
                else if(shouldgrab) inputgrab(grabinput = true);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
#ifdef __linux__
#define keycodeshift 0
#else
#define keycodeshift 2*(event.button.button>=SDL_BUTTON_X1)
#endif
                {
            	    processkey(-event.button.button - keycodeshift, event.button.state==SDL_PRESSED);
                    // CEF Mouse Click
                    CefMouseEvent mouse_click_event;
                    mouse_click_event.x = event.motion.x;
                    mouse_click_event.y = event.motion.y;
                    mouse_click_event.modifiers = 0;
                    CefBrowserHost::MouseButtonType mouse_button_type = (event.button.button == 1 ? MBT_LEFT : ( event.button.button == 3 ? MBT_RIGHT : MBT_MIDDLE));
                    cef_app->SendMouseClickEvent(
                        mouse_click_event,
                        mouse_button_type,
                        event.button.state != SDL_PRESSED,
                        1
                    );
                    break;
                }
            case SDL_MOUSEWHEEL:
                {
                    if(event.wheel.y > 0) { processkey(-4, true); processkey(-4, false); }
                    else if(event.wheel.y < 0) { processkey(-5, true); processkey(-5, false); }
                    if(event.wheel.x > 0) { processkey(-35, true); processkey(-35, false); }
                    else if(event.wheel.x < 0) { processkey(-36, true); processkey(-36, false); }
                    // CEF mouse wheel
                    CefMouseEvent mouse_wheel_event;
                    mouse_wheel_event.x = event.motion.x;
                    mouse_wheel_event.y = event.motion.y;
                    mouse_wheel_event.modifiers = 1;
                    cef_app->SendMouseWheelEvent(
                        mouse_wheel_event,
                        event.wheel.x > 0 ? 20 : (event.wheel.x < 0 ? -20 : 0),
                        event.wheel.y > 0 ? 20 : (event.wheel.y < 0 ? -20 : 0)
                    );
                    break;
                }
        }
    }
    if(mousemoved) resetmousemotion();
}

// swapping buffers allows to render to a new buffer
// while the other is being presented to the screen
void swapbuffers(bool overlay)
{
    recorder::capture(overlay);
    SDL_GL_SwapWindow(screen);
}

// stack dumper function for release builts on Windows
// It looks strange. Do not touch, look at or even think about it.
#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    string out, t;
    formatstring(out)("Inexor Win32 Exception: 0x%x [0x%x]\n\n", er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#ifdef _AMD64_
	STACKFRAME64 sf = {{context->Rip, 0, AddrModeFlat}, {}, {context->Rbp, 0, AddrModeFlat}, {context->Rsp, 0, AddrModeFlat}, 0};
    while(::StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
	{
		union { IMAGEHLP_SYMBOL64 sym; char symext[sizeof(IMAGEHLP_SYMBOL64) + sizeof(string)]; };
		sym.SizeOfStruct = sizeof(sym);
		sym.MaxNameLength = sizeof(symext) - sizeof(sym);
		IMAGEHLP_LINE64 line;
		line.SizeOfStruct = sizeof(line);
        DWORD64 symoff;
		DWORD lineoff;
        if(SymGetSymFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) && SymGetLineFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#else
    STACKFRAME sf = {{context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    while(::StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
	{
		union { IMAGEHLP_SYMBOL sym; char symext[sizeof(IMAGEHLP_SYMBOL) + sizeof(string)]; };
		sym.SizeOfStruct = sizeof(sym);
		sym.MaxNameLength = sizeof(symext) - sizeof(sym);
		IMAGEHLP_LINE line;
		line.SizeOfStruct = sizeof(line);
        DWORD symoff, lineoff;
        if(SymGetSymFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) && SymGetLineFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#endif
        {
            char *del = strrchr(line.FileName, '\\');
            formatstring(t)("%s - %s [%d]\n", sym.Name, del ? del + 1 : line.FileName, line.LineNumber);
            concatstring(out, t);
        }
    }
    fatal(out);
}
#endif


// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// fps and timing


#define MAXFPSHISTORY 60
int fpspos = 0, fpshistory[MAXFPSHISTORY];
bool inbetweenframes = false, renderedframe = true;

// limiting frames per seconds to use resources intelligently
VAR(menufps, 0, 60, 1000);
VARP(maxfps, 0, 200, 1000);
void limitfps(int &millis, int curmillis)
{
    int limit = (mainmenu || minimized) && menufps ? (maxfps ? min(maxfps, menufps) : menufps) : maxfps;
    if(!limit) return;
    static int fpserror = 0;
    int delay = 1000/limit - (millis-curmillis);
    if(delay < 0) fpserror = 0;
    else
    {
        fpserror += 1000%limit;
        if(fpserror >= limit)
        {
            ++delay;
            fpserror -= limit;
        }
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        }
    }
}

// clear fps history array
void resetfpshistory()
{
    loopi(MAXFPSHISTORY) fpshistory[i] = 1;
    fpspos = 0;
}

// add current frames per seconds score to fps history array
void updatefpshistory(int millis)
{
    fpshistory[fpspos++] = max(1, min(1000, millis));
    if(fpspos>=MAXFPSHISTORY) fpspos = 0;
}

// get average fps, best fps and worst fps (see command fpsrange)
void getfps(int &fps, int &bestdiff, int &worstdiff)
{
    int total = fpshistory[MAXFPSHISTORY-1], best = total, worst = total;
    loopi(MAXFPSHISTORY-1)
    {
        int millis = fpshistory[i];
        total += millis;
        if(millis < best) best = millis;
        if(millis > worst) worst = millis;
    }

    fps = (1000*MAXFPSHISTORY)/total;
    bestdiff = 1000/best-fps;
    worstdiff = fps-1000/worst;
}

// get fps as float or integer (??)
void getfps_(int *raw)
{
    if(*raw) floatret(1000.0f/fpshistory[(fpspos+MAXFPSHISTORY-1)%MAXFPSHISTORY]);
    else
    {
        int fps, bestdiff, worstdiff;
        getfps(fps, bestdiff, worstdiff);
        intret(fps);
    }
}
COMMANDN(getfps, getfps_, "i");

// clock management
static int clockrealbase = 0, clockvirtbase = 0;
static void clockreset() 
{ 
	clockrealbase = SDL_GetTicks(); 
	clockvirtbase = totalmillis;
}
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());

// get milliseconds since program start
int getclockmillis()
{
    int millis = SDL_GetTicks() - clockrealbase;
    if(clockfix) millis = int(millis*(double(clockerror)/1000000));
    millis += clockvirtbase;
    return max(millis, totalmillis);
}
VAR(numcpus, 1, 1, 16);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// main program start

// find command line argument
static bool findarg(int argc, char **argv, const char *str)
{
    for(int i = 1; i<argc; i++) if(strstr(argv[i], str)==argv[i]) return true;
    return false;
}

// FIXME: WTF? - main is in macutils.mm?
#ifdef __APPLE__
#define main SDL_main
#endif


ICOMMAND(initrpc, "", (), logoutf("init: rpc"); inexor::rpc::rpc_init());

int main(int argc, char **argv)
{
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #ifndef _DEBUG
    #ifndef __GNUC__
    __try {
    #endif
    #endif
    #endif

    setlogfile(NULL);

    int dedicated = 0;
    char *load = NULL, *initscript = NULL;

	// setting home directory
    initing = INIT_RESET;
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'q': 
			{
				const char *dir = sethomedir(&argv[i][2]);
				if(dir) logoutf("Using home directory: %s", dir);
				break;
			}
        }
    }

	// parsing command line arguments
    execfile("init.cfg", false);
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'q': /* parsed first */ break;
            case 'r': /* compat, ignore */ break;
            case 'k':
            {
                const char *dir = addpackagedir(&argv[i][2]);
                if(dir) logoutf("Adding package directory: %s", dir);
                break;
            }
            case 'g': logoutf("Setting log file: %s", &argv[i][2]); setlogfile(&argv[i][2]); break;
            case 'd': dedicated = atoi(&argv[i][2]); if(dedicated<=0) dedicated = 2; break;
            case 'w': scr_w = clamp(atoi(&argv[i][2]), SCR_MINW, SCR_MAXW); if(!findarg(argc, argv, "-h")) scr_h = -1; break;
            case 'h': scr_h = clamp(atoi(&argv[i][2]), SCR_MINH, SCR_MAXH); if(!findarg(argc, argv, "-w")) scr_w = -1; break;
            case 'z': depthbits = atoi(&argv[i][2]); break;
            case 'b': /* compat, ignore */ break;
            case 'a': fsaa = atoi(&argv[i][2]); break;
            case 'v': vsync = atoi(&argv[i][2]); restorevsync(); break;
            case 't': fullscreen = atoi(&argv[i][2]); break;
            case 's': stencilbits = atoi(&argv[i][2]); break;
            case 'f': 
            {
                extern int useshaders, shaderprecision, forceglsl;
                int sh = -1, prec = shaderprecision;
                for(int j = 2; argv[i][j]; j++) switch(argv[i][j])
                {
                    case 'a': case 'A': forceglsl = 0; sh = 1; break;
                    case 'g': case 'G': forceglsl = 1; sh = 1; break;
                    case 'f': case 'F': case '0': sh = 0; break;
                    case '1': case '2': case '3': if(sh < 0) sh = 1; prec = argv[i][j] - '1'; break;
                    default: break;
                }
                useshaders = sh > 0 ? 1 : 0;
                shaderprecision = prec;
                break;
            }
            case 'l': 
            {
                char pkgdir[] = "media/"; //todo
                load = strstr(path(&argv[i][2]), path(pkgdir)); 
                if(load) load += sizeof(pkgdir)-1; 
                else load = &argv[i][2]; 
                break;
            }
            case 'x': initscript = &argv[i][2]; break;
            default: if(!serveroption(argv[i])) gameargs.add(argv[i]); break;
        }
        else gameargs.add(argv[i]);
    }
    initing = NOT_INITING;

    numcpus = clamp(SDL_GetCPUCount(), 1, 16);

    logoutf("init: cef: fork process (%dpx x %dpx)", scr_w, scr_h);
    cef_app = new InexorCefApp(scr_w, scr_h);
    CefMainArgs main_args(argc, argv);
    int exit_code = CefExecuteProcess(main_args, cef_app.get(), NULL);
    if (exit_code >= 0) {
        logoutf("cef exit_code: %d", exit_code);
        return exit_code;
    }

    if(dedicated <= 1)
    {
        logoutf("init: sdl");

        int par = 0;
        #ifdef _DEBUG
        par = SDL_INIT_NOPARACHUTE;
        #endif

        if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO|par)<0) fatal("Unable to initialize SDL: %s", SDL_GetError());
    }
    
	// initialise game components
    logoutf("init: net");
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    atexit(enet_deinitialize);
    enet_time_set(0);

    logoutf("init: game");
    game::parseoptions(gameargs);
    initserver(dedicated>0, dedicated>1);  // never returns if dedicated
    ASSERT(dedicated <= 1);
    game::initclient();

    logoutf("init: video");
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
    #if !defined(WIN32) && !defined(__APPLE__)
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    #endif
    int useddepthbits = 0, usedfsaa = 0;
    setupscreen(useddepthbits, usedfsaa);
    SDL_ShowCursor(SDL_FALSE);
    SDL_StopTextInput(); // workaround for spurious text-input events getting sent on first text input toggle?

    logoutf("init: gl");
    gl_checkextensions();
    gl_init(useddepthbits, usedfsaa);
    notexture = textureload("media/texture/notexture.png");
    if(!notexture) fatal("could not find core textures");

    logoutf("init: console");
    if(!execfile("config/stdlib.cfg", false)) fatal("cannot find config files (you are running from the wrong folder, try .bat file in the main folder)");   // this is the first file we load.
    if(!execfile("config/font.cfg", false)) fatal("cannot find font definitions");
    if(!setfont("default")) fatal("no default font specified");

    inbetweenframes = true;
    renderbackground("initializing...");

    logoutf("init: gl: effects");
    loadshaders();
    particleinit();
    initdecals();

    logoutf("init: world");
    camera1 = player = game::iterdynents(0);
    emptymap(0, true, NULL, false);

    logoutf("init: sound");
    initsound();

    logoutf("init: cef: initialize application");
    InexorCefSettings settings;
    CefInitialize(main_args, settings, cef_app.get(), NULL);

    logoutf("init: cfg");
    execfile("config/keymap.cfg");
    execfile("config/stdedit.cfg");
    execfile("config/menus.cfg");
    execfile("config/sounds.cfg");
    execfile("config/heightmap.cfg");
    execfile("config/blendbrush.cfg");
    if(game::savedservers()) execfile(game::savedservers(), false);
    
    identflags |= IDF_PERSIST;
    
    initing = INIT_LOAD;
    if(!execfile(game::savedconfig(), false))
    {
        execfile(game::defaultconfig());
        writecfg(game::restoreconfig());
    }
    execfile(game::autoexec(), false);
    initing = NOT_INITING;

    identflags &= ~IDF_PERSIST;

    string gamecfgname;
    copystring(gamecfgname, "config/game_");
    concatstring(gamecfgname, game::gameident());
    concatstring(gamecfgname, ".cfg");
    execfile(gamecfgname);
    
    game::loadconfigs();

    identflags |= IDF_PERSIST;

    if(execfile("once.cfg", false)) remove(findfile("once.cfg", "rb"));

    if(load)
    {
        logoutf("init: localconnect");
        //localconnect();
        game::changemap(load);
    }

	loadhistory();
    if(initscript) execute(initscript);

    logoutf("init: mainloop");

    initmumble();
    resetfpshistory();

    inputgrab(grabinput = true);
    ignoremousemotion();

	// game loop
    for(;;)
    {
        static int frames = 0;
        int millis = getclockmillis();
        limitfps(millis, totalmillis);
        elapsedtime = millis - totalmillis;
        static int timeerr = 0;
        int scaledtime = game::scaletime(elapsedtime) + timeerr;
        curtime = scaledtime/100;
        timeerr = scaledtime%100;
        if(!multiplayer(false) && curtime>200) curtime = 200;
        if(game::ispaused()) curtime = 0;
		lastmillis += curtime;
        totalmillis = millis;
        updatetime();

        inexor::rpc::rpc_tick();

        checkinput();
        menuprocess();
        tryedit();

        if(lastmillis) game::updateworld();

        checksleep(lastmillis);

        serverslice(false, 0);

        if(frames) updatefpshistory(elapsedtime);
        frames++;

        // miscellaneous general game effects
        recomputecamera();
        updateparticles();
        updatesounds();

        // cef message loop iteration
        CefDoMessageLoopWork();

        if(minimized) continue;

        inbetweenframes = false;
        if(mainmenu) gl_drawmainmenu();
        else gl_drawframe();
        // cef rendering
        // cef_app->GetRenderHandler()->Render();
        swapbuffers();

        renderedframe = inbetweenframes = true;
    }
    
    CefShutdown();

    ASSERT(0);
    return EXIT_FAILURE;

    #if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
    #endif
}
