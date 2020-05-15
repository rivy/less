/*
 * Copyright (C) 1984-2020  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * High level routines dealing with the output to the screen.
 */

#include "less.h"
#if MSDOS_COMPILER==WIN32C
#include "windows.h"
#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE 0x8000
#endif
#endif

public int errmsgs; /* Count of messages displayed by error() */
public int need_clr;
public int final_attr;
public int at_prompt;

extern int sigs;
extern int sc_width;
extern int so_s_width, so_e_width;
extern int screen_trashed;
extern int any_display;
extern int is_tty;
extern int oldbot;

#if MSDOS_COMPILER==WIN32C || MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
extern int ctldisp;
extern int nm_fg_color, nm_bg_color;
extern int bo_fg_color, bo_bg_color;
extern int ul_fg_color, ul_bg_color;
extern int so_fg_color, so_bg_color;
extern int bl_fg_color, bl_bg_color;
extern int sgr_mode;
#if MSDOS_COMPILER==WIN32C
extern int have_full_ansi;
#endif
#endif

/*
 * Display the line which is in the line buffer.
 */
    public void
put_line(VOID_PARAM)
{
    int c;
    int i;
    int a;

    if (ABORT_SIGS())
    {
        /*
         * Don't output if a signal is pending.
         */
        screen_trashed = 1;
        return;
    }

    final_attr = AT_NORMAL;

    for (i = 0;  (c = gline(i, &a)) != '\0';  i++)
    {
        at_switch(a);
        final_attr = a;
        if (c == '\b')
            putbs();
        else
            putchr(c);
    }

    at_exit();
}

    public long
map_rgb_to_ANSI16( r, g, b )
    long r; /* [0..255] */
    long g; /* [0..255] */
    long b; /* [0..255] */
{
    long color = 0;

    const int COLOR_INTENSE = 0x8;
    const int COLOR_RED = 0x1;      // ANSI16 "RED"
    const int COLOR_GREEN = 0x2;    // ANSI16 "GREEN"
    const int COLOR_BLUE = 0x4;     // ANSI16 "BLUE"
    const int COLOR_BLACK = 0x0;
    const int COLOR_DARKGRAY = COLOR_INTENSE|COLOR_BLACK;

    // heuristics
    long base_color_threshold = 0x80;
    long intensity_threshold = 0xd0;
    long min_color_threshold = 0x20;
    long intensity_grouping_boundary = 25; // as a percentage

    long color_threshold;

    long combined_intensity_threshold;
    long combined_intensity = 0;
    int contributing_colors = 0;

    long max_intensity = 0;
    if ( r > max_intensity ) { max_intensity = r; }
    if ( g > max_intensity ) { max_intensity = g; }
    if ( b > max_intensity ) { max_intensity = b; }

    color_threshold = max_intensity - (max_intensity * intensity_grouping_boundary / 100);
    if (color_threshold < min_color_threshold) { color_threshold = min_color_threshold; }

    if ( r >= color_threshold ) { color |= COLOR_RED; contributing_colors++; combined_intensity += r; }
    if ( g >= color_threshold ) { color |= COLOR_GREEN; contributing_colors++; combined_intensity += g; }
    if ( b >= color_threshold ) { color |= COLOR_BLUE; contributing_colors++; combined_intensity += b; }

    combined_intensity_threshold = intensity_threshold * contributing_colors;

    if ((contributing_colors > 0) && (combined_intensity > combined_intensity_threshold)) { color |= COLOR_INTENSE; }
    if ((contributing_colors > 1) && (max_intensity < base_color_threshold)) { color = COLOR_DARKGRAY; }

    return color;
}

    public long
map_xterm256_to_ANSI16( n )
    long n; /* [0..255] */
{
    long color = 0;

    if ( n < 16 ) { color = n; }
    else if ( n > 231 )
    {
        if ( n < 238 ) { color = 0x0; } /* black */
        else if ( n < 244 ) { color = 0x8; } /* dark gray */
        else if ( n < 250 ) { color = 0x7; } /* gray */
        else color = 0xf; /* white */
    }
    else
    { /* 6 x 6 x 6 color increments */
        // ref: http://www.mudpedia.org/mediawiki/index.php/Xterm_256_colors @@ https://archive.is/dtVov
        unsigned char color_level[] = { 0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff }; // commonly used xterm256 RGB values (ref: https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit)
        long r = color_level[ ((n - 16) / 36) % 6 ];
        long g = color_level[ ((n - 16) / 6) % 6 ];
        long b = color_level[ (n - 16) % 6 ];
        color = map_rgb_to_ANSI16( r, g, b );
    }
    return color;
}

