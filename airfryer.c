/*
 * airfryer-hack.c — Flying Air Fryers XScreenSaver hack
 *
 * Native X11/Cairo port of the canvas animation.  No WebKit dependency.
 *
 * Build:  make
 * Test:   ./airfryer          (opens a standalone window)
 * Install: sudo make install  (copies binary + XML into xscreensaver dirs)
 *
 * Dependencies (Debian/Ubuntu):
 *   sudo apt install libcairo2-dev libx11-dev
 * Dependencies (Fedora/RHEL):
 *   sudo dnf install cairo-devel libX11-devel
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define NUM_TOASTERS  12
#define NUM_NUGGETS   14

/* ── helpers ─────────────────────────────────────────────────────── */

static double randf(void) { return drand48(); }

static void round_rect(cairo_t *cr, double x, double y,
                       double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI_2,   0);
    cairo_arc(cr, x+w-r, y+h-r, r, 0,          M_PI_2);
    cairo_arc(cr, x+r,   y+h-r, r, M_PI_2,     M_PI);
    cairo_arc(cr, x+r,   y+r,   r, M_PI,       3*M_PI_2);
    cairo_close_path(cr);
}

static void quad_to(cairo_t *cr, double cpx, double cpy,
                    double x, double y)
{
    double x0, y0;
    cairo_get_current_point(cr, &x0, &y0);
    cairo_curve_to(cr,
        x0 + (2.0/3.0)*(cpx-x0), y0 + (2.0/3.0)*(cpy-y0),
         x + (2.0/3.0)*(cpx- x),  y + (2.0/3.0)*(cpy- y),
         x, y);
}

/* ── Toaster ─────────────────────────────────────────────────────── */

typedef struct {
    double x, y, size, speed;
    double wingPhase, wingSpeed, displayBlink;
    int sw, sh;
} Toaster;

static void toaster_reset(Toaster *t, int initial)
{
    t->size         = 40 + randf() * 45;
    t->speed        = 1  + randf() * 2.5;
    t->wingPhase    = randf() * M_PI * 2;
    t->wingSpeed    = 0.07 + randf() * 0.04;
    t->displayBlink = randf() * M_PI * 2;

    if (initial) {
        t->x = randf() * (t->sw + 200);
        t->y = randf() * t->sh;
    } else if (randf() < 0.5) {
        t->x = t->sw + t->size;
        t->y = t->sh * 0.4 + randf() * t->sh * 0.6;
    } else {
        t->x = randf() * t->sw;
        t->y = t->sh + t->size * 2;
    }
}

static void toaster_init(Toaster *t, int w, int h)
{ t->sw = w; t->sh = h; toaster_reset(t, 1); }

static void toaster_update(Toaster *t, double sm)
{
    double s = t->speed * sm;
    t->x -= s * 1.8;
    t->y -= s * 0.9;
    t->wingPhase    += t->wingSpeed * fmax(0.5, sm);
    t->displayBlink += 0.03 * sm;
    if (t->x < -t->size * 3 || t->y < -t->size * 2)
        toaster_reset(t, 0);
}

/* -- wing path ---------------------------------------------------- */

static void wing_path(cairo_t *cr, double wl, double ww)
{
    cairo_new_path(cr);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, -wl*0.2, -ww*0.6,  -wl*0.5, -ww*0.95, -wl, -ww*0.5);
    cairo_curve_to(cr, -wl*0.95,-ww*0.2,  -wl*0.85, 0,        -wl*0.7, ww*0.1);
    cairo_curve_to(cr, -wl*0.45, ww*0.2,  -wl*0.2,  ww*0.1,   0, 0);
    cairo_close_path(cr);
}

