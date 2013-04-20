#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "notefunc.h"

char* notenames = "C\0 C#\0D\0 D#\0E\0 F\0 F#\0G\0 G#\0A\0 A#\0B\0 ";
char* notelist = "CCDDEFFGGAAB";
char returnval[10];

double eof_note_to_freq(char* note) {
    char notename;
    char octavestr[10];
    int notenum = 0;
    int octavenum;
    int issharp;

    fprintf(stderr,"%s\n",note);
    notename = note[0];
    if(strchr(note,'#') == NULL) {
        strcpy(octavestr,note+1);
        octavenum = atoi(octavestr);
        issharp = 0;
    }
    else {
        fprintf(stderr,"%s\n",note);
        fprintf(stderr,"%s\n",note+2);
        strcpy(octavestr,note+2);
        octavenum = atoi(octavestr);
        issharp = 1;
    }
    fprintf(stderr,"%c,%d,%d",notename,octavenum,issharp);

    int i;
    for(i=0;i < strlen(notelist);i++) {
        if(notelist[i] == notename)
            break;
    }
    notenum = i + issharp;

    return BASE_FREQ * pow(2,octavenum+(notenum/12.0));
}

char* eof_freq_to_note(double freq) {
    double midval = fabs(log(freq/BASE_FREQ)/log(2.0));
    int octavenum = floor(midval);
    int notenum = floor(12.0*(midval - octavenum));
    
    fprintf(stderr,"%f(%f):%d,%d",freq,midval,octavenum,notenum);
    sprintf(returnval, "%s%d", notenames+notenum*3, octavenum);
    return returnval;
}
