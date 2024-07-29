/* sds_cpnl.c: SDS 940 control panel simulator  */

/* Copyright (c) 2021, Ken Rector

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   KEN RECTOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Ken Rector shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Ken Rector.

   24-Jan-21    kenr    Initial Version

 
 
 This module implements a Control Panel GUI application for the SDS 940
 simulator using the sim_frontend API which runs the SDS 940 simulator 
 in a background process.
 
 The graphical interface includes register displays and toggle, pushbutton
 and rotary thumb switch controls.  Toggle and rotary thumb switches are
 manipulated by pressing the left mouse button down at the center of the
 control and swiping the cursor up, down, left or right before releasing.
 Pushbuttons are manipulated by simply clicking and releasing the left
 mouse button at the center of the control.
 
 Control Panel is closed by the window menu close command or the
 menu bar close button.
 
 A console terminal may be connected to a Telnet port to provide an
 interface to the SDS 940 keyboard/printer device.
 
 Control Panel requires four .png image files that are supplied.
 
  
 */
 
/* SDS 940 control panel image courtesy of Computer History Museum */
/* Acknowlegement to Roberto Sancho at https://github.com/rsanchovilla/SimH_cpanel */
/* Acknowlegement to Mark Emmer for research into the blue lamps
   (part number DM160) and other help based on his 940 experience.
  http://www.r-type.org/3rdparty/3rd0003.htm   */

#include "png.h"
#include "sds_cpnl.h"
#include "sim_frontpanel.h"
#include <pthread.h>
#include <math.h>
#include <errno.h>
#if defined(HAVE_LIBSDL)
#include "SDL2/SDL.h"

#if defined (main)
#undef main
#endif

#define BKGNDFILE           BOOT_CODE_ARRAY_1
#define BKGNDFILE_SIZE      BOOT_CODE_SIZE_1
#define BKGNDFILE_NAME      BOOT_CODE_FILENAME_1
#define ACTIVEFILE0         BOOT_CODE_ARRAY_2
#define ACTIVEFILE0_SIZE    BOOT_CODE_SIZE_2
#define ACTIVEFILE0_NAME    BOOT_CODE_FILENAME_2
#define ACTIVEFILE1         BOOT_CODE_ARRAY_3
#define ACTIVEFILE1_SIZE    BOOT_CODE_SIZE_3
#define ACTIVEFILE1_NAME    BOOT_CODE_FILENAME_3
#define ACTIVEFILE2         BOOT_CODE_ARRAY_4
#define ACTIVEFILE2_SIZE    BOOT_CODE_SIZE_4
#define ACTIVEFILE2_NAME   BOOT_CODE_FILENAME_4
#define MOMENTARY   3 

enum fpcmds {
    RU_BOOT = 1,
    RU_CONT,
    RU_STEP,
    RU_HALT
};

void cpnl_paint();
void cpnl_drawctl       (int x, int y, int w, int h, SDL_Surface *surface);
void cpnl_drawslct      (int x, int y, int w, int h, int srcx, int srcy,
                         SDL_Surface *surface);
void cpnl_drawlamp      (int bit, SDSControl *ctl);
void cpnl_drawlampavg   (int bit, SDSControl *ctl);
void cpnl_drawToggle    (SDSControl *ctl);
static uint32_t surface_rgb_color (uint32_t r, uint32_t g, uint32_t b);
typedef struct image_file {
    unsigned char *data;
    size_t size;
    const char *file_name;
    size_t position;
    } image_file;
static SDL_Surface *read_png_file (image_file *file_struct, int * WW, int * HH);
char sdsname_dev[] = {"S  C  I  E  N  T  I  F  I  C    D  A  T  A     S  Y  S  T  E  M  S"};
int A[24], B[24], C[24], X[24], P[14], EM2, EM3, OV, UAR, ION;

#include "940_inactive_png.h"   /* defines array inactive_940_png */
#include "940_active0_png.h"    /* defines array active0_940_png */
#include "940_active1_png.h"    /* defines array active1_940_png */
#include "940_active2_png.h"    /* defines array active2_940_png */

image_file cpnl_imgnm[] = {
        {BKGNDFILE,     BKGNDFILE_SIZE,     BKGNDFILE_NAME},
        {ACTIVEFILE0,   ACTIVEFILE0_SIZE,   ACTIVEFILE0_NAME},
        {ACTIVEFILE1,   ACTIVEFILE1_SIZE,   ACTIVEFILE1_NAME},
        {ACTIVEFILE2,   ACTIVEFILE2_SIZE,   ACTIVEFILE2_NAME}
    };

