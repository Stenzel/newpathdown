# demo for resampler resampling

demo:	resampler.cpp demo.cpp
		g++ -O2 resampler.cpp demo.cpp -o $@
		./demo
		
clean:
		rm -f demo demo.wav		
		