static void draw_wing(cairo_t *cr, double s, double flap, int mirror)
{
    double wl = s * 1.2, ww = s * 0.55;

    cairo_save(cr);
    if (mirror) cairo_scale(cr, -1, 1);
    cairo_rotate(cr, flap);

    /* gradient fill */
    wing_path(cr, wl, ww);
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, -wl, -ww*0.3);
    cairo_pattern_add_color_stop_rgb(g, 0,   0.941, 0.925, 0.902);
    cairo_pattern_add_color_stop_rgb(g, 0.5, 0.867, 0.847, 0.816);
    cairo_pattern_add_color_stop_rgb(g, 1,   0.784, 0.761, 0.722);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);

    cairo_set_source_rgba(cr, 0.627, 0.608, 0.569, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    /* feather lines (clipped) */
    cairo_save(cr);
    wing_path(cr, wl, ww);
    cairo_clip(cr);

    cairo_set_source_rgba(cr, 0.667, 0.647, 0.608, 0.35);
    cairo_set_line_width(cr, 0.7);
    for (int i = 0; i < 8; i++) {
        double t = (i + 1) / 9.0;
        double a = -0.8 - t * 1.1;
        double len = wl * (0.5 + t * 0.45);
        cairo_move_to(cr, -s*0.03*t, s*0.02*t);
        cairo_line_to(cr, cos(a)*len, sin(a)*len);
        cairo_stroke(cr);
    }

    cairo_set_source_rgba(cr, 1, 1, 1, 0.3);
    cairo_set_line_width(cr, s * 0.025);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, 0, -s*0.01);
    cairo_curve_to(cr, -wl*0.2, -ww*0.55, -wl*0.5, -ww*0.9, -wl*0.9, -ww*0.5);
    cairo_stroke(cr);

    cairo_restore(cr); /* unclip */
    cairo_restore(cr); /* undo mirror/rotate */
}

/* -- body path ---------------------------------------------------- */

static void body_path(cairo_t *cr, double bw, double bh)
{
    cairo_new_path(cr);
    cairo_move_to(cr, 0, -bh/2);
    cairo_curve_to(cr,  bw*0.45, -bh/2,     bw*0.55, -bh*0.15,  bw*0.5,  bh*0.1);
    cairo_curve_to(cr,  bw*0.48,  bh*0.35,  bw*0.38,  bh/2,     0,       bh/2);
    cairo_curve_to(cr, -bw*0.38,  bh/2,    -bw*0.48,  bh*0.35, -bw*0.5,  bh*0.1);
    cairo_curve_to(cr, -bw*0.55, -bh*0.15, -bw*0.45, -bh/2,    0,       -bh/2);
    cairo_close_path(cr);
}

