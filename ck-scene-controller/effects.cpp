//-----------------------------------------------------------------------------
// includes
//

#include "Particle.h"
#include "controller.h"
#include "effects.h"


//-----------------------------------------------------------------------------
// defines
//


//-----------------------------------------------------------------------------
// prototypes
//

// color # (0-1535) to RGB conversion functions
static void xlatestate (void);
static void xlatetoall (void);
static void xlatetoone (uint8_t which);

// color increment / decrement by step functions
static void AddDelayToColor (void);
static void SubDelayFromColor (void);


//-----------------------------------------------------------------------------
// constants
//

#define NUM_RANDOM_COLORS 134

static const uint32_t randomTable[NUM_RANDOM_COLORS] = {
    0xFF0017, 0x004BFF, 0xFF8400, 0x6800FF, 0x00FFB8, 0x004BFF,
    0xFF2400, 0xBBFF00, 0xFF009B, 0x00FF40, 0xFF002F, 0x97FF00,
    0xFF006B, 0x4FFF00, 0xFF0047, 0x4400FF, 0x00FF04, 0xFF006B,
    0x3800FF, 0xF7FF00, 0xFF0017, 0xD400FF, 0x2BFF00, 0x5C00FF,
    0x00FF94, 0x9400FF, 0x00FF94, 0xFF00FB, 0x003FFF, 0xFF7800,
    0x00FFDC, 0xDC00FF, 0x00FFDC, 0xEBFF00, 0xFF0C00, 0xEC00FF,
    0x1FFF00, 0xD400FF, 0x006FFF, 0xFF0077, 0x00FF88, 0xA400FF,
    0xEBFF00, 0x00B7FF, 0xFF00BF, 0xFF3C00, 0x004BFF, 0xFF1800,
    0x00FFFF, 0xFFB400, 0xE000FF, 0x0003FF, 0x7FFF00, 0x0033FF,
    0x00FF64, 0xFF6C00, 0x67FF00, 0x001BFF, 0xFF00BF, 0xC7FF00,
    0x00DBFF, 0xEBFF00, 0x0057FF, 0xFF6000, 0x0093FF, 0xAFFF00,
    0x0003FF, 0xFF0083, 0xF7FF00, 0xD400FF, 0xFF0C00, 0x8000FF,
    0x00B7FF, 0x43FF00, 0x00FFAC, 0xFF002F, 0xC800FF, 0xF7FF00,
    0xFF1800, 0xBC00FF, 0x009FFF, 0x73FF00, 0xFF9C00, 0xFF0047,
    0xFF9C00, 0x6800FF, 0xFFD800, 0x00FFFF, 0xEBFF00, 0x4400FF,
    0xFF006B, 0xC7FF00, 0xFF003B, 0x00FF58, 0x0093FF, 0x4FFF00,
    0xFF00B3, 0xFFA800, 0x00DBFF, 0xA400FF, 0xDFFF00, 0xFF0047,
    0xBBFF00, 0xFF0053, 0xBC00FF, 0xFF6C00, 0xFF005F, 0x9800FF,
    0xFFE400, 0xFF0000, 0xFF00EF, 0x00B7FF, 0xDFFF00, 0xFF1800,
    0x07FF00, 0xFF00E3, 0xDFFF00, 0x2C00FF, 0xFF00B3, 0x00FF34,
    0x3400FF, 0x00FF34, 0x006FFF, 0xFF005F, 0x2000FF, 0x67FF00,
    0xFF9C00, 0x00FFE8, 0xFF4800, 0x009FFF, 0x00FF40, 0xFF005F,
    0x37FF00, 0x00ABFF
};


//-----------------------------------------------------------------------------
// globals
//

// hue to rgb translation variables
static uint8_t xlatehi, xlatelo;
static uint8_t xlater, xlateg, xlateb;

// fixed mode variables
static uint8_t fixedRed, fixedGreen, fixedBlue;

// color wash and rainbow variables
static uint8_t washDirection;
static uint16_t washStep;
static uint32_t washColor;

// rainbow variables
static uint16_t rainbowOffset;

// random variables
static uint16_t randomDelay;
static uint16_t randomTimer;
static uint8_t randomIndex;

// random lfsr variables
static uint8_t randomLfsr = 2;				// 8-bit lfsr for lfsr random color mode
static uint32_t randomColor = 0;


// white variables
static uint8_t whiteLevel;

//-----------------------------------------------------------------------------
// color # (0-1535) to RGB conversion functions
//
// given a color number from 0-1535 in { xlatehi, xlatelo }, convert it to its
// RGB triple and store the triple in xlater, xlateg, xlateb
//
// 0 R: on   G: off  B: inc
// 1 R: dec  G: off  B: on
// 2 R: off  G: inc  B: on
// 3 R: off  G: on  B: dec
// 4 R: inc  G: on   B: off
// 5 R: on   G dec   B: off
//

void xlatestate (void)
{
  	switch (xlatehi) {
    		case 0: xlater = 0xff;           xlateg = 0x00;           xlateb = xlatelo; break;
    		case 1: xlater = 0xff - xlatelo; xlateg = 0x00;           xlateb = 0xff; break;
    		case 2: xlater = 0x00;           xlateg = xlatelo;        xlateb = 0xff; break;
    		case 3: xlater = 0x00;           xlateg = 0xff;           xlateb = 0xff - xlatelo; break;
    		case 4: xlater = xlatelo;        xlateg = 0xff;           xlateb = 0x00; break;
    		case 5: xlater = 0xff;           xlateg = 0xff - xlatelo; xlateb = 0x00; break;
  	}
}