static char obuf[OUTBUF_SIZE];
static char *ob = obuf;

/*
 * Flush buffered output.
 *
 * If we haven't displayed any file data yet,
 * output messages on error output (file descriptor 2),
 * otherwise output on standard output (file descriptor 1).
 *
 * This has the desirable effect of producing all
 * error messages on error output if standard output
 * is directed to a file.  It also does the same if
 * we never produce any real output; for example, if
 * the input file(s) cannot be opened.  If we do
 * eventually produce output, code in edit() makes
 * sure these messages can be seen before they are
 * overwritten or scrolled away.
 */
    public void
flush(VOID_PARAM)
{
    int n;
    int fd;

    n = (int) (ob - obuf);
    if (n == 0)
        return;

#if MSDOS_COMPILER==MSOFTC
    if (is_tty && any_display)
    {
        *ob = '\0';
        _outtext(obuf);
        ob = obuf;
        return;
    }
#else
#if MSDOS_COMPILER==WIN32C || MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
    if (is_tty && any_display)
    {
        *ob = '\0';
        if ((sgr_mode && have_full_ansi) || (ctldisp != OPT_ONPLUS))
            WIN32textout(obuf, ob - obuf);
        else
        {
            /*
             * Look for SGR escape sequences, and convert them
             * to color commands.  Replace bold, underline,
             * and italic escapes into colors specified via
             * the -D command-line option.
             */
            char *anchor, *p, *p_next;
            static int statics_initialized;
            static int fg, fgi, bg, bgi;
            static int at;
            int f, b;
            int slop;
#if MSDOS_COMPILER==WIN32C
            /* Screen colors used by 3x and 4x SGR commands. */
            static unsigned char screen_color[] = {
                0, /* BLACK */
                FOREGROUND_RED,
                FOREGROUND_GREEN,
                FOREGROUND_RED|FOREGROUND_GREEN,
                FOREGROUND_BLUE,
                FOREGROUND_BLUE|FOREGROUND_RED,
                FOREGROUND_BLUE|FOREGROUND_GREEN,
                FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED
            };
#else
            static enum COLORS screen_color[] = {
                BLACK, RED, GREEN, BROWN,
                BLUE, MAGENTA, CYAN, LIGHTGRAY
            };
#endif

            // initialize static variables which have non-constant initial values
            if (statics_initialized == 0)
            {
                fg  = nm_fg_color & 7;
                fgi = nm_fg_color & 8;
                bg  = nm_bg_color & 7;
                bgi = nm_bg_color & 8;
                statics_initialized = 1;
            }

            for (anchor = p_next = obuf;
                 (p_next = memchr(p_next, ESC, ob - p_next)) != NULL; )
            {
                p = p_next;
                if ((int)(ob - p) < 3) {
                    // possible incomplete sequence at end of obuf[] buffer; output any prior chars, copy to beginning of buffer, and return
                    if (p > anchor) {
                        WIN32textout(anchor, p-anchor);
                    }
                    slop = (int)(ob - p);
                    /* {{ strcpy args overlap! }} */
                    strcpy(obuf, p);
                    ob = &obuf[slop];
                    return;
                }
                if (p[1] == '[')  /* "ESC-[" sequence */
                {
                    if (p > anchor)
                    {
                        /*
                         * If some chars seen since
                         * the last escape sequence,
                         * write them out to the screen.
                         */
                        WIN32textout(anchor, p-anchor);
                        anchor = p;
                    }
                    p += 2;  /* Skip the "ESC-[" */
                    if (is_ansi_end(*p))
                    {
                        /*
                         * Handle null escape sequence
                         * "ESC[m", which restores
                         * the normal color.
                         */
                        p++;
                        anchor = p_next = p;
                        // ESC [ m  == Clear / reset styling (attributes and color)
                        fg  = nm_fg_color & 7;
                        fgi = nm_fg_color & 8;
                        bg  = nm_bg_color & 7;
                        bgi = nm_bg_color & 8;
                        at  = 0;
                        WIN32setcolors(nm_fg_color, nm_bg_color);
                        continue;
                    }
                    p_next = p;
                    at &= ~32;

                    /*
                    ## character attributes
                    at: 1 == bold => high-intensity foreground
                    at: 2 == italic / inverse => reverse fg / bg
                    at: 4 == underline => high-intensity background
                    at: 8 == blink => high-intensity background
                    at: 16 == concealed => set fg equal to value from bg
                    at: 32 == found compound_SGR_sequence (current SGR code is part of a longer sequence; eg, "\e[1;..." not "\e[1m")
                    */
                    /*
                    ## ANSI SGR (Select Graphic Rendition)
                    ESC [ m          == Clear / reset styling (attributes and color)
                    ESC [ 0 m        == Clear / reset styling (attributes and color)
                    ESC [ 1 m        == Set bright / bold => high-intensity foreground
                    ESC [ 2 m        == Set dim => UNset bright or bold (from ConEMU)
                    ESC [ 3 m        == Set italic or inverse => reversed foreground and background
                    ESC [ 4 m        == Set underline => high-intensity background
                    ESC [ 5 m        == Set blink (slow) => high-intensity background
                    ESC [ 6 m        == Set blink (fast) => high-intensity background
                    ESC [ 7 m        == Set reverse video => reversed foreground and background
                    ESC [ 8 m        == Set concealed video => foreground color taken from background
                    ESC [ 20 m       == Clear / reset attributes (alternatively / not implemented: Fraktur attribute)
                    ESC [ 21 m       == UNset bright / bold (alternatively / not implemented: double underline)
                    ESC [ 22 m       == UNset bright / bold and UNset dim (neither bright/bold nor dim)
                    ESC [ 23 m       == UNset italic or inverse (from ConEMU)
                    ESC [ 24 m       == UNset underline
                    ESC [ 25 m       == UNset blink
                    ESC [ 27 m       == UNset reverse video (normal video)
                    ESC [ 28 m       == UNset concealed video
                    ESC [ 30..37 m   == Set ANSI foreground color
                    ESC [ 38;2;R;G;B == Set xterm 24-bit foreground color, R,G,B are [0..255]
                    ESC [ 38;5;N     == Set xterm foreground color, N is color index [0..255]
                    ESC [ 39 m       == Reset foreground to default color (no attribute changes; note: "implementation defined")
                    ESC [ 40..47 m   == Set ANSI background color
                    ESC [ 48;2;R;G;B == Set xterm 24-bit background color, R,G,B are [0..255]
                    ESC [ 48;5;N     == Set xterm background color, N is color index [0..255]
                    ESC [ 49 m       == Reset background to default color (no attribute changes; note: "implementation defined")
                    ESC [ 90..97 m   == Set ANSI high-intensity foreground color (aixterm / bash'ism)
                    ESC [ 100..107 m == Set ANSI high-intensity background color (aixterm / bash'ism)
                    ## refs
                    [ANSICON sequences] https://raw.githubusercontent.com/adoxa/ansicon/master/sequences.txt @@ https://archive.is/ZRC1T
                    [ConEMU ANSI Escape Codes] https://conemu.github.io/en/AnsiEscapeCodes.html @@ https://archive.is/66HC7
                    [bash ANSI sequences] http://misc.flogisoft.com/bash/tip_colors_and_formatting @@ https://archive.is/LAw2i
                    [ANSI escape code] https://en.wikipedia.org/wiki/ANSI_escape_code @@ https://archive.is/ZRC1T
                    */

                    /*
                     * Select foreground/background colors
                     * based on the escape sequence.
                     */
                    while (!is_ansi_end(*p))
                    {
                        char *q;
                        long code = strtol(p, &q, 10);

                        if (*q == '\0')
                        {
                            /*
                             * Incomplete sequence.
                             * Leave it unprocessed
                             * in the buffer.
                             */
                            int slop = (int) (q - anchor);
                            /* {{ strcpy args overlap! }} */
                            strcpy(obuf, anchor);
                            ob = &obuf[slop];
                            return;
                        }
                        if (q == p ||
                            code > 107 || code < 0 ||
                            (!is_ansi_end(*q) && *q != ';'))
                        {
                            p_next = q;
                            break;
                        }
                        if (*q == ';')
                        {
                            q++;
                            at |= 32;
                        }

                        switch (code)
                        {
                        default: break;
                        case 0:
                        /* case 0: all attrs off + color reset to default */
                            fg = nm_fg_color & 7;
                            bg = nm_bg_color & 7;
                            at &= 32;
                            /*
                             * \e[0m use normal
                             * intensities, but
                             * \e[0;...m resets them
                             */
                            if (at & 32)
                            {// "\e[0m"
                                fgi = 0;
                                bgi = 0;
                            } else
                            {// "\e[...0;...m"
                                fgi = nm_fg_color & 8;
                                bgi = nm_bg_color & 8;
                            }
                            break;
                        case 1: /* bold on */
                            fgi = 8;
                            at |= 1;
                            break;
                        case 2: /* dim on => bold off */
                            at &= ~1;
                            break;
                        case 3: /* italic on */
                        case 7: /* inverse on */
                            at |= 2;
                            break;
                        case 4: /* underline on */
#if MSDOS_COMPILER==WIN32C
                            if (have_full_ansi)
                                bgi = COMMON_LVB_UNDERSCORE >> 4;
                            else
#endif
                            bgi = 8;
                            at |= 4;
                            break;
                        case 5: /* slow blink on */
                        case 6: /* fast blink on */
                            bgi = 8;
                            at |= 8;
                            break;
                        case 8: /* concealed on */
                            at |= 16;
                            break;
                        case 20: /* attrs off */
                            at = 0;
                            break;
                        case 21: /* bold off */
                        case 22: /* bold off + dim off */
                            fgi = 0;
                            at &= ~1;
                            break;
                        case 23: /* italic off */
                        case 27: /* inverse off */
                            at &= ~2;
                            break;
                        case 24: /* underline off */
                            bgi = 0;
                            at &= ~4;
                            break;
                        case 25: /* blink off  */
                            at &= ~8;
                            break;
                        case 28: /* concealed off */
                            at &= ~16;
                            break;
                        case 30: case 31: case 32:
                        case 33: case 34: case 35:
                        case 36: case 37:
                            fg = screen_color[code - 30];
                            at |= 32;
                            break;
                        case 38:
                            {
                            /* set foreground color */
                            /* ;2;R;G;B */
                            /* ;5;N (xterm) */
                            long color_spec = strtol( q, &q, 10 );
                            int color = 0;
                            if (*q == '\0') { break; }
                            if (*q == ';') { q++; }
                            if (color_spec == 2)
                            {
                                long r, g, b;
                                r = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                g = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                b = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                color = map_rgb_to_ANSI16( r, g, b );
                            }
                            else if (color_spec == 5)
                            {
                                long n = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                color = map_xterm256_to_ANSI16( n );
                            }
                            fg = screen_color[ color & 0x7 ];
                            if ( color & 0x8 ) { at |= 1; } else { at &= ~1; }
                            }
                            at |= 32;
                            break;
                        case 39: /* default fg */
                            fg  = nm_fg_color;
                            at |= 32;
                            break;
                        case 40: case 41: case 42:
                        case 43: case 44: case 45:
                        case 46: case 47:
                            bg = screen_color[code - 40];
                            at |= 32;
                            break;
                        case 48:
                            {
                            /* set background color */
                            /* ;2;R;G;B */
                            /* ;5;N (xterm) */
                            long color_spec = strtol( q, &q, 10 );
                            int color = 0;
                            if (*q == '\0') { break; }
                            if (*q == ';') { q++; }
                            if (color_spec == 2)
                            {
                                long r, g, b;
                                r = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                g = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                b = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                color = map_rgb_to_ANSI16( r, g, b );
                            }
                            else if (color_spec == 5)
                            {
                                long n = strtol( q, &q, 10 );
                                if (*q == '\0') { break; }
                                if (*q == ';') { q++; }
                                color = map_xterm256_to_ANSI16( n );
                            }
                            bg = screen_color[ color & 0x7 ];
                            if ( color & 0x8 ) { at |= 4; } else { at &= ~4; }
                            }
                            at |= 32;
                            break;
                        case 49: /* default bg */
                            bg  = nm_bg_color;
                            at |= 32;
                            break;
                        case 90: case 91: case 92:
                        case 93: case 94: case 95:
                        case 96: case 97:
                            /* bash ANSI: set high-intensity foreground */
                            fg = (screen_color[code - 90] | 0x8);
                            at |= 32;
                            break;
                        case 100: case 101: case 102:
                        case 103: case 104: case 105:
                        case 106: case 107:
                            /* bash ANSI: set high-intensity background */
                            bg = (screen_color[code - 100] | 0x8);
                            at |= 32;
                            break;
                        }
                        p = q;
                        if (*q == '\0')
                        {
                            /*
                             * Incomplete sequence.
                             * Leave it unprocessed
                             * in the buffer.
                             */
                            int slop = (int) (q - anchor);
                            /* {{ strcpy args overlap! }} */
                            strcpy(obuf, anchor);
                            ob = &obuf[slop];
                            return;
                        }
                    }
                    if (!is_ansi_end(*p) || p == p_next)
                        break;
                    /*                    /*
                     * In SGR mode, the ANSI sequence is
                     * always honored; otherwise if an attr
                     * is used by itself ("\e[1m" versus
                     * "\e[1;33m", for example), set the
                     * color assigned to that attribute.
                     */
                    /* same order/priority as in screen.c within at_enter() */
                    f = fg | fgi;
                    b = bg | bgi;
                    if (at & 4)
                    {
                        if (!(sgr_mode || (at & 32)) && ul_fg_color >= 0)
                        {
                            f = ul_fg_color;
                            b = ul_bg_color;
                        }
                        else
                            b |= 8;
                    }
                    if (at & 1)
                    {
                        if (!(sgr_mode || (at & 32)) && bo_fg_color >= 0)
                        {
                            f = bo_fg_color;
                            b = bo_bg_color;
                        }
                        else
                            f |= 8;
                    }
                    if (at & 8)
                    {
                        if (!(sgr_mode || (at & 32)) && bl_fg_color >= 0)
                        {
                            f = bl_fg_color;
                            b = bl_bg_color;
                        }
                        else
                            b |= 8;
                    }
                    if (at & 2)
                    {
                        if (!(sgr_mode || (at & 32)) && so_fg_color >= 0)
                        {
                            f = so_fg_color;
                            b = so_bg_color;
                        } else
                        {
                            unsigned char t = f;
                            f = b;
                            b = t;
                        }
                    }
                    if (at & 16)
                    {
                        f = b;
                    }
                    f &= 0xf;
#if MSDOS_COMPILER==WIN32C
                    b &= 0xf | (COMMON_LVB_UNDERSCORE >> 4);
#else
                    b &= 0xf;
#endif
                    WIN32setcolors(f, b);
                    p_next = anchor = p + 1;
                } else
                    p_next++;
            }

            /* Output what's left in the buffer.  */
            WIN32textout(anchor, ob - anchor);
        }
        ob = obuf;
        return;
    }
#endif
#endif
    fd = (any_display) ? 1 : 2;
    if (write(fd, obuf, n) != n)
        screen_trashed = 1;
    ob = obuf;
}


