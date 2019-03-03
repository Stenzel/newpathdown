# A New Path Down
Maybe a new approach to resampling

This is resampling with a little twist. I honestly don't know if this idea is new, but I could not find any prior implementation. After discovering this method, I figured out I have been doing it wrong for decades, so with the risk of publishing something that is already common knowledge and making a fool of myself, here some lengthy explanation:

## Resampling
Resampling is required whenever we want to play back sampled audio with transposition.
Luckily, sampling is an exact representation of band limited audio, so the continuous signal can be perfectly reconstructed from the samples.
The bad news though is that this requires an infinite summation, which is a bit impractical in most cases.

For resampling there are basically two versions:
Upsampling is resampling with a higher rate. Pitch goes down if you play back audio upsampled - confusing, I know. Downsampling is resampling with a lower rate, and pitch goes up. 

## Upsampling
Here the best would be to evaluate the infinite summation formula for each time t to get the resampled sample. To avoid infinite summation, we use a windowed sinc filter of finite length. We'll need this filter sampled at different fractional positions in between samples, so the usual approach is to have a bank of say 256 filters and interpolate between them. Or just take the nearest, this works too. As the bandwidth is increased, there is no need to fight aliasing. This method is quite standard, and I used it for upsampling and downsampling. But no more!

## Downsampling
Downsampling has different requirements, the signal has to be bandlimited to half the new sampling rate, which is lower now. Luckily we can combine the polyphase reconstruction filter with a sinc style lowpass, but the lowpass cutoff frequency depends on the downsampling ratio. Lower cutoff requires a longer filter, as a rule of thumb each input sample should contribute to at least two output samples, better more, so the filter length needs to be minimum twice the downsampling ratio. With the filter size and cutoff changing, we also need to have as many samples in direct access as the filter length, which is a limiting factor for many applications where samples are streamed. There is however a simple solution to all these problems of downsampling, so simple that in hindsight I cannot grasp how I could ever be so stupid to miss it.

## FIR Filtering
For normal FIR filtering, each output sample y[n] is a linear combination of N input samples x[n-N/2...n+N/2-1] . But that does not mean we have to calculate it that way. What if we take each input sample x[n] and add the contribution to the output samples x[n-N/2...n+N/2-1] ? This works just as well, the contribution from x to y is just the filter in reverse order. If we implement this, we need a buffer of output samples, not input samples. Every step, one output sample is flushed out and a zero is shifted in. (please don't really shift a buffer, it hurts my eyes every time I see such code!)

## The New Method
If this scheme is used for downsampling, something strange happens - filter length and cutoff frequency are magically adjusted, and there is no need to increase buffer size with higher downsampling ratios. Calculation will take more time for large downsampling ratios, but that's what you would expect from a longer filter. As input and output sample rates differ, the algorithm is a bit different: 
The contribution of x[n] to the N buffered outputs y[0...N] is added. Take the time index m = n/decimation_ratio in the grid of the target rate and use the fractional part to to get the proper polyphase filter. If the integer part of m changes, output a sample and shift in a zero. All the troubles with downsampling disperse! 

However, this cannot be used for upsampling. For a flexible system with time varying resampling ratios, like every sample player, it is mandatory to support up- and downsampling, so if these cannot be combined, it would make little sense. Luckily such a combination is possible, but it requires some special code for the transitions. 

The code here demonstrates this combination of conventional upsampling, the new downsampling and transitions from either to the other. If none of this writing makes sense to you, maybe the code will. It should however by no mean be considered production quality code, in the current form it is more a proof of concept. 

All of this is quite suitable for SIMD optimizations, code for this will appear here in the near future.