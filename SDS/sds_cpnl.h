/* sds_cpnl.h - control panel simulation  */

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

*/

typedef struct {
    int x;
    int y;
} SDSPoint;

typedef struct {
    char  name[32];
    SDSPoint at;           // click point pixel position in image
    SDSPoint dim;          // width, height of control image
    int image;          // surface to draw from
    int brite;          // brightness index, or display counter
} SDSControl;

enum tgls {
    PARITY,
    PAPER,
    CARD,
    TAPE,
    DRUM,
    RUN,
    STEP,
    HOLD,
    BKPT1,
    BKPT2,
    BKPT3,
    BKPT4,
    INTENBL,
    POWER,
    CHANNEL,
    CLEAR,
    START,
    REGISTER,
    MEMORYL,
    MEMORYR
};
   
#define NUMDISPTGLS INTENBL+1

/* Note:  ctl and toggle arrays are parallel and use
   the same enum.  */
SDSControl ctl[] = {
    {"",{315,580}},         // parity
    {"",{138,495}},         // fill paper
    {"",{195,495}},         // fill cards
    {"",},                  // fill mag tape
    {"",},                  // fill drum
    {"",{1030,580}},        // run
    {"",},                  // step
    {"",{1030,445}},        // hold
    {"",{530,580}},         // breakpoint 1
    {"",{610,580}},         // breakpoint 2
    {"",{706,580}},         // breakpoint 3
    {"",{798,580}},         // breakpoint 4
    {"",{260,580}},         // interrupt
    {"",{160,575}},         // power
    {"",{173,440}},         // io display select
    {"",{357,537}},         // clear
    {"",{978,428}},         // start
    {"",{1030,500}},        // register display select
    {"",{444,575}},         // memory clear
    {"",{881,576}}          // memory clear
};
    
SDSControl toggle[] = {
    {"PAR",{290,530},{55,110},3},
    {"PAPER",{100,450},{55,110},3},
    {"CARD",{180,450},{55,110},3},
    {"TAPE",{100,450},{55,110},2},
    {"DRUM",{180,450},{55,110},2},
    {"RUN",{1010,530},{50,110},3},
    {"STEP",{1010,530},{50,110},2},
    {"HOLD",{1010,380},{50,75},3},
    {"BKPT1",{510,540},{50,75},3},
    {"BKPT2",{600,540},{50,75},3},
    {"BKPT3",{690,540},{50,75},3},
    {"BKPT4",{780,540},{50,75},3},
    {"INT",{240,530},{55,110},3}
};

    
SDSPoint regbutton[] = {
    {385,542},
    {407,539},
    {432,539},
    {455,539},
    {482,540},
    {503,539},
    {525,538},
    {546,540},
    {573,540},
    {598,541},
    {624,542},
    {645,542},
    {664,541},
    {692,541},
    {713,542},
    {741,543},
    {764,543},
    {787,542},
    {808,542},
    {834,543},
    {857,543},
    {882,542},
    {903,542},
    {929,546}
    };
    
SDSControl pLamp[] = {
    {"",{621,427}},
    {"",{643,427}},
    {"",{667,427}},
    {"",{690,427}},
    {"",{712,427}},
    {"",{736,427}},
    {"",{759,427}},
    {"",{782,427}},
    {"",{806,427}},
    {"",{828,427}},
    {"",{852,427}},
    {"",{876,427}},
    {"",{899,427}},
    {"",{922,427}}
    };
SDSControl regLamp[] = {
    {"",{385,493}},
    {"",{409,493}},
    {"",{433,493}},
    {"",{455,493}},
    {"",{479,493}},
    {"",{503,493}},
    {"",{525,493}},
    {"",{549,493}},
    {"",{573,493}},
    {"",{596,493}},
    {"",{618,493}},
    {"",{643,493}},
    {"",{666,493}},
    {"",{690,493}},
    {"",{713,493}},
    {"",{735,493}},
    {"",{759,493}},
    {"",{783,493}},
    {"",{805,493}},
    {"",{828,493}},
    {"",{853,493}},
    {"",{876,493}},
    {"",{899,493}},
    {"",{923,493}}
    };
SDSControl unitLamp[] = {
    {"",{388,427}},
    {"",{412,427}},
    {"",{435,427}},
    {"",{457,427}},
    {"",{481,427}},
    {"",{504,427}}
    };
    
    
SDSControl misc[] = {
    {"EM3",{550,427}},
    {"EM2",{574,427}},
    {"halt",{317,427}},
    {"OV",{271,427}},
    {"parity",{314,493}},
    {"ION",{268,493}},
    {"PWR",{140,550}}
    };

SDSControl reg[] = {
    {"A",{470,349},{34,18}},
    {"B",{157,349},{34,18}},
    {"C",{199,349},{34,18}},
    {"X",{517,349},{34,18}}
};

SDSControl chan[] = {
    {"W",{560,349},{34,18}},
    {"Y",{116,349},{34,18}},
    {"C",{199,349},{34,18}},
    {"D",{240,349},{34,18}},
    {"E",{286,349},{34,18}},
    {"F",{335,349},{34,18}},
    {"G",{380,349},{34,18}},
    {"H",{421,349},{34,18}}
};