/*
 * Output a character.
 */
    public int
putchr(c)
    int c;
{
#if 0 /* fake UTF-8 output for testing */
    extern int utf_mode;
    if (utf_mode)
    {
        static char ubuf[MAX_UTF_CHAR_LEN];
        static int ubuf_len = 0;
        static int ubuf_index = 0;
        if (ubuf_len == 0)
        {
            ubuf_len = utf_len(c);
            ubuf_index = 0;
        }
        ubuf[ubuf_index++] = c;
        if (ubuf_index < ubuf_len)
            return c;
        c = get_wchar(ubuf) & 0xFF;
        ubuf_len = 0;
    }
#endif
    if (need_clr)
    {
        need_clr = 0;
        clear_bot();
    }
#if MSDOS_COMPILER
    if (c == '\n' && is_tty)
    {
        /* remove_top(1); */
        putchr('\r');
    }
#else
#ifdef _OSK
    if (c == '\n' && is_tty)  /* In OS-9, '\n' == 0x0D */
        putchr(0x0A);
#endif
#endif
    /*
     * Some versions of flush() write to *ob, so we must flush
     * when we are still one char from the end of obuf.
     */
    if (ob >= &obuf[sizeof(obuf)-1])
        flush();
    *ob++ = c;
    at_prompt = 0;
    return (c);
}

