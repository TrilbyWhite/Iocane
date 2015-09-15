/************************************************************************\
* IOCANE - Simulate X11 mouse events
*
* Author: Jesse McClure, copyright 2012-2013
* License: GPLv3
*
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	INTERACTIVE_DISABLED, /*! Do not grab keys, only the key bound to toggle commands. */
	COMMAND, /*! input from -c commands only, no other sources given */
	INPUTFILE /*! input from file only */
} OperationMode;

/* This will hold a table `key [keysymb] -> command [string]' of length `keycount' */
static Key *keys = NULL;
static unsigned int keycount = 0; /* number of keys in `keys' */
/* This will hold the one special key binding to grab/ungrab all other keys.
 * If set to 0 (invalid KeyCode), there is no such key.
 */
#define NO_TOGGLE_KEY 0
static KeyCode grab_ungrab_key = NO_TOGGLE_KEY;
static Bool grab_ungrab_key_grabbed = False;

OperationMode mode = STDIN; /* assume default mode */
static Display *dpy;
static int scr, sw, sh;
static Window root;
static Bool running = True;


/* list of keymasks for which to grab the keys */
static const unsigned int mods_to_grab[] = {0,LockMask,Mod2Mask,LockMask|Mod2Mask};

/* grab all keys
 * For every valid keycode found in a config file, the associated key on
 * the keyboard is grabbed.
 */
static void grab_keys()
{
	int i,j;
	/* iterate over all elements in `keys' */
	for (i = 0; i < keycount; i++) {
		for (j = 0; j < (sizeof(mods_to_grab)/sizeof(unsigned int)); j++)
			XGrabKey(dpy, keys[i].key, mods_to_grab[j], root,True,GrabModeAsync, GrabModeAsync);
	}
	/* if defined and not already grabbed, grab the special key to grab/ungrab
	 * all keys */
	if ((grab_ungrab_key != NO_TOGGLE_KEY) && (grab_ungrab_key_grabbed == False)) {
		for (j = 0; j < (sizeof(mods_to_grab)/sizeof(unsigned int)); j++)
			XGrabKey(dpy, grab_ungrab_key, mods_to_grab[j], root,True,GrabModeAsync, GrabModeAsync);
		grab_ungrab_key_grabbed = True;
	}
}

/* ungrab all keys */
static void ungrab_keys()
{
	int i, j;
	for (i = 0; i < keycount; i++) {
		for (j = 0; j < (sizeof(mods_to_grab)/sizeof(unsigned int)); j++)
			XUngrabKey(dpy, keys[i].key, mods_to_grab[j], root);
	}
}

/* Toggle whether to grab keys or not in interactive mode.
 * If not in interactive mode (see `mode') print an error.
 */
static void toggle_interactive_mode()
{
	switch (mode) {
		case INTERACTIVE:
			ungrab_keys();
			printf("interactive mode off\n");
			mode = INTERACTIVE_DISABLED;
			break;
		case INTERACTIVE_DISABLED:
			grab_keys();
			printf("interactive mode re-activated\n");
			mode = INTERACTIVE;
			break;
		default:
			fprintf(stderr, "Toggle command called in non-interactive mode.");
			break;
	}
}

/*! Send press-wait-release sequence to X for mouse button no `arg'.
 * Parts of this sequence can be disabled by passing `False' for doPress or
 * doRelease.
 */
static void press(int arg, Bool doPress, Bool doRelease) {
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
	if (doPress) {
		/* send press command */
		XSendEvent(dpy,PointerWindow,True,0xfff,&ev);
		XFlush(dpy);
		usleep(100000); /* wait for 0.1 s */
	}
	if (doRelease)
	{
		/* send release command */
		ev.type = ButtonRelease;
		ev.xbutton.state = 0x400;
		XSendEvent(dpy,PointerWindow, True, 0xfff, &ev);
		XFlush(dpy);
	}
}