int cpnl_imgW, cpnl_imgH;
double cpnl_sclW, cpnl_sclH;
SDSPoint cpnl_mousedown;
SDSPoint cpnl_mouseup;

int cpnl_power = 0;
int cpnl_run = 0;
int cpnl_abx = 0;
int cpnl_hold;
int cpnl_regselect;
int cpnl_regchg = 0;
int cpnl_chanselect;
int cpnl_starting;
int cpnl_halt;
char *cpnl_bootunit;
int cpnl_previous;
int *cpnl_registers[4] = {A,B,C,X};
char *cpnl_regnm[4] = {"A","B","C","X"};
char *sim_config = NULL;
char *sim_path;
PANEL *panel;
int debug = 0;
int throttle = 0;
unsigned long long simulation_time;
int cpnl_quit;
SDL_Window *cpnl_window;
SDL_Surface *cpnl_work_surface;
SDL_Surface *cpnl_surfaces[4];
int sim_end;

static void displaycallback (PANEL *panel, unsigned long long sim_time, void *context)
{
    simulation_time = sim_time;
    cpnl_paint();
}

static int distance  (SDSPoint *p1, SDSPoint *p2) {
    return ((int)hypot((double)(p1->x - p2->x), (double)(p1->y - p2->y)));
}

int cpnl_decodepress(SDSPoint *down, SDSPoint *released) {
    
    int swipe;
    int bp;
    int zero = 0;
    int two = 2;
    int three = 3;
    int i;
    
    for (i = 0; i < sizeof(ctl)/sizeof(SDSControl);i++) {
        if ( distance(down,&ctl[i].at) < 20) {
            swipe = down->y - released->y;
            switch (i) {
                case POWER:
                    cpnl_power = cpnl_power ^ 1;
                    if (cpnl_run) {                 // stop on power down
                        sim_panel_exec_halt (panel);
                    }
                    break;
                case PAPER:
                case CARD:
                    switch (i) {
                        case PAPER:
                            if (swipe > 0)
                                cpnl_bootunit = "PTR";
                            else
                                i = TAPE;
                                cpnl_bootunit = "MT0";
                            break;
                        case CARD:
                            if (swipe > 0)
                                cpnl_bootunit = "CR";
                            else {
                                i = DRUM;
                                cpnl_bootunit = "RAD";
                            }
                            break;
                    }
                    toggle[i].brite = MOMENTARY;
                    if (cpnl_power & cpnl_run & cpnl_starting)
                         sim_panel_exec_boot (panel, cpnl_bootunit);
                    cpnl_starting = 0;
                    break;
                case START:
                    if (cpnl_power && !cpnl_run && (cpnl_regselect == 2)) {
                        // fill sequence must be idle, start, run, fill
                        sim_panel_gen_deposit (panel, "A", sizeof (int), &zero);
                        sim_panel_gen_deposit (panel, "B", sizeof (int), &zero);
                        sim_panel_gen_deposit (panel, "C", sizeof (int), &zero);
                        sim_panel_gen_deposit (panel, "X", sizeof (int), &zero);
                        sim_panel_gen_deposit (panel, "P", sizeof (int), &zero);
                        sim_panel_gen_deposit (panel, "EM3", sizeof (int), &three);
                        sim_panel_gen_deposit (panel, "EM2", sizeof (int), &two);
                        sim_panel_gen_deposit (panel, "OV", sizeof (int), &zero);
                        cpnl_starting = 1;
                    }
                    break;
                case HOLD:
                    toggle[HOLD].brite = (swipe > 0) ? -1 : 0;
                    if ((cpnl_hold = (swipe > 0)) ? 1 : 0) {
                        cpnl_run = 0;
                        cpnl_starting = 0;
                    }
                    break;
                case CHANNEL:
                    if (down->x <= released->x)
                        cpnl_chanselect = (cpnl_chanselect + 1) % 9;
                    else
                        if (cpnl_chanselect == 0)
                            cpnl_regselect = 3;
                        else
                            cpnl_chanselect = (cpnl_chanselect - 1) % 9;
                    break;
                case REGISTER:
                    if (down->x <= released->x)
                        cpnl_regselect = (cpnl_regselect + 1) % 4;
                    else
                        if (cpnl_regselect == 0)
                            cpnl_regselect = 3;
                        else
                            cpnl_regselect = (cpnl_regselect - 1) % 4;
                    break;
                case MEMORYL:
                    if (cpnl_previous == MEMORYR)
                         sim_panel_gen_deposit (panel, "ALL", sizeof (int), &zero);
                    break;
                case MEMORYR:
                    if (cpnl_previous == MEMORYL)
                         sim_panel_gen_deposit (panel, "ALL", sizeof (int), &zero);
                    break;
                case BKPT1:
                case BKPT2:
                case BKPT3:
                case BKPT4:
                    bp = (swipe > 0) ? 0 : 1;
                    switch (i) {
                        case BKPT1:
                            sim_panel_gen_deposit (panel,"BPT1",sizeof (int),&bp);
                            toggle[BKPT1].brite = bp * -1;
                            break;
                        case BKPT2:
                            sim_panel_gen_deposit (panel,"BPT2",sizeof (int),&bp);
                            toggle[BKPT2].brite = bp * -1;
                            break;
                        case BKPT3:
                            sim_panel_gen_deposit (panel,"BPT3",sizeof (int),&bp);
                            toggle[BKPT3].brite = bp * -1;
                            break;
                        case BKPT4:
                            sim_panel_gen_deposit (panel,"BPT4",sizeof (int),&bp);
                            toggle[BKPT4].brite = bp * -1;
                            break;
                    }
                    break;
                case RUN:
                    if (swipe > 0){
                        if (!cpnl_run) {
                            cpnl_run++;
                            toggle[RUN].brite = -1;
                            if (!cpnl_starting &&
                                !cpnl_hold &&
                                cpnl_power)
                                sim_panel_exec_run (panel);
                        }
                    }
                    else if (cpnl_run) {
                        cpnl_run = 0;
                        toggle[RUN].brite = 0;
                        if (cpnl_power)
                            sim_panel_exec_halt (panel);
                    }
                    else {
                        toggle[STEP].brite = MOMENTARY;
                        cpnl_halt = 0;
                        if (cpnl_power)
                            sim_panel_exec_step (panel);
                    }
                    break;
                case INTENBL:
                    toggle[INTENBL].brite = (swipe < 0) ? -1 : 0;
                    break;
                case PARITY:
                    toggle[PARITY].brite = (swipe < 0) ? -1 : 0;
                    break;
                case CLEAR:
                    if ((cpnl_power == 1) &&
                        (cpnl_run == 0)) {
                        *cpnl_registers[cpnl_regselect] = 0;
                        sim_panel_gen_deposit(panel,cpnl_regnm[cpnl_regselect],
                            sizeof (int), &zero);
                    }
                    break;
            }
            cpnl_previous = i;
            return 0;
        }
    }
    if ((cpnl_power == 1) && (cpnl_run == 0)) {
        for (i = 0; i < (sizeof(regbutton)/sizeof(SDSPoint));i++) {
            if ( distance(down,&regbutton[i]) < 10) {
                *cpnl_registers[cpnl_regselect] |= 040000000 >> i;
                sim_panel_gen_deposit(panel,cpnl_regnm[cpnl_regselect],
                sizeof (int),cpnl_registers[cpnl_regselect]);
            }
        }
    }
    return 0;
}
    
