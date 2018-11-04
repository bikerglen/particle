#define RGB_LIGHTS      (16)
#define CHANNELS        (3*RGB_LIGHTS)

typedef struct {
    int mode;                     // mode select
    uint8_t red;                  // used by fixed mode
    uint8_t green;
    uint8_t blue;
    uint8_t direction;            // used by color wash and rainbow modes
    uint16_t step;
    uint16_t offset;              // used by rainbow mode
    uint16_t delay;               // used by both random color modes
    uint8_t level;                // used by white mode
} Scene;

extern uint8_t levels[CHANNELS];
