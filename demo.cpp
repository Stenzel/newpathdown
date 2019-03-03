/*
	Demonstration for resampling
	(c) 2019 Stefan Stenzel
	
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resampler.h"

#define MAXSMP	0x100000
float Src[MAXSMP*8];

#define NSMP	0x200000
float Out[NSMP];

int wavload(const char *filename, float *dst, int maxsmp);
void wavsave(const float *smp, int nsmp, int rate, const char *filename);

const int Seq[17] = {0,0,0,0,4,7,11, 9, 7, 4, 0,2,5,9,12,11,11};

int main(int argc, char *argv[])
{
	resampler resample;
	
	const char *sample = "1234.wav";
	if(argc >= 2) sample = argv[1];

	int nsmp = wavload(sample,Src,MAXSMP);
	if(nsmp <= 0) return nsmp;
	
	//loop eight times - ridiculous, I know.
	for(int i=1; i<8; i++) memcpy(Src + nsmp * i,Src,nsmp * sizeof(float));
	
	float *loopend = Src + nsmp;
	
	float semitone = 0.0f;
	const float *src = Src;
	for(int i=0; i<NSMP; i+=16)
	{
		if(i < NSMP/2)
		{
			semitone = i * 0x3p-15f - 24.0f;
			resample.transpose(semitone);
		}
		else
		{
			int index = (i-NSMP/2)/(NSMP>>8);
			if(index < 17) 
			{
				resample.transpose(semitone + Seq[index]);
			}
			else 
			{
				semitone -= 0x4p-11f; 
				resample.transpose(semitone + 11.0f);
			}
		}
		src  = resample.process16(src,Out+i);
		while(src > loopend) src -= nsmp;
	}
	printf("saving output to file \"demo.wav\"\n");
	wavsave(Out,NSMP,44100,"demo.wav");
	return 0;
}

// quick & dirty mono wav file writer
void wavsave(const float *smp, int n, int rate, const char *filename) 
{
    int head[] = {'FFIR',4*n+36,'EVAW',' tmf',16,0x10003,rate,4*rate,0x200004,'atad',4*n};
    FILE *f = fopen(filename,"wb");
    fwrite(&head,44,1,f);
    fwrite(smp,n,4,f);
    fclose(f);
}

// quick & very dirty mono wav file reader
int wavload(const char *filename, float *dst, int maxsmp)
{
	FILE *f = fopen(filename,"rb");
	if(!f) return -1;

	uint32_t riff[3];
	fread(riff,4,3,f);
	if(riff[0] != 'FFIR' || riff[2] != 'EVAW') {fclose(f); return -2;}	

	char *w = (char *) malloc(riff[1]);	
	char *end = w + fread(w,1,riff[1],f);
	fclose(f); 
	
	char *chunk = w;
	uint32_t *data = 0,*fmt = 0,*cmp = (uint32_t *)chunk;
	
	do 	// get fmt and data chunk
	{
		if(cmp[0] == ' tmf') fmt  = cmp;
		if(cmp[0] == 'atad') data = cmp;
		chunk += cmp[1] + 8;
		cmp  = (uint32_t *)chunk;
	} while(chunk < end && (!data || !fmt));
	
	if(!data || !fmt) {free(w); return -3;}
	
	uint16_t *wavhead = (uint16_t *) &fmt[2];
	int nsmp =  data[1] / wavhead[6];
	if(nsmp > maxsmp) nsmp = maxsmp;
	int nchnl = wavhead[1];

	switch(wavhead[7])				// read first channel only, different formats supported
	{
		case 64:	
		{	double *src = (double *) &data[2];
			for(int i=0; i<nsmp; i++) {*dst++ = *src; src += nchnl;}
		} break;
		
		case 32:
		{	if(wavhead[0] == 3)
			{	float *src =  (float *) &data[2];
				for(int i=0; i<nsmp; i++)  {*dst++ = *src; src += nchnl;}
			} else {
				int32_t *src = (int32_t *) &data[2];
				for(int i=0; i<nsmp; i++) {*dst++ = *src * 0x1p-31f;  src += nchnl;}	
			}	
		} break;

		case 24:
		{	char *src = (char *) &data[2];
			for(int i=0; i<nsmp; i++)
			{ 	int32_t s = *((int32_t *) src) << 8;
				*dst++ = s * 0x1p-31f;
				src += nchnl * 3;
			}	
		} break;

		case 16:
		{	int16_t *src = (int16_t *) &data[2];
			for(int i=0; i<nsmp; i++) {*dst++ = *src * 0x1p-15f; src += nchnl;}
		} break;
		
		case 8:
		{	int8_t *src = (int8_t *) &data[2];
			for(int i=0; i<nsmp; i++) {*dst++ = *src * 0x1p-7f; src += nchnl;}
		} break;
		
		default: nsmp = 0;		// signal error
	}
	free(w);
	return nsmp;
}
