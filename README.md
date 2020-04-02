ffmpeg compile:
./configure --prefix=./out --enable-gpl --enable-libx264 --enable-shared --enable-ffplay --disable-static --extra-cflags=-I../tmp/include --extra-ldflags=-L../tmp/lib
make
make install

