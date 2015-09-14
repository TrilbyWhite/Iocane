/************************************************************************\
* IOCANE - Simulate X11 mouse events
*
* Author: Jesse McClure, copyright 2012-2013
* License: GPLv3
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
\************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>

#define MAXLINE		100
#define MAXSYMLEN	12

#define HELP	\
	"IOCANE: Copyright 2012, Jesse McClure\n" \
	"See `man iocane` for commands and examples.\n"

typedef struct { KeyCode key; char *command;} Key;

/*! Operation mode of this program -- where to get commands from */
typedef enum {
    UNSET = 0, /*! no mode selected yet */
    STDIN, /*! input from stdin (default) */
    INTERACTIVE, /*! interpret key strokes */
    COMMAND, /*! input from -c commands only, no other sources given */
    INPUTFILE /*! input from file only */
} OperationMode;

static Display *dpy;
static int scr, sw, sh;
static Window root;
static Bool running = True;

/*! Send press-wait-release sequence to X for mouse button no `arg'. */
static void press(int arg) {
	XEvent ev;
	memset(&ev, 0x00, sizeof(ev));
	usleep(100000); /* wait for 0.1 s */
	ev.type = ButtonPress;
	ev.xbutton.button = arg;
	ev.xbutton.same_screen = True;
    /* fetch current coordinates of pointer; stored in ev */
	XQueryPointer(dpy, root, &ev.xbutton.root, &ev.xbutton.window,
			&ev.xbutton.x_root, &ev.xbutton.y_root, &ev.xbutton.x, 
			&ev.xbutton.y,&ev.xbutton.state);
	ev.xbutton.subwindow = ev.xbutton.window;
	while(ev.xbutton.subwindow) {
		ev.xbutton.window = ev.xbutton.subwindow;
		XQueryPointer(dpy, ev.xbutton.window, &ev.xbutton.root,
				&ev.xbutton.subwindow, &ev.xbutton.x_root, &ev.xbutton.y_root, 
				&ev.xbutton.x, &ev.xbutton.y, &ev.xbutton.state);
	}
    /* send press command */
	XSendEvent(dpy,PointerWindow,True,0xfff,&ev);
	XFlush(dpy);
	usleep(100000); /* wait for 0.1 s */
	ev.type = ButtonRelease;
	ev.xbutton.state = 0x400;
    /* send release command */
	XSendEvent(dpy,PointerWindow, True, 0xfff, &ev);
	XFlush(dpy);
}


/*! Interpret one line of `[<comand>] <x> <y>'; ignores empy lines and
 * comments. */
static void command(char *line) {
    /* ignore empty lines and comments */
	if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') return;
	int x=0, y=0;
    /* line begins with digit; parse "<number> <number>" to x,y */
	if (line[0] > 47 && line[0] < 58) sscanf(line,"%d %d",&x,&y);
    /* otherwise, ignore leading command and parse x,y */
	else sscanf(line,"%*s %d %d",&x,&y);

    /* interpret command (actually only first character is used),
     * call appropriate X function */
	if (line[0] == 'p') XWarpPointer(dpy,None,root,0,0,0,0,sw,sh);
	else if (line[0] == 'b') press(x);
	else if (line[0] == 'm') XWarpPointer(dpy,None,None,0,0,0,0,x,y);
	else if (line[0] == 'c')
		XDefineCursor(dpy,root,XCreateFontCursor(dpy,x));
	else if (line[0] == 'q') running = False;
	else if (line[0] == 'e') running = False;
	else if (line[0] == 's') { sleep(x); usleep(y*1000); }
	else XWarpPointer(dpy,None,root,0,0,0,0,x,y);
	XFlush(dpy);
}

