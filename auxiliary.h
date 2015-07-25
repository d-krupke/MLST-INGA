#ifndef AUXILIARY_H
#define AUXILIARY_H
#include <stdio.h>
#include <stdlib.h>
#include "lib/random.h"

//returns a random float x with a<=x<=b
float getRandomFloat(float a, float b){
	static uint8_t isInited = 0;
	if(isInited==0){ random_init((linkaddr_node_addr.u8[0]<<8) | linkaddr_node_addr.u8[1]); isInited = 1;}
	return a+(b-a)*((float)random_rand()/RANDOM_RAND_MAX);
}

#endif