static void toaster_draw(Toaster *t, cairo_t *cr)
{
    double s  = t->size;
    double fa = sin(t->wingPhase) * 0.4;

    cairo_save(cr);
    cairo_translate(cr, t->x, t->y);

    /* wings */
    cairo_save(cr);
    cairo_translate(cr, -s*0.15, -s*0.35);
    draw_wing(cr, s, -0.15 + fa, 0);
    cairo_restore(cr);

    cairo_save(cr);
    cairo_translate(cr, s*0.15, -s*0.35);
    draw_wing(cr, s, -0.15 + fa, 1);
    cairo_restore(cr);

    double bw = s * 0.8,  bh = s * 1.2;
    double seamY = -bh/2 + bh * 0.42;

    /* body gradient */
    body_path(cr, bw, bh);
    cairo_pattern_t *bg = cairo_pattern_create_linear(-bw*0.3, -bh/2, bw*0.3, bh/2);
    cairo_pattern_add_color_stop_rgb(bg, 0,   0.220, 0.220, 0.220);
    cairo_pattern_add_color_stop_rgb(bg, 0.3, 0.165, 0.165, 0.165);
    cairo_pattern_add_color_stop_rgb(bg, 0.7, 0.118, 0.118, 0.118);
    cairo_pattern_add_color_stop_rgb(bg, 1,   0.082, 0.082, 0.082);
    cairo_set_source(cr, bg);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(bg);
    cairo_set_source_rgb(cr, 0.29, 0.29, 0.29);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    /* highlight (clipped to body) */
    cairo_save(cr);
    body_path(cr, bw, bh);
    cairo_clip(cr);
    cairo_pattern_t *hl = cairo_pattern_create_radial(
        -bw*0.15, -bh*0.25, 0,  -bw*0.15, -bh*0.25, bw*0.5);
    cairo_pattern_add_color_stop_rgba(hl, 0, 1, 1, 1, 0.1);
    cairo_pattern_add_color_stop_rgba(hl, 1, 1, 1, 1, 0);
    cairo_set_source(cr, hl);
    cairo_rectangle(cr, -bw, -bh, bw*2, bh*2);
    cairo_fill(cr);
    cairo_pattern_destroy(hl);
    cairo_restore(cr);

    /* seam */
    double seamFrac = (seamY + bh/2) / bh;
    double seamHW   = bw * (0.42 + seamFrac * 0.12);
    cairo_set_source_rgb(cr, 0.333, 0.333, 0.333);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, -seamHW, seamY);
    cairo_line_to(cr,  seamHW, seamY);
    cairo_stroke(cr);

    /* handle */
    double hY = seamY + bh * 0.06;
    double hW = bw * 0.5;
    cairo_set_source_rgb(cr, 0.333, 0.333, 0.333);
    cairo_set_line_width(cr, fmax(2.5, s * 0.04));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, -hW/2, hY);
    quad_to(cr, 0, hY + s*0.08, hW/2, hY);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 1, 1, 1, 0.12);
    cairo_set_line_width(cr, fmax(1, s * 0.02));
    cairo_move_to(cr, -hW*0.4, hY + s*0.005);
    quad_to(cr, 0, hY + s*0.065, hW*0.4, hY + s*0.005);
    cairo_stroke(cr);

    /* display */
    double dW = bw * 0.55, dH = bh * 0.07, dY = -bh * 0.28;
    round_rect(cr, -dW/2, dY, dW, dH, 3);
    cairo_set_source_rgb(cr, 0.031, 0.031, 0.031);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.227, 0.227, 0.227);
    cairo_set_line_width(cr, 0.8);
    cairo_stroke(cr);

    /* LED text */
    double ledA = 0.7 + 0.3 * sin(t->displayBlink * 2);
    double fsz  = fmax(7, s * 0.1);
    cairo_select_font_face(cr, "monospace",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, fsz);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, "375\xC2\xB0", &ext);
    double tx = -ext.width/2  - ext.x_bearing;
    double ty = dY + dH/2 - ext.height/2 - ext.y_bearing;

    /* glow */
    cairo_set_source_rgba(cr, 0, 0.784, 1.0, ledA * 0.3);
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            cairo_move_to(cr, tx+dx, ty+dy);
            cairo_show_text(cr, "375\xC2\xB0");
        }
    cairo_set_source_rgba(cr, 0, 0.784, 1.0, ledA);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, "375\xC2\xB0");

    /* buttons */
    double btnY = dY + dH + s * 0.04;
    double btnR = s * 0.03;
    double btnSp = bw * 0.13;
    for (int i = -2; i <= 2; i++) {
        cairo_arc(cr, i*btnSp, btnY, btnR, 0, M_PI*2);
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.29, 0.29, 0.29);
        cairo_set_line_width(cr, 0.5);
        cairo_stroke(cr);
    }
    cairo_arc(cr, 0, btnY, btnR*0.6, 0, M_PI*2);
    cairo_set_source_rgba(cr, 0, 0.784, 1.0,
                          0.4 + 0.2*sin(t->displayBlink*2));
    cairo_fill(cr);

    /* vents */
    cairo_set_source_rgb(cr, 0.267, 0.267, 0.267);
    cairo_set_line_width(cr, 0.8);
    double vY = -bh/2 + bh*0.06, vW = bw*0.25;
    for (int i = 0; i < 4; i++) {
        double vy = vY + i * s * 0.025;
        cairo_move_to(cr, -vW/2, vy);
        cairo_line_to(cr,  vW/2, vy);
        cairo_stroke(cr);
    }

    /* feet */
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    round_rect(cr, -bw*0.3, bh/2-2, bw*0.1, s*0.04, 1);
    cairo_fill(cr);
    round_rect(cr,  bw*0.2, bh/2-2, bw*0.1, s*0.04, 1);
    cairo_fill(cr);

    cairo_restore(cr);
}

