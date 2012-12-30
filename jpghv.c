/*
 * jpghv  -  divide JPG image files into a directory according to
 *           landscape/portrait types
 *
 * Filename:   jpghv.c
 * Version:    0.2
 * Author:     itouhiro
 * Time-stamp: <Dec 30 2012>
 * License:    MIT
 * Copyright (c) 2012 Itou Hiroki
 */
#define PROGNAME "jpghv"
#define PROGVERSION "0.2"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strrchr(), strncat() */
#include <sys/stat.h> /* mkdir() */

#define MAXLINECHAR 1024
#define errret {free(src); size.width=-1; return size;}

/*
 * global variable
 */
const char *g_suffix[] = { ".jpg", ".JPG", ".jpeg" };

/*
 * struct
 */
typedef struct{
    long width;
    long height;
} size_type;
typedef enum {HORIZONTAL, VERTICAL} direction;


/*
 * prototype
 */
int getSuffix(char *fname);
size_type jpgGetSize(char *fname);
void moveFile(char *fname, char *dir);

/***********************************************************************/
int main(int argc, char *argv[]){
    int i;
    direction hv = HORIZONTAL;

    /*
     * print help & exit
     */
    if(argc<=1){
        printf("%s version %s\n  divide JPG image files into a directory according to landscape/portrait types\n"
               "usage:  %s <option> file..\n"
               "option: -h, -l, --horizontal, --landscape (default)\n"
               "                move horizontally-long images to 'landscape' sub directory.\n"
               "        -v, -p, --vertical, --portrait\n"
               "                move vertically-long images to 'portrait' sub directory.\n"
               "supported: ", PROGNAME, PROGVERSION, PROGNAME);
        for(i=0; i<sizeof(g_suffix)/sizeof(g_suffix[0]); i++)
            fprintf(stderr, "%s ", g_suffix[i]);
        printf("\n");
        fflush(stdout);
        exit(1);
    }

    /*
     * argument handling
     */
    for(i=1; i<argc; i++){
        size_type size;
        int ftype;

        if((strcmp(argv[i],"-h")==0) ||
           (strcmp(argv[i],"-l")==0) ||
           (strcmp(argv[i],"--horizontal")==0) ||
           (strcmp(argv[i],"--landscape")==0)){
            hv = HORIZONTAL;
            continue;
        }else if((strcmp(argv[i],"-v")==0) ||
                 (strcmp(argv[i],"-p")==0) ||
                 (strcmp(argv[i],"--vertical")==0) ||
                 (strcmp(argv[i],"--portrait")==0)){
            hv = VERTICAL;
            continue;
        }

        /*
         * filename check
         */
        if(getSuffix(argv[i]) >= 0){
            size = jpgGetSize(argv[i]); //jpg
        }else{
            size.width = -1;
        }

        /*
         * output result
         */
        if(size.width <= -1){
            fprintf(stdout, "Warning: '%s' is not an image file ?\n", argv[i]);
        }else{
            if(hv == HORIZONTAL && size.width > size.height){
                moveFile(argv[i], "landscape");
#ifdef DEBUG
            fprintf(stderr, "%s w=%d, h=%d (landscape)\n", argv[i], size.width, size.height);
#endif /* DEBUG */
            }else if (hv == VERTICAL && size.width < size.height){
                moveFile(argv[i], "portrait");
#ifdef DEBUG
            fprintf(stderr, "%s w=%d, h=%d (portrait)\n", argv[i], size.width, size.height);
#endif /* DEBUG */
            }
        }
    }//for(i
    exit(0);
}


int getSuffix(char *fname){
    char *suf;
    int sufnum;
    int i;

    suf = strrchr(fname, '.');
    if(suf == NULL) return -1;
    sufnum = sizeof(g_suffix) / sizeof(g_suffix[0]); //suffix number
    for(i=0; i<sufnum; i++){
        if(strcmp(suf, g_suffix[i]) == 0)
            return i;
    }
    return -1;
}


size_type jpgGetSize(char *fname){
    size_type size;
    unsigned char *p, *pmax;
    unsigned char *src = NULL;
    int flagsize = 0;

    //file open
    {
        FILE *fin;
        struct stat info;

        if((fin=fopen(fname,"rb")) == NULL) errret;
        if(stat(fname, &info) != 0) errret;
        if((src=malloc(info.st_size)) == NULL) errret;
        if(fread(src, 1, info.st_size, fin) != info.st_size){
            free(src); errret;
        }
        fclose(fin);
        pmax = src + info.st_size;
    }

    /*
     * JPG validity check
     *  first 2 bytes matchs SOI(start of image) marker => JPG image
     */
    if(*src==0xff && *(src+1)==0xd8){
        p = src + 2; //2 byte
    }else{
        errret;
    }


    /* search SOFn segments */
    for(;;){
        while(*p!=0xff){
            p++; if(p>=pmax) errret;
        }

        //stop searching because SOS(start of scan),EOI(end of image)
        // segments are found
        if((*(p+1)==0xda || *(p+1)==0xd9) && flagsize==1){
            free(src);
            return size;
        }

        //printf("ff %02x\n", *(p+1));

        // check SOFn segments
        if(flagsize==0){
            int i;
            static unsigned char sof[] = { 0xc0, 0xc1, 0xc2, 0xc3, 0xc5, 0xc6, 0xc7, 0xc9, 0xca, 0xcb, 0xcd, 0xce, 0xcf, };
            for(i=0; i<sizeof(sof)/sizeof(sof[0]); i++){
                if(*(p+1) == sof[i]){
                    /*
                     * just find SOFn segment
                     * get pixel size of the image
                     *
                     * SOFmarker segmentlength bit/sample height width ..
                     * ff cN     NN NN         NN         NN NN  NN NN ..
                     */
                    size.height = *(p+5)<<8 | *(p+6);
                    size.width = *(p+7)<<8 | *(p+8);
                    //printf("width=%ld height=%ld\n", size.width, size.height);
                    flagsize = 1;
                    break;
                }
            }//for
        }//if

        // not SOFn segment
        {
            unsigned short next = (*(p+2))<<8 | *(p+3);
            p += 2;
            p += next; //marker(2byte) + segment length
            if(p>=pmax) errret;
        }
    }

    return size;
}
void moveFile(char *fname, char *dir){
    char new_fname[MAXLINECHAR];

    //make directory if not exist
    mkdir(dir, 0777);
    //move
    strncpy(new_fname, dir, MAXLINECHAR);
    strncat(new_fname, "/", MAXLINECHAR);
    strncat(new_fname, fname, MAXLINECHAR);
    new_fname[MAXLINECHAR-1] = '\0';
    rename(fname, new_fname);
}
//end of file
