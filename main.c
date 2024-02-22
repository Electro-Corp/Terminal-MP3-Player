/*
    Terminal Mp3 player
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// There's some reason i made a struct for this but im not sure
typedef struct{
    char* fName;
    char* sName;
    char* sAuthor;
} MP3File;



int getSizeOfString(uint8_t* buffer, int offset);
void readString(char** dest, uint8_t* src, int offset);
void marqueeString(char** marquee);
void help();
void interrupted(int signal);
void cleanExit();

int main(int argv, char** args){
    // Init signal handler
    signal(SIGINT, interrupted);
    signal(SIGSEGV, interrupted);
    signal(SIGABRT, interrupted);
    // Remove cursor
    printf("\e[?25l");
    if(argv < 2){
        help();
    }
    FILE* fp = fopen(args[1], "r");
    if(!fp){
        printf("File %s not found!\n", args[1]);
        exit(-1);
    }
    fseek(fp, 0L, SEEK_END);
    uint8_t headerBuffer[5000];
    rewind(fp);
	fread(headerBuffer, sizeof(headerBuffer), 1, fp);
    
    // Check header
    uint8_t header[3];
    uint8_t mp3header[3] = {73, 68, 51};
    memcpy(header, headerBuffer, 3);

    if(memcmp(header, mp3header, 3) != 0){
        printf("Not an MP3 file!\n");
        exit(-1);
    }

    int headerReadOffset = 0;

    MP3File file;
    file.fName = args[1];

    // Find header frame
    while(sizeof(headerBuffer) > headerReadOffset){
        if(headerBuffer[headerReadOffset] == 84 && headerBuffer[headerReadOffset+1] == 73 && headerBuffer[headerReadOffset+2] == 84 && headerBuffer[headerReadOffset+3] == 50)
            break;
        headerReadOffset++;
    }


    // Header

    // Song name
    headerReadOffset += 11;
    readString(&(file.sName), headerBuffer, headerReadOffset);
    // Song artist
    headerReadOffset = 0;
    while(sizeof(headerBuffer) > headerReadOffset){
        if(headerBuffer[headerReadOffset] == 84 && headerBuffer[headerReadOffset+1] == 80 && headerBuffer[headerReadOffset+2] == 69 && headerBuffer[headerReadOffset+3] == 49)
            break;
        headerReadOffset++;
    }
    headerReadOffset += 11;
    readString(&(file.sAuthor), headerBuffer, headerReadOffset);

    // print out data
    //printf("%s:\n%s\n%s\n",file.fName, file.sName, file.sAuthor);

    // Init audio
    ma_result result;
    ma_engine engine;
    ma_sound sound;

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initilize miniaudio audio engine!\n");
        exit(-1);
    }

    result = ma_sound_init_from_file(&engine, args[1], 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        printf("Failed to play sound!\n");
        exit(-1);
    }

    ma_sound_start(&sound);
    float time = 0, length = 0, prevTime = 0;

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int maxDispLen = w.ws_col / 3;
    int prevLen = maxDispLen;


    ma_sound_get_cursor_in_seconds(&sound, &time);
    ma_sound_get_length_in_seconds(&sound, &length);

    char* line = (char*)malloc(maxDispLen * sizeof(char));
    char* marquee = (char*)malloc(maxDispLen * sizeof(char));

    int marqOffset = 0;
    // Do we have a name for the song?
    if(strlen(file.sName) > 0){
        for(int i = 0; i < strlen(file.sName); i++){
            marquee[i] = file.sName[i];
        }
        for(int i = strlen(file.sName); i < maxDispLen; i++){
            marquee[i] = ' ';
        }
    }else{
        // Use filename
        for(int i = 0; i < strlen(file.fName); i++){
            marquee[i] = file.fName[i];
        }
        for(int i = strlen(file.fName); i < maxDispLen; i++){
            marquee[i] = ' ';
        }
    }
    
    for(int i = 0; i < maxDispLen; i++){
        line[i] = '-';
    }

    while(time < length - 1){
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        maxDispLen = w.ws_col / 3;
        if(maxDispLen != prevLen){
            free(line);
            line = (char*)malloc(maxDispLen * sizeof(char));
            prevLen = maxDispLen;
            for(int i = 0; i < maxDispLen; i++){
                if(i < (int)time)
                    line[i] = '=';
                else
                    line[i] = '-';
            }
            // Clear line
            for(int i = 0; i < w.ws_col; i++){
                printf(" ");
            }
        }
        ma_sound_get_cursor_in_seconds(&sound, &time);
        if(time - prevTime > 0.3){ 
            marqueeString(&marquee);
            prevTime = time;
        }
        printf("[%s]\n", marquee);    
        printf("[%s] - %d/%d\r", line, (int)time, (int)length);
        printf("\x1b[1A");
        fflush(stdout);
        
        time = (maxDispLen - 0) * ((time - 0) / length) + 0;
        if(time > 1){
            line[(int)time - 1] = '=';
            
            line[(int)time] = '$';
        }
        ma_sound_get_cursor_in_seconds(&sound, &time);
    }

    free(line);
    free(marquee);

    cleanExit();

    return 0;
}

/*
    Get size of string in uint8_t buffer

    buffer: source
    offset: where the string supposedly starts
*/
int getSizeOfString(uint8_t* buffer, int offset){
    if(offset == 0) offset++;
    int i = 0;
    while(buffer[offset++] != 0)
        i++;
    return i;
}

/*
    Read string from uint8_t buffer
    
    dest: char to write to
    src: buffer source
    offset: offset in buffer to read from
*/
void readString(char** dest, uint8_t* src, int offset){
    int strSize = getSizeOfString(src, offset);
    char* tmp = (char*)malloc(strSize * sizeof(char));
    for(int i = offset; i < offset + strSize; i++){
        tmp[i - offset] = src[i];
    }
    *dest = tmp;
}

/*

*/
void marqueeString(char** marqueeT){
    // Create modifiable text
    char marquee[strlen(*marqueeT)];
    // Copy text over
    strcpy(marquee, *marqueeT);
    // Shift every character by 1 to the right (overflow the most right char to the first one)
    char tmp = marquee[0];
    marquee[0] = marquee[strlen(*marqueeT) - 1];
    for(int i = 1; i < strlen(*marqueeT); i++){
        char tmpcopy = marquee[i];
        marquee[i] = tmp;
        tmp = tmpcopy;
    }
    // Copy it back
    strcpy(*marqueeT, marquee);
}

void help(){
    printf("Terminal Mp3 Player: \n");
    printf("./mp3 [filename]\n");
    exit(-1);
}

void interrupted(int signal){
    if(signal == SIGSEGV) printf("Segmentation Fault!\n");  
    cleanExit();
}

void cleanExit(){
    printf("\e[?25h");
    exit(0);
}