void xlatetoall (void)
{
  int i;
  uint8_t *l;

  l = levels;
  for (i = 0; i < RGB_LIGHTS; i++) {
      *l++ = xlater;
      *l++ = xlateg;
      *l++ = xlateb;
  }
}

static void xlatetoone (uint8_t which)
{
  	levels[3 * which + 0] = xlater;
  	levels[3 * which + 1] = xlateg;
  	levels[3 * which + 2] = xlateb;
}


//-----------------------------------------------------------------------------
// color increment / decrement by step functions
//

void AddDelayToColor (void)
{
  	washColor += washStep;
  	if (washColor >= 393216) {
  		  washColor -= 393216;
  	}
}

void SubDelayFromColor (void)
{
    washColor -= washStep;
    if (washColor >= 393216) {
        washColor += 393216;
    }
}


//-----------------------------------------------------------------------------
// blackout mode
//

void InitBlackout (void)
{
}

void RunBlackout (void)
{
    int i;

    for (i = 0; i < CHANNELS; i++) {
        levels[i] = 0x00;
    }
}


//-----------------------------------------------------------------------------
// fixed color mode
//

void InitFixed (uint8_t r, uint8_t g, uint8_t b)
{
    fixedRed = r;
    fixedGreen = g;
    fixedBlue = b;
}

void RunFixed (void)
{
    int i;
    uint8_t *l;

    l = levels;
    for (i = 0; i < RGB_LIGHTS; i++) {
        *l++ = fixedRed;
        *l++ = fixedGreen;
        *l++ = fixedBlue;
    }
}


//-----------------------------------------------------------------------------
// color wash mode
//

void InitColorWash (uint8_t direction, uint16_t step)
{
    washDirection = direction;
    washStep = step;
}

void RunColorWash (void)
{
  	if (washDirection == 0) {
  		  AddDelayToColor ();
  	} else {
  		  SubDelayFromColor ();
  	}
  	xlatehi = (washColor >> 16) & 0xff;
  	xlatelo = (washColor >> 8) & 0xff;
  	xlatestate ();
  	xlatetoall ();
}


//-----------------------------------------------------------------------------
// rainbow mode
//

void InitRainbow (uint8_t direction, uint16_t step, uint16_t offset)
{
    washDirection = direction;
    washStep = step;
    rainbowOffset = offset;
}

void RunRainbow (void)
{
    int i;
    uint16_t tmp;

    if (washDirection == 0) {
        AddDelayToColor ();
    } else {
        SubDelayFromColor ();
    }

    xlatehi = (washColor >> 16) & 0xff;
    xlatelo = (washColor >> 8) & 0xff;

    for (i = 0; i < RGB_LIGHTS; i++) {
        xlatestate ();
        xlatetoone (i);
        tmp = (((unsigned short)xlatehi) << 8) | (unsigned short)xlatelo;
        tmp += rainbowOffset;
        if (tmp >= 1536) {
            tmp -= 1536;
        }
        xlatehi = tmp >> 8;
        xlatelo = tmp & 0xff;
    }
}


//-----------------------------------------------------------------------------
// random color mode
//

void InitRandomColor (uint16_t delay)
{
    randomDelay = delay;
    randomTimer = 0xFFFF;
    randomIndex = 0;
}

void RunRandomColor (void)
{
    if (randomTimer >= randomDelay) {
        // reset timer
        randomTimer = 0;

        // set color
        randomColor = randomTable[randomIndex];
        xlater = randomColor >> 16;
        xlateg = randomColor >> 8;
        xlateb = randomColor & 0xff;
        xlatetoall ();

        // advance index
        randomIndex = randomIndex + 1;
        if (randomIndex >= NUM_RANDOM_COLORS) {
            randomIndex = 0;
        }
    } else {
        randomTimer++;
    }
}


//-----------------------------------------------------------------------------
// lfsr random color mode
//

void InitLfsrRandom (uint16_t delay)
{
    randomDelay = delay;
    randomTimer = 0xFFFF;
    randomColor = 0;
}

void RunLfsrRandom (void)
{
    if (randomTimer >= randomDelay) {
        // reset timer
        randomTimer = 0;

        // set color
        // pick next random color (rndclr = (rndclr + 384 + (3*[0-255])) % 1536
        if (randomLfsr & 1) {
            randomLfsr = (randomLfsr >> 1) ^ 0xB4;
        } else {
            randomLfsr = (randomLfsr >> 1);
        }

        randomColor += 384;
        randomColor += randomLfsr;
        randomColor += randomLfsr;
        randomColor += randomLfsr;
        if (randomColor >= 1536) {
            randomColor -= 1536;
        }

        xlatehi = randomColor >> 8;
        xlatelo = randomColor & 0xff;
        xlatestate ();
        xlatetoall ();
    } else {
        randomTimer++;
    }
}


//-----------------------------------------------------------------------------
// white mode
//

void InitWhite (uint8_t level)
{
    whiteLevel = level;
}

void RunWhite (void)
{
    int i;

    for (i = 0; i < CHANNELS; i++) {
        levels[i] = 0xff;
    }
}


#ifdef __JUNK__XYZ__

unsigned short randomColor = 0;				// current random color state for lfsr random color mode
unsigned short randomColors[RGB_CHANNELS];
unsigned short randomLfsr16 = 2;			// 8-bit lfsr for lfsr random color mode

#endif