/*! Interpret one line of `[<comand>] <x> <y>'; ignores empy lines and
 * comments. */
static void command(char *line) {
	/* ignore empty lines and comments */
	if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') return;
	int x=0, y=0;
	Bool isMove = False;
	/* line begins with digit; parse "<number> <number>" to x,y */
	if (line[0] >= '0' && line[0] <= '9') {
		sscanf(line,"%d %d",&x,&y);
		isMove = True;
	}
	/* otherwise, ignore leading command and parse x,y */
	else sscanf(line,"%*s %d %d",&x,&y);

	/* interpret command (actually only first character is used),
	 * call appropriate X function */
	if		(line[0] == 'p') XWarpPointer(dpy,None,root,0,0,0,0,sw,sh);
	else if (line[0] == 'b') press(x, True, True); /* press and release */
	else if (line[0] == 'h') press(x, True, False); /* press without release */
	else if (line[0] == 'r') press(x, False, True); /* release without press */
	else if (line[0] == 'm') XWarpPointer(dpy,None,None,0,0,0,0,x,y);
	else if (line[0] == 'c')
		XDefineCursor(dpy,root,XCreateFontCursor(dpy,x));
	else if (line[0] == 'q') running = False;
	else if (line[0] == 'e') running = False;
	else if (line[0] == 's') { sleep(x); usleep(y*1000); }
	else if (isMove == True) XWarpPointer(dpy,None,root,0,0,0,0,x,y);
	else fprintf(stderr, "unable to interpret command '%s'", line);
	XFlush(dpy);
}

int main(int argc, const char **argv) {
	int i = 0;
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

	/* parse config file */
	KeySym keysym;
	char *line = (char *) calloc(MAXLINE+MAXSYMLEN+2,sizeof(char));
	char *command_string;
	char keystring[MAXSYMLEN];
	keycount = 0;
	while (fgets(line,MAXLINE+MAXSYMLEN+2,rcfile) != NULL) {
		if (line[0] == '#' || line[0] == '\n') continue;
		/* split each line at space, parse into keysym,command_string */
		strncpy(keystring,line,MAXSYMLEN); *strchr(keystring,' ') = '\0';
		if ( (keysym=XStringToKeysym(keystring)) == NoSymbol ) continue;
		command_string = (char *) calloc(strlen(line) - strlen(keystring), sizeof(char));
		strcpy(command_string,strchr(line,' ')+1);
		/* check, if it's the toggle command */
		if (command_string[0] == 't') {
			/* if so, set the special toggle key */
			grab_ungrab_key = XKeysymToKeycode(dpy,keysym);
		}
		else {
			/* otherwise (normal key) add keysymbol and command to keys */
			keys = realloc(keys, (keycount+1) * sizeof(Key));
			keys[keycount].key = XKeysymToKeycode(dpy,keysym);
			keys[keycount].command = command_string;
			keycount++;
		}
	}
	/* done parsing; clean up */
	free(line);
	fclose(rcfile);

	/* actually grab the keys */
	grab_keys();

	/* loop for interactive mode
	 * Wait for event (blocks) of a grabbed key.  If the key is registered in
	 * the `keys' list, then the associated command is executed.
	 */
	XEvent ev;
	XKeyEvent *e;
	while (running && !XNextEvent(dpy,&ev)) if (ev.type == KeyPress) {
		e = &ev.xkey;
		if (e->keycode == grab_ungrab_key) {
			toggle_interactive_mode();
		}
		else {
			for (i = 0; i < keycount; i++)
				if (e->keycode == keys[i].key && keys[i].command)
					command(keys[i].command); /* execute associated command */
		}
	}

	/* program done; cleanup */
	for (i = 0; i < keycount; i++) free(keys[i].command);
	free(keys);
	XCloseDisplay(dpy); /* close connection to X server and release resources */
	return 0;
}


/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 noexpandtab : */