/*
 * Output a string.
 */
    public void
putstr(s)
    constant char *s;
{
    while (*s != '\0')
        putchr(*s++);
}


/*
 * Convert an integral type to a string.
 */
#define TYPE_TO_A_FUNC(funcname, type) \
void funcname(num, buf) \
    type num; \
    char *buf; \
{ \
    int neg = (num < 0); \
    char tbuf[INT_STRLEN_BOUND(num)+2]; \
    char *s = tbuf + sizeof(tbuf); \
    if (neg) num = -num; \
    *--s = '\0'; \
    do { \
        *--s = (num % 10) + '0'; \
    } while ((num /= 10) != 0); \
    if (neg) *--s = '-'; \
    strcpy(buf, s); \
}

TYPE_TO_A_FUNC(postoa, POSITION)
TYPE_TO_A_FUNC(linenumtoa, LINENUM)
TYPE_TO_A_FUNC(inttoa, int)

/*
 * Convert an string to an integral type.
 */
#define STR_TO_TYPE_FUNC(funcname, type) \
type funcname(buf, ebuf) \
    char *buf; \
    char **ebuf; \
{ \
    type val = 0; \
    for (;;) { \
        char c = *buf++; \
        if (c < '0' || c > '9') break; \
        val = 10 * val + c - '0'; \
    } \
    if (ebuf != NULL) *ebuf = buf; \
    return val; \
}

