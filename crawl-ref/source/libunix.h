/*
 *  File:       libunix.h
 */

#ifndef LIBUNIX_H
#define LIBUNIX_H

#ifndef USE_TILE

// Some replacement routines missing in gcc

#ifndef O_BINARY
#define O_BINARY O_RDWR
#endif

#ifdef UNICODE_GLYPHS
typedef unsigned int screen_buffer_t;
#else
typedef unsigned short screen_buffer_t;
#endif

#include <stdio.h>

int get_number_of_lines();
int get_number_of_cols();

int getch_ck(void);
int clrscr(void);
int cprintf(const char *format,...);
int gotoxy_sys(int x, int y);
void fakecursorxy(int x, int y);
extern "C" char *itoa(int value, char *strptr, int radix);
int kbhit(void);
int putch(unsigned char chr);
int putwch(unsigned chr);
void put_colour_ch(int colour, unsigned ch);
int translate_keypad(int keyin);
int wherex(void);
int wherey(void);
int window(int x1, int y1, int x2, int y2);
void puttext(int x1, int y1, int x2, int y2, const screen_buffer_t *);
void update_screen(void);
void clear_to_end_of_line(void);
void clear_to_end_of_screen(void);

void delay(unsigned long time);
void unixcurses_shutdown(void);
void unixcurses_startup(void);
void textbackground(int bg);
void textcolor(int col);
void textattr(int col);

void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
inline void enable_smart_cursor(bool) { }
inline bool is_smart_cursor_enabled() { return (false); }

void set_mouse_enabled(bool enabled);

#if defined(SIGHUP_SAVE) && defined(USE_UNIX_SIGNALS)
void sighup_save_and_exit();
#endif

#ifndef _LIBUNIX_IMPLEMENTATION
/* Some stuff from curses, to remove compiling warnings.. */
extern "C"
{
    int getstr(char *);
    int getch(void);
    int noecho(void);
    int echo(void);
    char *strlwr(char *str);
}
#endif


#endif
#endif
