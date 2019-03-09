/*
 Resampling using different methods for up- and downsampling
 (c) 2019 Stefan Stenzel 
 stefan at ioptigan dot com 

 See license.txt for terms of use

*/

#include <math.h>
#include <stdio.h>

#include "resampler.h"

//set transposition in semitones
void resampler::transpose(float semitone)
{
    setratio(exp2f(semitone / 12.0f));
}

//set resampling ratio
void resampler::setratio(float newratio)
{
    ratio     = newratio;
    downscale = 1.0f / ratio;
    deltaup   = 0x1p25f * ratio;
    deltadown = 0x1p25f / ratio;
    
    if(deltadown > 0x2000000 && state == DOWN) state = DOWN2UP;
    if(deltaup   > 0x2000000 && state == UP)   state = UP2DOWN;
}

// conventional approach, used for upsampling
const float *resampler::up16(const float *src, float *dst)
{
    uint32_t ixsmp = (counter >> 25) & 0x07;
    for(int i=0; i<16; i++)
    {
        counter += deltaup;
        uint32_t ixnew = (counter >> 25) & 0x07; 
        
        //if(ixnew != ixsmp)    in[ixsmp] = *src++;
        while(ixnew != ixsmp)                           // this allows this routine to be uses for slight upsampling as well.
        {
            in[ixsmp] = *src++;
            ixsmp = (ixsmp + 1) & 0x07;
        }

        uint32_t ixflt = (counter >> 17) & 0x0FF; 
        float fraction = (counter & 0x1FFFF) * 0x1p-17f;
        float *sinca = &Sinc[ixflt][0];
        float *sincb = &Sinc[ixflt+1][0];
    
        float acca = 0.0f;
        float accb = 0.0f;
        for(int i=0; i<8; i++) 
        {   
            acca += in[ixsmp] * *sinca++;
            accb += in[ixsmp] * *sincb++;
            ixsmp = (ixsmp + 1) & 0x07;
        }
        *dst++ = acca + (accb - acca) * fraction;
    }
    return src;
}

//the new method for downsampling
const float *resampler::down16(const float *src, float *dst)
{
    uint32_t ixsmp = (counter >> 25) & 0x07;
    float *end = dst + 16;
    do {
        uint32_t ixflt = (counter >> 17) & 0x0FF; 
        float fraction = (counter & 0x1FFFF) * 0x1p-17f;
        float *sinca = &Sinc[ixflt][0];
        float *sincb = &Sinc[ixflt+1][0];
        float x = *src++ * downscale;
    
        float xb = x * fraction;
        float xa = x - xb;
    
        for(int i=0; i<8; i++) 
        {
            out[ixsmp] += xa * *sinca++;
            out[ixsmp] += xb * *sincb++;
            ixsmp = (ixsmp + 1) & 0x07;
        }
        counter += deltadown;
        uint32_t ixnew = (counter >> 25) & 0x07; 
        if(ixnew != ixsmp)  
        {
            *dst++ = out[ixsmp];    
            out[ixsmp] = 0.0f;
            ixsmp = ixnew;
        }
    } while(dst < end);
    return src;
}

// used for fading in new version oer eight samples
// cannot name things "new", so I used German 
void fade(float *dst, float *alt, float *neu)
{
    for(int i=0; i<8; i++) *dst++ = *alt++;
    
    neu += 8;
    
    const float step = 1.0f/9.0f;
    float sc = step;
    for(int i=0; i<8; i++)
    {
        float a = *alt++;
        *dst++ = a + (*neu++ - a) * sc;
        sc += step;
    }
}

//transition from downsampling to upsampling
const float *resampler::down2up16(const float *src, float *dst)
{
    float neu[16];
    uint32_t nextcount = ~counter;
    down16(src,dst);
    state = UP;
    counter = nextcount; 
    for(int i=0; i<8; i++) in[i] = 0.0f;
    src = up16(src,neu);
    fade(dst,dst,neu);
    return src;
}

//transition from upsampling to downsampling
const float *resampler::up2down16(const float *src, float *dst)
{
    float neu[16];
    uint32_t nextcount = ~counter;
    up16(src,dst);
    state = DOWN;
    counter = nextcount;  
    for(int i=0; i<8; i++) out[i] = 0.0f;
    src =  down16(src,neu);
    fade(dst,dst,neu);
    return src;
}

//process a block of 16 samples
//eats a variable number of samples
//returns pointer behind the last eaten sample 
const float *resampler::process16(const float *src, float *dst)
{
    switch(state)
    {
        case UP:        return up16(src,dst);
        case DOWN:      return down16(src,dst);
        case UP2DOWN:   return up2down16(src,dst);
        case DOWN2UP:   
        default:        return down2up16(src,dst);      
    }   
}

#define WCF0        0.344109006115
#define WCF1       -0.508650393885
#define WCF2        0.193119398005
#define WCF3       -0.028578010236
#define DILATION    0.83

//build the windowed sinc filter table
void resampler::buildsinc(void)
{
    double step  = M_PI * DILATION;
    double wstep = M_PI * 2.0/(8 + 1.0);
    
    for(int i=0; i<=256; i++)
    {
        double fraction = 1.0f - i * 1.0/256;
        double arg  = (fraction - 8 * 0.5) * step;
        double warg = (fraction + 0.5) * wstep;
            
        for(int k=0; k<8; k++)
        {
            double  s = 1.0;
            if(arg) s = sin(arg)/arg;             // sinc - but avoid division by zero.
            
            // Apply the Stenzel Window to minimize DC error 
            double w = WCF0 + WCF1 * cos(warg) + WCF2 * cos(warg*2.0) + WCF3 * cos(warg*3.0);

            Sinc[i][k] = s * w;                   // save windowed tap  
            
            arg  += step;                         // increment sinc arg
            warg += wstep;                        // increment window arg
        }
    }
}

float resampler::Sinc[256+1][8];

//constructor...
resampler::resampler()
{
    counter  = 0;
    state = UP;
    setratio(1.0f);
    for(int i=0; i<8; i++) 
    {
        in[i]  = 0;
        out[i] = 0;
    }
    buildsinc();
}