void cpnl_paint() {
    
    int *dreg;
    int i;
    
    if (cpnl_quit)
        return;
    if (SDL_BlitScaled(cpnl_surfaces[0],NULL,cpnl_work_surface,NULL)) {
        fprintf(stderr,"Paint BlitSurface failed %s\r\n",SDL_GetError());
    }
    if (cpnl_power) {
        for (i = 0; i < (sizeof(pLamp)/sizeof(SDSControl));i++) {
            cpnl_drawlampavg(P[13-i],&pLamp[i]);
        }
        if ((cpnl_run) && (cpnl_abx == 0))
            dreg = C;
        else
            dreg = cpnl_registers[cpnl_regselect];
        for (i = 0; i < (sizeof(regLamp)/sizeof(SDSControl));i++) {
            cpnl_drawlampavg(dreg[23-i],&regLamp[i]);
        }
        for (i = 0; i < (sizeof(unitLamp)/sizeof(SDSControl));i++) {
            if ((UAR & (040 >> i)) != 0)
                cpnl_drawlamp((UAR & (040 >> i)),&unitLamp[i]);
        }
        cpnl_drawlamp((EM3 != 3),&misc[0]);
        cpnl_drawlamp((EM2 != 2),&misc[1]);
        cpnl_drawlamp(cpnl_halt,&misc[2]);
        cpnl_drawlamp(OV,&misc[3]);
        cpnl_drawlamp(ION,&misc[5]);
        cpnl_drawctl(misc[6].at.x-10,misc[6].at.y-10,40,40,cpnl_surfaces[3]);
    }
    for (i = 0; i < NUMDISPTGLS; i++) {
        cpnl_drawToggle(&toggle[i]);
    }
    cpnl_drawslct(chan[cpnl_chanselect].at.x,chan[cpnl_chanselect].at.y,
        chan[cpnl_chanselect].dim.x,chan[cpnl_chanselect].dim.y,158,420,
        cpnl_surfaces[1] );
    cpnl_drawslct(reg[cpnl_regselect].at.x,reg[cpnl_regselect].at.y,
        reg[cpnl_regselect].dim.x,reg[cpnl_regselect].dim.y,1018,488,
        cpnl_surfaces[1]);
    
    SDL_UpdateWindowSurface(cpnl_window);
}

