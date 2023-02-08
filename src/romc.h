#ifndef _ROMC_H
#define _ROMC_H

#include <stdint.h>

int romc_encode(uint8_t data[], int size, uint8_t out[]);
void romc_decode(uint8_t* input, int size, uint8_t* out);

#endif