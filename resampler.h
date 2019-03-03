/*
 Resampling using different methods for up- and downsampling
 (c) 2019 Stefan Stenzel 
 stefan at ioptigan dot com	

 See license.txt for terms of use

*/
#pragma once

#include <stdint.h>

class resampler
{
	float ratio;
	float downscale;
	uint32_t counter;
	uint32_t deltaup;
	uint32_t deltadown;
	
	enum {UP,DOWN,UP2DOWN,DOWN2UP};
	int state;
	float in[8];
	float out[8];
	
	static float Sinc[256+1][8];
	void buildsinc(void);
	
	const float *down16(const float *src, float *dst);
	const float *up16(const float *src, float *dst);
	const float *down2up16(const float *src, float *dst);
	const float *up2down16(const float *src, float *dst);
public:	
	resampler();
	void setratio(float newratio);
	void transpose(float semitone);
	const float *process16(const float *src, float *dst);
};