void cpnl_drawToggle(SDSControl *ctl) {
    
    if (ctl->brite)
        cpnl_drawctl(ctl->at.x,ctl->at.y,ctl->dim.x,ctl->dim.y,cpnl_surfaces[ctl->image]);
    if (ctl->brite > 0)
        ctl->brite--;
}

void cpnl_drawlamp(int state, SDSControl *ctl) {

    if (state)
        cpnl_drawctl(ctl->at.x,ctl->at.y,6,11,cpnl_surfaces[3]);
}

void cpnl_drawlampavg(int state, SDSControl *ctl) {

    cpnl_drawctl(ctl->at.x,ctl->at.y,6,11,cpnl_surfaces[state]);
}

void cpnl_drawctl(int x, int y, int w, int h, SDL_Surface *surface) {

    SDL_Rect srect,drect;
    
    srect.x = x;
    srect.y = y;
    srect.w = w;
    srect.h = h;
    drect.x = (int)(x / cpnl_sclW);
    drect.y = (int)(y / cpnl_sclH);
    drect.w = (int)(w / cpnl_sclW);
    drect.h = (int)(h / cpnl_sclH);
    if (SDL_BlitScaled(surface,&srect,cpnl_work_surface,&drect ) < 0)
           printf("BlitScaled error2  %s\r\n",SDL_GetError());
}

void cpnl_drawslct(int x, int y, int w, int h,
                   int dstx, int dsty, SDL_Surface *surface) {
    
    SDL_Rect srect,drect;
    
    srect.x = x;
    srect.y = y;
    srect.w = w;
    srect.h = h;
    drect.x = (int)(dstx / cpnl_sclW);
    drect.y = (int)(dsty / cpnl_sclH);
    drect.w = (int)(w / cpnl_sclW);
    drect.h = (int)(h / cpnl_sclH);
    if (SDL_BlitScaled(surface,&srect,cpnl_work_surface,&drect ) < 0)
           printf("BlitScaled error1  %s\r\n",SDL_GetError());
}

