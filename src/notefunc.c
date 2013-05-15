#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "notefunc.h"

char* notenames = "C\0 C#\0D\0 D#\0E\0 F\0 F#\0G\0 G#\0A\0 A#\0B\0 ";
char* notelist = "CxDxEFxGxAxB";
char returnval[10];

double notefunc_note_to_freq(char* note) {
	char notename;
	char octavestr[10];
	int notenum = 0;
	int octavenum;
	char accidental = 0;

	notename = note[0];
	if(strlen(note) == 2) 
	{
		strcpy(octavestr,note+1);
		accidental = 0;
	} 
	else if(strlen(note) == 3) 
	{
		strcpy(octavestr,note+2);
		accidental = note[1];
	} 
	else 
	{ //Couldn't parse, just return A4
		return 440;
	}

	octavenum = atoi(octavestr);			//Parse the octave
	if(notename > 90) { notename -= 32; }	//Upper-case the note

	//fprintf(stderr,"%c,%d,%c\n",notename,octavenum,accidental);

	int numnotes = strlen(notelist);
	for(notenum=0;notenum < numnotes;notenum++) 
	{ //Find the base note number
		if(notelist[notenum] == notename)
			break;
	}
	if(notenum == numnotes) 
	{ 
		return 440; 
	}	//Again, not a note we know!

	//Adjust for accidentals
	int adj = 0;
	if(accidental) 
	{
		if(accidental == '#') 
		{
			adj = 1;
		} 
		else if(accidental == 'b') 
		{
			adj = -1;
		}
		notenum = (notenum + adj) % numnotes;
	}

	//Round to two decimal places to avoid floating point weirdness
	return round(BASE_FREQ * pow(2,octavenum+(notenum/12.0)) *100)/100;
}

char* notefunc_freq_to_note(double freq) 
{
	double midval = round(fabs(log(freq/BASE_FREQ)/log(2.0)) *100)/100;
	int octavenum = floor(midval);
	int notenum = round(12.0*(midval - octavenum));
	
	//fprintf(stderr,"%f(%f):%d,%d\n",freq,midval,octavenum,notenum);
	sprintf(returnval, "%s%d", notenames+notenum*3, octavenum);
	return returnval;
}