STR_TO_TYPE_FUNC(lstrtopos, POSITION)
STR_TO_TYPE_FUNC(lstrtoi, int)

/*
 * Output an integer in a given radix.
 */
    static int
iprint_int(num)
    int num;
{
    char buf[INT_STRLEN_BOUND(num)];

    inttoa(num, buf);
    putstr(buf);
    return ((int) strlen(buf));
}

/*
 * Output a line number in a given radix.
 */
    static int
iprint_linenum(num)
    LINENUM num;
{
    char buf[INT_STRLEN_BOUND(num)];

    linenumtoa(num, buf);
    putstr(buf);
    return ((int) strlen(buf));
}

/*
 * This function implements printf-like functionality
 * using a more portable argument list mechanism than printf's.
 */
    static int
less_printf(fmt, parg)
    char *fmt;
    PARG *parg;
{
    char *s;
    int col;

    col = 0;
    while (*fmt != '\0')
    {
        if (*fmt != '%')
        {
            putchr(*fmt++);
            col++;
        } else
        {
            ++fmt;
            switch (*fmt++)
            {
            case 's':
                s = parg->p_string;
                parg++;
                while (*s != '\0')
                {
                    putchr(*s++);
                    col++;
                }
                break;
            case 'd':
                col += iprint_int(parg->p_int);
                parg++;
                break;
            case 'n':
                col += iprint_linenum(parg->p_linenum);
                parg++;
                break;
            case '%':
                putchr('%');
                break;
            }
        }
    }
    return (col);
}

