Window:



Linux:

ffmpeg compile:
./configure --prefix=../ffmpeg_build --enable-gpl --enable-libx264 --enable-shared --disable-static --enable-sdl --extra-cflags=-I../x264_build/include --extra-ldflags=-L../x264_build/lib --extra-cflags=-I../Sdl_build/include --extra-ldflags=-L../Sdl_build/lib
make 
make install

test compile:
gcc xxx.c -I../ffmpeg_build/include -I../x264_build/include -L../ffmpeg_build/lib -L../x264_build/lib -lx264 -lavcodec -lavformat -lavutil -lswresample


sudo apt-get install libsdl2-2.0
sudo apt-get install libsdl2-dev



dpdk

https://www.sdnlab.com/23023.html
https://www.cnblogs.com/yhp-smarthome/p/6705638.html
http://blog.chinaunix.net/uid-28541347-id-5785122.html
https://www.cnblogs.com/bakari/p/8404650.html