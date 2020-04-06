#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
//Break
#define BREAK_EVENT  (SDL_USEREVENT + 2)

static int window_width = 512;
static int window_height = 320;
static int frame_width = 512;
static int frame_height = 320;

static SDL_Window* window  = NULL;
static SDL_Renderer* render = NULL;
static SDL_Texture* texture = NULL;
static SDL_Rect rect;

static int thread_exit = 0;

//每隔40毫秒向SDL系统发送一个REFRESH_EVENT事件
int refresh_video(void *opaque){

	while (thread_exit == 0) {
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(40);
	}
	// thread_exit=0;
	//Break
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);
	return 0;
}

int main(int argc, char* argv[]) {
    int ret = -1;
    // int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO;
	int flags = SDL_INIT_VIDEO;
    ret = SDL_Init(flags);
    if (ret < 0) {
        printf("SDL init failed:%s\n", SDL_GetError());
        return -1;
    }
    printf("SDL init success\n");
    window = SDL_CreateWindow("忘了爱", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                         window_width, window_height, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
                                        
    if (!window) {
        printf("SDL create window failed\n");
        return -1;
    }
    printf("SDL create window success\n");

    // render = SDL_CreateRenderer(window, -1, 0);
	render = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!render) {
        printf("SDL create render failed\n");
        return -1;
    }
    printf("SDL create render success\n");

    Uint32 pixformat = SDL_PIXELFORMAT_NV21;
    texture = SDL_CreateTexture(render, pixformat, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
    if(!texture) {
        printf("SDL create texture failed\n");
        return -1;
    }
    printf("SDL create texture success\n");

    FILE* fp = fopen("/home/lihai/Desktop/xiaoli666/test/test.yuv", "rb");
    if (!fp) {
        printf("open file failed\n");
        return -1;
    }

	SDL_Thread *refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
	SDL_Event event;
	uint8_t* buffer = (uint8_t*)malloc(window_width * window_height * 3 / 2);
	int buffer_size = window_width * window_height * 3 / 2;
	while(1){
		//收取SDL系统里面的事件
		SDL_WaitEvent(&event);
		if(event.type==REFRESH_EVENT) {
			if (fread(buffer, buffer_size, 1, fp) != buffer_size){
				// Loop
				// fseek(fp, 0, SEEK_SET);
				// fread(buffer, buffer_size, 1, fp);
			}
 
			SDL_UpdateTexture(texture, NULL, buffer, 512);  
 
			//FIX: If window is resize
			rect.x = 0;  
			rect.y = 0;  
			rect.w = window_width;  
			rect.h = window_height;  
			
			SDL_RenderClear(render);   
			SDL_RenderCopy(render, texture, NULL, &rect);  
			SDL_RenderPresent(render);
			
		} else if(event.type==SDL_WINDOWEVENT){
			// 窗口相关事件

		} else if(event.type==SDL_QUIT){
			
			// 窗口关闭事件, 做一些业务处理
			thread_exit = 1;
		} else if(event.type==BREAK_EVENT){

			// 不知道啥子事件会触发。。。
			break;
		}
	}
	SDL_Quit();

	fclose(fp);
    return 0;
}
