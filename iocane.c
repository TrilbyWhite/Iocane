
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <string.h>

#define MAXLINE		100
#define MAXSYMLEN	12

static Display *dpy;
static int scr, sw, sh;
static Window root;
static Bool running = True;

typedef struct { KeySym keysym; char *command;} Key;

static void press(int arg) {
	XEvent ev;
	memset(&ev, 0x00, sizeof(ev));
	usleep(100000);
	ev.type = ButtonPress;
	ev.xbutton.button = arg;
	if (ev.xbutton.button < 1 || ev.xbutton.button > 7) return;
	ev.xbutton.same_screen = True;
	XQueryPointer(dpy,root,&ev.xbutton.root,&ev.xbutton.window,&ev.xbutton.x_root,&ev.xbutton.y_root,&ev.xbutton.x,&ev.xbutton.y,&ev.xbutton.state);
	ev.xbutton.subwindow = ev.xbutton.window;
	while(ev.xbutton.subwindow) {
		ev.xbutton.window = ev.xbutton.subwindow;
		XQueryPointer(dpy,ev.xbutton.window,&ev.xbutton.root,&ev.xbutton.subwindow,&ev.xbutton.x_root,&ev.xbutton.y_root,&ev.xbutton.x,&ev.xbutton.y,&ev.xbutton.state);
	}
	XSendEvent(dpy,PointerWindow,True,0xfff,&ev);
	XFlush(dpy);
	usleep(100000);
	ev.type = ButtonRelease;
	ev.xbutton.state = 0x400;
	XSendEvent(dpy,PointerWindow, True, 0xfff, &ev);
	XFlush(dpy);
}

static void usage() {
	printf("IOCANE: Copyright 2012, Jesse McClure\nSee `man iocane` for commands and examples.\n");
}

static void command(char *line) {
	if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') return;
	int x=0, y=0;
	sscanf(line,"%*s %d %d",&x,&y);
	char *arg1 = strchr(line,' ') + 1;
	if (line[0] == 'p') XWarpPointer(dpy,None,root,0,0,0,0,sw,sh);
	else if (line[0] == 'b') press(x);
	else if (line[0] == 'm') XWarpPointer(dpy,None,None,0,0,0,0,x,y);
	else if (line[0] == 'c') XDefineCursor(dpy,root,XCreateFontCursor(dpy,x));
	else if (line[0] == 'q') running = False; /* only relevant in interactive mode */
	else if (line[0] == 's') { sleep(x); usleep(y*1000); }
	else XWarpPointer(dpy,None,root,0,0,0,0,x,y);
	XFlush(dpy);
}

static void scriptmode(int argc, const char **argv) {
	int i,len;
	char *line = (char *) calloc(MAXLINE+1,sizeof(char));
	char *p = line;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == ':') {
			command(line);
			p = line;
		}
		else {
			if (p > line) {*p=' '; p++;}
			len = strlen(argv[i]);
			strncpy(p,argv[i],len);
			*(p+=len) = '\0';
		}
	}
	command(line);
	free(line);
}

static void stdinmode(int argc,const char **argv) {
	FILE *input = NULL;
	if (argc == 3) input=fopen(argv[2],"r");
	if (input == NULL) input = stdin;
	char *line = (char *) calloc(MAXLINE+1,sizeof(char));
	while ( (fgets(line,MAXLINE,input)) != NULL ) command(line);
	free(line);
	if (input != stdin) fclose(input);
}

int main(int argc, const char **argv) {
	if (!(dpy=XOpenDisplay(0x0))) return 1;
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy,scr);
	sw = DisplayWidth(dpy,scr);
	sh = DisplayHeight(dpy,scr);
	if (argc > 1) {
		if (argv[1][0] == '-' && argv[1][1] == 'h') usage();
		if (argv[1][0] == '-' && argv[1][1] == '\0') stdinmode(argc,argv);
		else scriptmode(argc,argv);
		return 0;
	}
	/* esle interactive mode: */
	Key *keys = NULL;
	char *line = (char *) calloc(MAXLINE+20,sizeof(char));
	char keystring[20];
	KeySym keysym;
	int keycount = 0;
	chdir(getenv("HOME"));
	FILE *rcfile = fopen(".iocanerc","r");
	if (rcfile == NULL) fopen("/usr/share/iocane/iocanerc","r");
	if (rcfile == NULL) {
		fprintf(stderr,"IOCANE: no iocanerc file found.\n");
		return 0;
	}
	while (fgets(line,MAXLINE+MAXSYMLEN+2,rcfile) != NULL) {
		if (line[0] == '#' || line[0] == '\n') continue;
		strncpy(keystring,line,MAXSYMLEN); *strchr(keystring,' ') = '\0';
		if ( (keysym=XStringToKeysym(keystring)) == NoSymbol ) continue;
		keys = realloc(keys,(++keycount) * sizeof(Key));
		keys[keycount-1].keysym = keysym;
		keys[keycount-1].command = (char *) calloc(strlen(line) - strlen(keystring),sizeof(char));
		strcpy(keys[keycount-1].command,strchr(line,' ')+1);
	}
	free(line);
	fclose(rcfile);	
	KeyCode code;
	int i;
	for (i = 0; i < keycount; i++) if ( (code=XKeysymToKeycode(dpy,keys[i].keysym)) ) {
		XGrabKey(dpy,code,0,root,True,GrabModeAsync,GrabModeAsync);
		XGrabKey(dpy,code,LockMask,root,True,GrabModeAsync,GrabModeAsync);
	}
	XEvent ev;
	XKeyEvent *e;
	while (running && !XNextEvent(dpy,&ev)) if (ev.type == KeyPress) {
		e = &ev.xkey;
		keysym = XkbKeycodeToKeysym(dpy,(KeyCode)e->keycode,0,0);
		for (i = 0; i < keycount; i++)
			if ( (keysym==keys[i].keysym) && keys[i].command )
				command(keys[i].command);
	}
	for (i = 0; i < keycount; i++) free(keys[i].command);
	free(keys);
	XCloseDisplay(dpy);
	return 0;
}