/* ── Nugget ──────────────────────────────────────────────────────── */

typedef struct {
    double x, y, size, speed, spin, spinSpeed;
    int sw, sh;
} Nugget;

static void nugget_reset(Nugget *n, int initial)
{
    n->size      = 18 + randf() * 22;
    n->speed     = 1.2 + randf() * 2.8;
    n->spin      = randf() * M_PI * 2;
    n->spinSpeed = (0.04 + randf() * 0.05) * (randf() < 0.5 ? 1 : -1);

    if (initial) {
        n->x = randf() * (n->sw + 200);
        n->y = randf() * n->sh;
    } else if (randf() < 0.5) {
        n->x = n->sw + n->size;
        n->y = n->sh * 0.4 + randf() * n->sh * 0.6;
    } else {
        n->x = randf() * n->sw;
        n->y = n->sh + n->size * 2;
    }
}

static void nugget_init(Nugget *n, int w, int h)
{ n->sw = w; n->sh = h; nugget_reset(n, 1); }

static void nugget_update(Nugget *n, double sm)
{
    double s = n->speed * sm;
    n->x -= s * 1.8;
    n->y -= s * 0.9;
    n->spin += n->spinSpeed * sm;
    if (n->x < -n->size * 3 || n->y < -n->size * 2)
        nugget_reset(n, 0);
}

static void blob_path(cairo_t *cr, double s)
{
    cairo_new_path(cr);
    cairo_move_to(cr,  s*0.55, 0);
    cairo_curve_to(cr,  s*0.6, -s*0.3,   s*0.35,-s*0.6,   0,     -s*0.55);
    cairo_curve_to(cr, -s*0.3, -s*0.6,  -s*0.65,-s*0.35, -s*0.6,  0);
    cairo_curve_to(cr, -s*0.65, s*0.35, -s*0.3,  s*0.6,   s*0.1,  s*0.55);
    cairo_curve_to(cr,  s*0.4,  s*0.6,   s*0.65, s*0.3,   s*0.55, 0);
    cairo_close_path(cr);
}

static void nugget_draw(Nugget *n, cairo_t *cr)
{
    double s  = n->size;
    double sy = 0.68 + 0.32 * fabs(cos(n->spin));

    cairo_save(cr);
    cairo_translate(cr, n->x, n->y);
    cairo_rotate(cr, n->spin * 0.4);
    cairo_scale(cr, 1, sy);

    /* gradient fill */
    blob_path(cr, s);
    cairo_pattern_t *g = cairo_pattern_create_radial(
        -s*0.15, -s*0.15, s*0.05,  0, 0, s*0.9);
    cairo_pattern_add_color_stop_rgb(g, 0,    0.831, 0.584, 0.165);
    cairo_pattern_add_color_stop_rgb(g, 0.35, 0.722, 0.471, 0.094);
    cairo_pattern_add_color_stop_rgb(g, 0.7,  0.541, 0.322, 0.031);
    cairo_pattern_add_color_stop_rgb(g, 1,    0.369, 0.204, 0.020);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);

    /* breading crosshatch (clipped) */
    cairo_save(cr);
    blob_path(cr, s);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, 0.549, 0.314, 0.059, 0.18);
    cairo_set_line_width(cr, 0.8);
    double step = s * 0.18;
    for (double i = -s*2; i < s*2; i += step) {
        cairo_move_to(cr, i, -s); cairo_line_to(cr, i+s,  s); cairo_stroke(cr);
        cairo_move_to(cr, i,  s); cairo_line_to(cr, i+s, -s); cairo_stroke(cr);
    }
    cairo_restore(cr);

    /* sheen */
    blob_path(cr, s);
    cairo_pattern_t *sh = cairo_pattern_create_radial(
        -s*0.2, -s*0.2, 0,  -s*0.2, -s*0.2, s*0.5);
    cairo_pattern_add_color_stop_rgba(sh, 0, 1.0, 0.941, 0.706, 0.25);
    cairo_pattern_add_color_stop_rgba(sh, 1, 1.0, 0.941, 0.706, 0);
    cairo_set_source(cr, sh);
    cairo_fill(cr);
    cairo_pattern_destroy(sh);

    cairo_restore(cr);
}