int main(int argc, const char **argv) {
	int i = 0;
    OperationMode mode = STDIN; /* assume default mode */
	FILE *input = NULL;
    /* Try to connect to X server */
	if (!(dpy=XOpenDisplay(0x0))) return 1;
    /* Initialize globals */
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy,scr);
	sw = DisplayWidth(dpy,scr);
	sh = DisplayHeight(dpy,scr);
    /* parse command line arguments */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'h') { printf(HELP); exit(0); }
			else if (argv[i][1] == 'i') mode = INTERACTIVE;
			else if (argv[i][1] == 's') mode = STDIN;
			else if (argv[i][1] == 'c' && argc > i+1) {
                // interpret the content of the `-c' argument
				command((char *)argv[(++i)]);
				if (mode == UNSET) mode = COMMAND;
			}
		}
		else {
            /* file input; try to open the file */
			printf("in = ",argv[i]);
			input = fopen(argv[i],"r");
			if (input) mode = INPUTFILE;
		}
	}

    /* if input was only -c parameters, done */
	if (mode == COMMAND) exit(0);
    /* if no input file was opened, use stdin */
	if (!input) input = stdin;
    /* iterate over given list of commands */
	if (mode == UNSET || mode == STDIN || mode == INPUTFILE) {
		char line[MAXLINE+1];
		while (running && (fgets(line,MAXLINE,input)) != NULL)
			command(line);
		XCloseDisplay(dpy);
		if (mode == 3) fclose(input);
		return 0;
	}


	/* no args -> interactive mode: */

    /* search for config file */
	char *dir = getenv("PWD");
	chdir(getenv("XDG_CONFIG_HOME"));
	chdir("iocane");
	FILE *rcfile = fopen("config","r");
	if (!rcfile) {
		chdir(getenv("HOME"));
		rcfile = fopen(".iocanerc","r");
	}
	if (!rcfile) rcfile = fopen("/usr/share/iocane/iocanerc","r");
	if (dir) chdir(dir); /* go back to current directory */
    /* if unable to find a config file, quit */
	if (!rcfile) {
		fprintf(stderr,"IOCANE: no iocanerc file found.\n");
		XCloseDisplay(dpy);
		return 0;
	}

    /* parse config file
     * For every valid keycode found in a config file, the associated key on
     * the keyboard is grabbed.
     */
	Key *keys = NULL; /* this will hold a table `key [keysymb] -> command [string]' */
	KeySym keysym;
	char *line = (char *) calloc(MAXLINE+MAXSYMLEN+2,sizeof(char));
	char keystring[MAXSYMLEN];
	int j; i = 0;
    /* list of keymasks for which to grab the keys */
	unsigned int mods[] = {0,LockMask,Mod2Mask,LockMask|Mod2Mask};
	while (fgets(line,MAXLINE+MAXSYMLEN+2,rcfile) != NULL) {
		if (line[0] == '#' || line[0] == '\n') continue;
        /* split each line at space */
		strncpy(keystring,line,MAXSYMLEN); *strchr(keystring,' ') = '\0';
		if ( (keysym=XStringToKeysym(keystring)) == NoSymbol ) continue;
        /* add keysymbol and command to keys */
		keys = realloc(keys,(i+1) * sizeof(Key));
		keys[i].key = XKeysymToKeycode(dpy,keysym);
		keys[i].command = (char *) calloc(strlen(line) - strlen(keystring),
				sizeof(char));
		strcpy(keys[i].command,strchr(line,' ')+1);
        /* get control over the identified keys from the config file */
		for (j = 0; j < 4; j++) XGrabKey(dpy, keys[i].key, mods[j],
				root,True,GrabModeAsync, GrabModeAsync);
		i++;
	}
    /* done parsing; clean up */
	int keycount = i;
	free(line);
	fclose(rcfile);

	XEvent ev;
	XKeyEvent *e;
    /* wait for event (blocks) of a grabbed key
     * If the key is registered in the keys list, then the associated command
     * is executed.
     */
	while (running && !XNextEvent(dpy,&ev)) if (ev.type == KeyPress) {
		e = &ev.xkey;
		for (i = 0; i < keycount; i++)
			if (e->keycode == keys[i].key && keys[i].command)
				command(keys[i].command); /* execute associated command */
	}

    /* program done; cleanup */
	for (i = 0; i < keycount; i++) free(keys[i].command);
	free(keys);
	XCloseDisplay(dpy);
	return 0;
}