int main (int argc, char *argv[]) {
    int i;
    char *c;
    SDL_Event e;
    SDSPoint drop;
    union {int i; char c[sizeof (int)]; } end_test;
    
    end_test.i = 1;                             //test endian-ness
    sim_end = (end_test.c[0] != 0);
    
    
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr,"Failed to initialize the SDL2 library\n");
        return 0;
    }
    for (i = 0; i < 4; i++) {
        cpnl_surfaces[i] = read_png_file(&cpnl_imgnm[i], &cpnl_imgW, &cpnl_imgH);
        if (!cpnl_surfaces[i]) {
            fprintf(stderr, "Error reading %s  - %s\n", cpnl_imgnm[i].file_name, SDL_GetError());
            return 0;
        }
    }
    cpnl_sclW = cpnl_sclH = 1;
    cpnl_window = SDL_CreateWindow(sdsname_dev,
         100,100,cpnl_imgW, cpnl_imgH,SDL_WINDOW_RESIZABLE);
    if(cpnl_window == 0) {
        fprintf(stderr,"Failed to create window\n");
        return 0;
    }
    cpnl_work_surface = SDL_GetWindowSurface(cpnl_window);
    if (SDL_BlitSurface(cpnl_surfaces[0],NULL,cpnl_work_surface,NULL)) {
        fprintf(stderr,"Initial BlitSurface failed %s\r\n",SDL_GetError());
    }
    if (SDL_UpdateWindowSurface(cpnl_window)) {
        fprintf(stderr,"Initial update surface failed %s\r\n",SDL_GetError());
        return 0;
    }

    sim_path = strcpy ((char *)malloc (strlen (argv[0]) + 10), argv[0]);
    c = strrchr (sim_path, '/');
    if (c == NULL)
        c = strrchr (sim_path, '\\');
    if (c != NULL)
        strcpy (c + 1, "sds");
    else
        strcpy (sim_path, "sds");

    while (--argc > 0) {
        ++argv;
        if (strcmp (argv[0], "-debug") == 0) {
            debug = 1;
            continue;
        }
        if (strcmp (argv[0], "-throttle") == 0) {
            throttle = 1;
            continue;
        }
        if (strcmp (argv[0], "-abx") == 0) {
            cpnl_abx = 1;
            continue;
        }
        if (argv[0][0] != '-') {
            sim_config = argv[0];
            continue;
        }
        if ((argv[0][0] == '?') || (0 == strcmp (argv[0], "--help"))) {
            fprintf (stderr, "Usage: sdscp {-d} {-abx} {optional-configuration-file}\n");
            fprintf (stderr, "  -debug     enable debug logging\n");
            fprintf (stderr, "  -throttle  throttle simulator execution rate\n");
            fprintf (stderr, "  -abx       The documented SDS 940 frontpanel only displays the C register\n");
            fprintf (stderr, "             when running.  Registers A, B, C are only displayed when Idle.\n");
            fprintf (stderr, "             However, to display the switch-selected register when running\n");
            fprintf (stderr, "             for demonstration purposes (such as watching the A register count\n");
            fprintf (stderr, "             up or down in real time), invoke this panel application with the\n");
            fprintf (stderr, "             -abx argument on the command line.\n\n");
            fprintf (stderr, "If an optional configuration file is specified, then that will be used for the\n");
            fprintf (stderr, "simulator configuration, otherwise a default one will be created.\n");
            return 0;
        }
    }
    if (sim_config == NULL) {
        FILE *f;

        sim_config = "../sds_panel.ini";
        f = fopen (sim_config, "w");
        if (f == NULL) {
            fprintf (stderr, "Can't open '%s': %s\n", sim_config, strerror (errno));
            return 0;
        }
        if (debug) {
            fprintf (f, "set debug -n -a -p -f simulator.dbg\n");
            fprintf (f, "set rem-con debug=XMT;RCV;MODE;REPEAT;CMD\n");
            fprintf (f, "set SCP-PROCESS debug\n");
            fprintf (f, "set INT-CLOCK debug\n");
        }
        fprintf (f, "set cpu history=400\n");

        fprintf (f, "set console telnet=buffered\n");
        fprintf (f, "set console -u telnet=1927\n");
        fprintf (f, "set console telnet=connect\n");
        if (throttle) {
            fprintf (f, "# throttle to get a more realistic view, slower < 250 > faster\n");
            fprintf (f, "set throttle 200/1\n");
        }
        fprintf (f, ";set tto wait=80\n");
        fprintf (f, ";att cr /Users/admin/sds/sds-kit/fortdeck\n");
        fprintf (f, "att cr /Users/admin/sds/sds-kit/850648/850648-84,/Users/admin/sds/sds-kit/tests/bo\n");
        fprintf (f, "att cp card-punch\n");
        fprintf (f, "att lp line-printer\n");
        fprintf (f, "att mt0 mt0\n");
        fprintf (f, "att mt1 mt1\n");
        fprintf (f, "att mt2 mt2\n");
        fprintf (f, "att mt3 mt3\n");

        fprintf (f, "dep A 234\n");
        fprintf (f, "dep B 77777777\n");
        fprintf (f, "dep X 0\n");
        fprintf (f, "dep P 100\n");
        fprintf (f, "dep C 02000777\n");
        fprintf (f, "dep 100 NOP  100\n");
        fprintf (f, "dep 101 NOP  200\n");
        fprintf (f, "dep 102 NOP  300\n");
        fprintf (f, "dep 103 NOP  400\n");
        fprintf (f, "dep 104 BRU 100\n");
        fprintf (f, "dep 105 EOM 02001\n");
        fprintf (f, "dep 106 WIM 00077\n");
        fprintf (f, "dep 107 DSC 0\n");
        fprintf (f, "dep 110 HLT\n");
        fprintf (f, "dep 111 BRU 100\n");

        fprintf (f, "dep 102 brx 104\n");
        fclose (f);
    }
    panel = sim_panel_start_simulator_debug (sim_path,
        sim_config, 0,debug? "frontpanel.dbg" : NULL);
    if (!panel) {
        printf ("Error starting simulator %s with config %s: %s\n", sim_path, sim_config, sim_panel_get_error());
        return 0;
    }

    if (sim_panel_set_sampling_parameters_ex (panel, 11, 5, 3)) {
        printf("Error setting sampling parameters: %s\n",sim_panel_get_error());
    }
    if (sim_panel_add_register_bits (panel, "P",  NULL, 14, P)) {
        printf ("Error adding register 'P': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register_bits (panel, "C",  NULL, 24, C)) {
        printf ("Error adding register 'C': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register_bits (panel, "A",  NULL, 24, A)) {
        printf ("Error adding register 'A': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register_bits (panel, "B",  NULL, 24, B)) {
        printf ("Error adding register 'B': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register_bits (panel, "X",  NULL, 24, X)) {
        printf ("Error adding register 'X': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register (panel, "UAR[0]",  NULL, sizeof(UAR), &UAR)) {
        printf ("Error adding register 'UAR': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register (panel, "EM2",  NULL, sizeof(EM2), &EM2)) {
        printf ("Error adding register 'EM2': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register (panel, "EM3",  NULL, sizeof(EM3), &EM3)) {
        printf ("Error adding register 'EM3': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register (panel, "ION",  NULL, sizeof(ION), &ION)) {
        printf ("Error adding register 'ION': %s\n", sim_panel_get_error());
    }
    if (sim_panel_add_register (panel, "OV",  NULL, sizeof(OV), &OV)) {
        printf ("Error adding register 'OV': %s\n", sim_panel_get_error());
    }

    if (sim_panel_set_display_callback_interval(panel,&displaycallback, NULL, 10000)) {
        printf ("Error setting automatic display callback: %s\n", sim_panel_get_error());
    }


    cpnl_quit=0;
    while((!cpnl_quit) && (sim_panel_get_state (panel) != Error) && ( SDL_WaitEvent( &e ) != 0 )) {
        switch (e.type) {
            case SDL_QUIT:
                cpnl_quit++;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    cpnl_mousedown.x = (int)(e.button.x * cpnl_sclW);
                    cpnl_mousedown.y = (int)(e.button.y * cpnl_sclH);
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    cpnl_mouseup.x = (int)(e.button.x * cpnl_sclW);
                    cpnl_mouseup.y = (int)(e.button.y * cpnl_sclH);
                    cpnl_decodepress(&cpnl_mousedown,&cpnl_mouseup);
                }
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    cpnl_sclW = (double)cpnl_imgW / (double)e.window.data1;
                    cpnl_sclH = (double)cpnl_imgH / (double)e.window.data2;
                    cpnl_work_surface = SDL_GetWindowSurface(cpnl_window);
                    if(cpnl_work_surface == 0) {
                        fprintf(stderr,"Failed to get the surface from the window\n");
                        cpnl_quit++;
                    }
                }
                else {
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                        cpnl_quit++;
                    }
                    else {
                        if (e.window.event == SDL_WINDOWEVENT_ENTER) {
                            SDL_RaiseWindow (cpnl_window);
                        }
                    }
                }
                break;
            case SDL_DROPFILE:
                SDL_GetMouseState(&drop.x,&drop.y);
                drop.x = (int)(drop.x * cpnl_sclW);
                drop.y = (int)(drop.y * cpnl_sclH);
                if (drop.x < ctl[1].at.x + 28) {
                    if (drop.x > (ctl[1].at.x - 40)) {
                        if (drop.y > ctl[1].at.y)
                            cpnl_bootunit = "MT0";
                        else
                            cpnl_bootunit = "PTR";
                    }
                    else
                        break;
                }
                else {
                    if (drop.x < ctl[2].at.x + 40) {
                        if ((drop.y - ctl[2].at.y) > 0)
                            cpnl_bootunit = "RAD";
                        else
                            cpnl_bootunit = "CR";
                    }
                    else
                        break;
                }
                printf("Mounting %s on %s\r\n",e.drop.file,cpnl_bootunit);
                if (sim_panel_mount(panel,cpnl_bootunit," ",e.drop.file))
                    printf("error on mount %s, %s\n",e.drop.file,
                           sim_panel_get_error());
                break;
        }
    }
    if (sim_panel_get_state (panel) == Error)
        fprintf(stderr, "Panel Error: %s\n", sim_panel_get_error ());
    sim_panel_destroy (&panel);
    SDL_Quit();
    return 0;
}


/*
PNG load routine
http://zarb.org/~gc/html/libpng.html
*/
// compose a surface uint32_t from rr,gg,bb values
static uint32_t surface_rgb_color(uint32_t r, uint32_t g, uint32_t b)  /* r,g,b are 8bit! */
{
    uint32_t color;

    // rgb to 16 bits
    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
    r = r << 8; g = g << 8; b = b << 8;

    color = sim_end ?
               (0xFF000000 | ((r & 0xFF00) << 8) | (g & 0xFF00) | ((b & 0xFF00) >> 8)) :
               (0x000000FF | (r  & 0xFF00) | ((g & 0xFF00) << 8) | ((b & 0xFF00) << 16));
    return color;
}


/* Reading from memory: http://pulsarengine.com/2009/01/reading-png-images-from-memory/ */

void read_png_image(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead)
{
struct image_file *imagefile = (struct image_file *)png_get_io_ptr(png_ptr);

if ((imagefile->position + byteCountToRead) > imagefile->size)
    byteCountToRead = imagefile->size - imagefile->position;
memcpy (outBytes, &imagefile->data[imagefile->position], byteCountToRead);
imagefile->position += byteCountToRead;
}

// read png file, malloc and populate surface with image
// return it's address or zero if error
// set    WW, HH vith size of image

SDL_Surface* read_png_file(image_file *imagefile, int * WW, int * HH)
{
    png_byte color_type;
    png_byte bit_depth;

    png_structp png_ptr;
    png_infop info_ptr;
    int number_of_passes;
    png_bytep * row_pointers;

    png_bytep row;
    png_bytep px;

    int H, W, x, y, rr, gg, bb, aa, p;
    uint32_t col;
    uint32_t *pngsurface;

    *HH = *WW = 0;

    /* initialize stuff */
    if (!png_check_sig((png_const_bytep) imagefile->data, 8))
        return 0;

    /* initialize stuff */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fprintf(stderr, "Image file %s png_create_read_struct failed\n", imagefile->file_name);
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fprintf(stderr, "Image file %s png_create_info_struct failed\n", imagefile->file_name);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return 0;
    }

    png_set_read_fn(png_ptr, imagefile, read_png_image);

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Image file %s Error during init_io\n", imagefile->file_name);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    png_set_sig_bytes(png_ptr, 8);
    imagefile->position = 8;
    png_read_info(png_ptr, info_ptr);

    W = png_get_image_width(png_ptr, info_ptr);
    H = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    *HH=H, *WW=W;

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt
 
    if(bit_depth == 16) png_set_strip_16(png_ptr);
    if(color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
 
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
 
    // These color_type don't have an alpha channel then fill it with 0xff.
    if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png_ptr);

    number_of_passes = png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);
    
    /* read file */
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "PNG file %s Error during read_image\n", imagefile->file_name);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }
        
    row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * H);
    for (y=0; y<H; y++) {
        row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));
    }

    png_read_image(png_ptr, row_pointers);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    imagefile->position = 0;

    // alocate mem for read image surface
    pngsurface = (uint32_t *)malloc(W*H*sizeof(*pngsurface));
    if (!pngsurface) {
        fprintf(stderr, "PNG file %s surface malloc error\n", imagefile->file_name);
        return 0;
    }
    p = 0;

    for(y = 0; y < H; y++) {
        row = row_pointers[y];
        for(x = 0; x < W; x++) {
            px = &(row[x * 4]); // get pixel at x,y
            rr = px[0]; gg = px[1]; bb = px[2]; aa = px[3]; // extract RGBA, A=Alpha
            col = surface_rgb_color(rr,gg,bb);
            pngsurface[p++] = col;
        }
    }

    // free png mem
    for (y=0; y<H; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    return SDL_CreateRGBSurfaceFrom(pngsurface,W,H,32,6000,
        0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
}

#else /* !defined(HAVE_LIBSDL) */
int main (int argc, char **argv) {
fprintf (stderr, "SDL Video Support unavailable\n");
}
#endif