/* ── main ────────────────────────────────────────────────────────── */

static volatile sig_atomic_t running = 1;
static void on_signal(int sig) { (void)sig; running = 0; }

int main(int argc, char **argv)
{
    double speed_mul = 1.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-speed") == 0 && i + 1 < argc)
            speed_mul = atof(argv[++i]) / 100.0;
        /* silently ignore -root and other xscreensaver flags */
    }
    if (speed_mul < 0.1) speed_mul = 0.1;
    if (speed_mul > 5.0) speed_mul = 5.0;

    srand48((long)time(NULL) ^ getpid());

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "airfryer: cannot open display\n"); return 1; }

    const char *wid_str = getenv("XSCREENSAVER_WINDOW");
    Window win;
    int own_window = 0;

    if (wid_str && *wid_str) {
        win = (Window)strtoul(wid_str, NULL, 0);
    } else {
        int scr = DefaultScreen(dpy);
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr),
                                  0, 0, 1280, 720, 0,
                                  WhitePixel(dpy, scr),
                                  BlackPixel(dpy, scr));
        XStoreName(dpy, win, "Flying Air Fryers");
        Atom del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy, win, &del, 1);
        XSelectInput(dpy, win, StructureNotifyMask);
        XMapWindow(dpy, win);
        own_window = 1;
    }

    XWindowAttributes wa;
    XGetWindowAttributes(dpy, win, &wa);
    int w = wa.width, h = wa.height;

    /* X surface — used only as the blit target */
    cairo_surface_t *xsurf = cairo_xlib_surface_create(
        dpy, win, wa.visual, w, h);
    cairo_t *xcr = cairo_create(xsurf);

    /* Off-screen back buffer — all drawing goes here first */
    cairo_surface_t *buf = cairo_image_surface_create(
        CAIRO_FORMAT_RGB24, w, h);
    cairo_t *cr = cairo_create(buf);

    Toaster toasters[NUM_TOASTERS];
    Nugget  nuggets[NUM_NUGGETS];
    for (int i = 0; i < NUM_TOASTERS; i++) toaster_init(&toasters[i], w, h);
    for (int i = 0; i < NUM_NUGGETS;  i++) nugget_init(&nuggets[i],  w, h);

    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);

    Atom wm_del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == ConfigureNotify &&
                (ev.xconfigure.width != w || ev.xconfigure.height != h)) {
                w = ev.xconfigure.width;
                h = ev.xconfigure.height;
                cairo_xlib_surface_set_size(xsurf, w, h);
                /* recreate back buffer at new size */
                cairo_destroy(cr);
                cairo_surface_destroy(buf);
                buf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
                cr  = cairo_create(buf);
                for (int i = 0; i < NUM_TOASTERS; i++)
                    { toasters[i].sw = w; toasters[i].sh = h; }
                for (int i = 0; i < NUM_NUGGETS; i++)
                    { nuggets[i].sw = w; nuggets[i].sh = h; }
            }
            if (ev.type == ClientMessage &&
                (Atom)ev.xclient.data.l[0] == wm_del)
                running = 0;
        }

        /* draw everything to back buffer */
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_paint(cr);

        for (int i = 0; i < NUM_NUGGETS; i++) {
            nugget_update(&nuggets[i], speed_mul);
            nugget_draw(&nuggets[i], cr);
        }
        for (int i = 0; i < NUM_TOASTERS; i++) {
            toaster_update(&toasters[i], speed_mul);
            toaster_draw(&toasters[i], cr);
        }

        /* single blit to screen */
        cairo_set_source_surface(xcr, buf, 0, 0);
        cairo_paint(xcr);

        cairo_surface_flush(xsurf);
        XFlush(dpy);
        usleep(16667); /* ~60 fps */
    }

    cairo_destroy(cr);
    cairo_surface_destroy(buf);
    cairo_destroy(xcr);
    cairo_surface_destroy(xsurf);
    if (own_window) XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