/*
 * Get a RETURN.
 * If some other non-trivial char is pressed, unget it, so it will
 * become the next command.
 */
    public void
get_return(VOID_PARAM)
{
    int c;

#if ONLY_RETURN
    while ((c = getchr()) != '\n' && c != '\r')
        bell();
#else
    c = getchr();
    if (c != '\n' && c != '\r' && c != ' ' && c != READ_INTR)
        ungetcc(c);
#endif
}

/*
 * Output a message in the lower left corner of the screen
 * and wait for carriage return.
 */
    public void
error(fmt, parg)
    char *fmt;
    PARG *parg;
{
    int col = 0;
    static char return_to_continue[] = "  (press RETURN)";

    errmsgs++;

    if (any_display && is_tty)
    {
        if (!oldbot)
            squish_check();
        at_exit();
        clear_bot();
        at_enter(AT_STANDOUT);
        col += so_s_width;
    }

    col += less_printf(fmt, parg);

    if (!(any_display && is_tty))
    {
        putchr('\n');
        return;
    }

    putstr(return_to_continue);
    at_exit();
    col += sizeof(return_to_continue) + so_e_width;

    get_return();
    lower_left();
    clear_eol();

    if (col >= sc_width)
        /*
         * Printing the message has probably scrolled the screen.
         * {{ Unless the terminal doesn't have auto margins,
         *    in which case we just hammered on the right margin. }}
         */
        screen_trashed = 1;

    flush();
}

static char intr_to_abort[] = "... (interrupt to abort)";

/*
 * Output a message in the lower left corner of the screen
 * and don't wait for carriage return.
 * Usually used to warn that we are beginning a potentially
 * time-consuming operation.
 */
    public void
ierror(fmt, parg)
    char *fmt;
    PARG *parg;
{
    at_exit();
    clear_bot();
    at_enter(AT_STANDOUT);
    (void) less_printf(fmt, parg);
    putstr(intr_to_abort);
    at_exit();
    flush();
    need_clr = 1;
}

/*
 * Output a message in the lower left corner of the screen
 * and return a single-character response.
 */
    public int
query(fmt, parg)
    char *fmt;
    PARG *parg;
{
    int c;
    int col = 0;

    if (any_display && is_tty)
        clear_bot();

    (void) less_printf(fmt, parg);
    c = getchr();

    if (!(any_display && is_tty))
    {
        putchr('\n');
        return (c);
    }

    lower_left();
    if (col >= sc_width)
        screen_trashed = 1;
    flush();

    return (c);
}
