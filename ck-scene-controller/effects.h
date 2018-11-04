enum {
  	MODE_BLACKOUT = 0,
  	MODE_FIXED = 1,
  	MODE_WASH = 2,
  	MODE_RAINBOW = 3,
  	MODE_RANDOM = 4,
  	MODE_LFSR_RANDOM = 5,
    MODE_WHITE = 6
};

void InitBlackout (void);
void RunBlackout (void);

void InitFixed (uint8_t r, uint8_t g, uint8_t b);
void RunFixed (void);

void InitColorWash (uint8_t direction, uint16_t step);
void RunColorWash (void);

void InitRainbow (uint8_t direction, uint16_t step, uint16_t offset);
void RunRainbow (void);

void InitRandomColor (uint16_t delay);
void RunRandomColor (void);

void InitLfsrRandom (uint16_t delay);
void RunLfsrRandom (void);

void InitWhite (uint8_t level);
void RunWhite (void);
