#include <allegro.h>
#include <assert.h>
#include "event.h"
#include "foflc/Lyric_storage.h"
#include "ini_import.h"
#include "main.h"
#include "midi_data_import.h"
#include "midi_import.h"
#include "menu/beat.h"
#include "menu/note.h"
#include "song.h"	//Include before beat.h for EOF_SONG struct definition
#include "utility.h"
#include "beat.h"

#ifdef USEMEMWATCH
#include "memwatch.h"
#endif

typedef struct
{
	unsigned long pos;
	unsigned char type;
	unsigned char channel;
	int d1, d2;
	unsigned long track;	//The track number this event is from
	char text[EOF_MAX_MIDI_TEXT_SIZE+1];
	char *dp;	//Stores pointer to data (ie. Sysex messages)
} EOF_IMPORT_MIDI_EVENT;

typedef struct
{
	EOF_IMPORT_MIDI_EVENT * event[EOF_IMPORT_MAX_EVENTS];
	unsigned long events;
	int type;
	int game;	//Is set to 0 to indicate a Frets on Fire, Rock Band or Guitar Hero style MIDI is being imported, set to 1 to indicate a Power Gig MIDI is being imported
	unsigned char diff;	//Some tracks (such as the pro keys and Power Gig tracks) have all of their contents applicable to a single difficulty level
	unsigned long tracknum;
} EOF_IMPORT_MIDI_EVENT_LIST;

static MIDI * eof_work_midi = NULL;
static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_events[EOF_MAX_IMPORT_MIDI_TRACKS];
static EOF_MIDI_TS_LIST * eof_import_ts_changes[EOF_MAX_IMPORT_MIDI_TRACKS];
static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_bpm_events;
static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_ks_events;
static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_text_events;
int eof_import_bpm_count = 0;

static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_create_events_list(void)
{
	EOF_IMPORT_MIDI_EVENT_LIST * lp;
	eof_log("eof_import_create_events_list() entered", 1);

	lp = malloc(sizeof(EOF_IMPORT_MIDI_EVENT_LIST));
	if(!lp)
	{
		return NULL;
	}
	lp->events = 0;
	lp->type = -1;
	return lp;
}

static void eof_import_destroy_events_list(EOF_IMPORT_MIDI_EVENT_LIST * lp)
{
	int i;

	eof_log("eof_import_destroy_events_list() entered", 1);

	if(lp)
	{
		for(i = 0; i < lp->events; i++)
		{
			free(lp->event[i]);
		}
		free(lp);
	}
}

/* parse_var_len:
 *  The MIDI file format is a strange thing. Time offsets are only 32 bits,
 *  yet they are compressed in a weird variable length format. This routine
 *  reads a variable length integer from a MIDI data stream. It returns the
 *  number read, and alters the data pointer according to the number of
 *  bytes it used.
 */
unsigned long eof_parse_var_len(unsigned char * data, unsigned long pos, unsigned long * bytes_used)
{	//bytes_used is set to the number of bytes long the variable length value is, the value itself is returned
//	eof_log("eof_parse_var_len() entered");

	int cpos = pos;
	unsigned long val = *(&data[cpos]) & 0x7F;

	if(!data || !bytes_used)
	{
		return 0;
	}

	while(data[cpos] & 0x80)
	{
		cpos++;
		(*bytes_used)++;
		val <<= 7;
		val += (data[cpos] & 0x7F);
	}

	(*bytes_used)++;
	return val;
}

static int eof_import_distance(int pos1, int pos2)
{
//	eof_log("eof_import_distance() entered");

	int distance;

	if(pos1 > pos2)
	{
		distance = pos1 - pos2;
	}
	else
	{
		distance = pos2 - pos1;
	}
	return distance;
}

static long eof_import_closest_beat(EOF_SONG * sp, unsigned long pos)
{
//	eof_log("eof_import_closest_beat() entered");

	unsigned long i;
	long bb = -1, ab = -1;	//If this function is changed to return unsigned long, then these can be changed to unsigned long as well
	char check1 = 0, check2 = 0;

	if(!sp)
	{
		return -1;
	}
	for(i = 0; i < sp->beats; i++)
	{
		if(sp->beat[i]->pos <= pos)
		{
			bb = i;
			check1 = 1;
		}
	}
	for(i = sp->beats; i > 0; i--)
	{
		if(sp->beat[i-1]->pos >= pos)
		{
			ab = i-1;
			check2 = 1;
		}
	}
	if(check1 && check2)
	{
		if(eof_import_distance(sp->beat[bb]->pos, pos) < eof_import_distance(sp->beat[ab]->pos, pos))
		{
			return bb;
		}
		else
		{
			return ab;
		}
	}
	return -1;
}

static void eof_midi_import_add_event(EOF_IMPORT_MIDI_EVENT_LIST * events, unsigned long pos, unsigned char event, unsigned long d1, unsigned long d2, unsigned long track)
{
//	eof_log("eof_midi_import_add_event() entered");

	if(events && (events->events < EOF_IMPORT_MAX_EVENTS))
	{
		events->event[events->events] = malloc(sizeof(EOF_IMPORT_MIDI_EVENT));
		if(events->event[events->events])
		{
			events->event[events->events]->pos = pos;
			events->event[events->events]->type = event & 0xF0;		//The event type
			events->event[events->events]->channel = event & 0xF;	//The channel number
			events->event[events->events]->d1 = d1;
			events->event[events->events]->d2 = d2;
			events->event[events->events]->track = track;	//Store the event's track number
			events->event[events->events]->dp = NULL;

			events->events++;
		}
	}
	else
	{
//		allegro_message("too many events!");
	}
}

static void eof_midi_import_add_text_event(EOF_IMPORT_MIDI_EVENT_LIST * events, unsigned long pos, unsigned char event, char * text, unsigned long size, unsigned long track)
{
//	eof_log("eof_midi_import_add_text_event() entered");

	if(events && text && (events->events < EOF_IMPORT_MAX_EVENTS))
	{
		if(size > EOF_MAX_MIDI_TEXT_SIZE)	//Prevent a buffer overflow by truncating the string if necessary
			size = EOF_MAX_MIDI_TEXT_SIZE;

		events->event[events->events] = malloc(sizeof(EOF_IMPORT_MIDI_EVENT));
		if(events->event[events->events])
		{
			events->event[events->events]->pos = pos;
			events->event[events->events]->type = event;			//The meta event type
			memcpy(events->event[events->events]->text, text, (size_t)size);
			events->event[events->events]->text[size] = '\0';
			events->event[events->events]->track = track;	//Store the event's track number
			events->event[events->events]->dp = NULL;

			events->events++;
		}
	}
	else
	{
//		allegro_message("too many events!");
	}
}

static void eof_midi_import_add_sysex_event(EOF_IMPORT_MIDI_EVENT_LIST * events, unsigned long pos, char * data, unsigned long size, unsigned long track)
{
//	eof_log("eof_midi_import_add_sysex_event() entered");
	char *datacopy = NULL;

	if(events && (events->events < EOF_IMPORT_MAX_EVENTS) && data && (size > 0))
	{
		events->event[events->events] = malloc(sizeof(EOF_IMPORT_MIDI_EVENT));
		if(events->event[events->events])
		{
			datacopy = malloc((size_t)size);
			if(datacopy)
			{
				memcpy(datacopy, data, (size_t)size);			//Copy the input data into the new buffer
				events->event[events->events]->pos = pos;
				events->event[events->events]->type = 0xF0;	//The Sysex event type
				events->event[events->events]->d1 = size;	//Store the size of the Sysex message
				events->event[events->events]->dp = datacopy;	//Store the newly buffered data
				events->event[events->events]->track = track;	//Store the event's track number

				events->events++;
			}
		}
	}
	else
	{
//		allegro_message("too many events!");
	}
}

double eof_ConvertToRealTime(unsigned long absolutedelta,struct Tempo_change *anchorlist,EOF_MIDI_TS_LIST *tslist,unsigned long timedivision,unsigned long offset)
{
//	eof_log("eof_ConvertToRealTime() entered");

	struct Tempo_change *temp=anchorlist;	//Point to first link in list
	double time=0.0;
	unsigned long reldelta=0;
	double tstime=0.0;			//Stores the realtime position of the closest TS change before the specified realtime
	unsigned long tsdelta=0;	//Stores the delta time position of the closest TS change before the specified realtime
	unsigned long ctr=0;

//Find the last time signature change at or before the target delta time
	if((tslist != NULL) && (tslist->changes > 0))
	{	//If there's at least one TS change
		for(ctr=0;ctr < tslist->changes;ctr++)
		{
			if(absolutedelta >= tslist->change[ctr]->pos)
			{	//If the TS change is at or before the target delta time
				tstime = tslist->change[ctr]->realtime;		//Store the realtime position
				tsdelta = tslist->change[ctr]->pos;			//Store the delta time position
			}
		}
	}

//Find the last tempo change before the target delta time
	while((temp->next != NULL) && (absolutedelta >= (temp->next)->delta))	//For each tempo change
	{	//If the tempo change is at or before the target delta time
		temp=temp->next;	//Advance to that time stamp
	}

//Find the latest tempo or TS change that occurs before the target delta position and use that event's timing for the conversion
	if(tsdelta > temp->delta)
	{	//If the TS change is closer to the target realtime, find the delta time relative from this event
		reldelta=absolutedelta - tsdelta;	//Find the relative delta time from this TS change
		time=tstime;						//Store the absolute realtime for the TS change
	}
	else
	{	//Find the delta time relative from the closest tempo change
		reldelta=absolutedelta - temp->delta;	//Find the relative delta time from this tempo change
		time=temp->realtime;					//Store the absolute realtime for the tempo change
	}

//reldelta is the amount of deltas we need to find a relative time for, and add to the absolute real time of the nearest preceding tempo/TS change
//At this point, we have reached the tempo change that absolutedelta resides within, find the realtime
	time+=(double)reldelta / (double)timedivision * ((double)60000.0 / (temp->BPM));

	return time+offset;
}

inline unsigned long eof_ConvertToRealTimeInt(unsigned long absolutedelta,struct Tempo_change *anchorlist,EOF_MIDI_TS_LIST *tslist,unsigned long timedivision,unsigned long offset)
{
	return eof_ConvertToRealTime(absolutedelta,anchorlist,tslist,timedivision,offset) + 0.5;
}

//#define EOF_DEBUG_MIDI_IMPORT

EOF_SONG * eof_import_midi(const char * fn)
{
	EOF_SONG * sp = NULL;
	int pticker = 0;
	int ptotal_events = 0;
	int percent;
	unsigned long i, j;
	long k;			//k is being used with note_count[] with signed logic
	int rbg = 0;	//Is set once the guitar track is parsed?
	int tracks = 0;
	int track[EOF_MAX_IMPORT_MIDI_TRACKS] = {0};
	int track_pos;
	unsigned long delta;
	unsigned long absolute_pos;
	unsigned long last_delta_time=0;	//Will store the absolute delta time of the last parsed MIDI event
	unsigned long bytes_used;
	unsigned char current_event;
	unsigned char current_event_hi = 0;
	unsigned char last_event = 0;
	unsigned char current_meta_event;
	unsigned long d1=0, d2=0, d3, d4;
	char text[EOF_MAX_MIDI_TEXT_SIZE+1];
	char nfn[1024] = {0};
	char backup_filename[1024] = {0};
	char ttit[256] = {0};
	EOF_PHRASE_SECTION *phraseptr = NULL, *phraseptr2 = NULL;
	unsigned long bitmask;
	char chord0name[100] = "", chord1name[100] = "", chord2name[100] = "", chord3name[100] = "", *chordname = NULL;	//Used for chord name import
	char debugstring[100] = {0};
	char is_event_track;	//This will be set to nonzero if the current track's name is EVENTS (for sorting text events into global event list instead of track specific list)
	unsigned long beat_track = 0;	//Will identify the "BEAT" track if it is found during import
	struct Tempo_change *anchorlist=NULL;	//Anchor linked list
	unsigned long deltapos = 0;		//Stores the ongoing delta time
	double deltafpos = 0.0;			//Stores the ongoing delta time (with double floating precision)
	double realtimepos = 0.0;		//Stores the ongoing real time (start at 0s, displace by the MIDI delay where appropriate)
	unsigned lastnum=4,lastden=4;	//Stores the last applied time signature details (default is 4/4)
	unsigned curnum=4,curden=4;		//Stores the current time signature details (default is 4/4)
	unsigned long lastppqn=0;		//Stores the last applied tempo information
	unsigned long curppqn=500000;	//Stores the current tempo in PPQN (default is 120BPM)
	unsigned long ctr,ctr2,nextanchor;
	char midbeatchange, midbeatchangefound = 0;
	double beatlength, beatreallength;
	double BPM=120.0;	//Assume a default tempo of 120BPM and TS of 4/4 at 0 deltas
	unsigned long event_realtime;		//Store the delta time converted to realtime to avoid having to convert multiple times per note
	long beat;
	int picked_track;
	char used_track[EOF_MAX_IMPORT_MIDI_TRACKS] = {0};	//for Rock Band songs, we need to ignore certain tracks
	unsigned char lane = 0;				//Used to track the lane referenced by a MIDI on/off note
	char diff = -1;						//Used to track the difficulty referenced by a MIDI on/off note (-1 means difficulty undetermined)
	unsigned char lane_chart[EOF_MAX_FRETS] = {1, 2, 4, 8, 16, 32};	//Maps each lane to a note bitmask value
	long note_count[EOF_MAX_IMPORT_MIDI_TRACKS] = {0};
	long first_note;
	unsigned long hopopos[4];			//Used for forced HOPO On/Off parsing
	char hopotype[4];					//Used for forced HOPO On/Off parsing
	int hopodiff;						//Used for forced HOPO On/Off parsing
	unsigned long strumpos[4];			//Used for pro guitar strum direction parsing
	char strumtype[4];					//Used for pro guitar strum direction parsing
	int strumdiff;						//Used for pro guitar strum direction parsing
	unsigned long arpegpos[4];			//Used for pro guitar arpeggio parsing
	int arpegdiff;						//Used for pro guitar arpeggio parsing
	char prodrums = 0;					//Tracks whether the drum track being written includes Pro drum notation
	unsigned long tracknum;				//Used to de-obfuscate the legacy track number
	int phrasediff;						//Used for parsing Sysex phrase markers
	int slidediff;						//Used for parsing slide markers
	unsigned char slidevelocity[4];		//Used for parsing slide markers
	unsigned long openstrumpos[4] = {0}, slideuppos[4] = {0}, slidedownpos[4] = {0}, openhihatpos[4] = {0}, pedalhihatpos[4] = {0}, rimshotpos[4] = {0}, sliderpos[4] = {0}, palmmutepos[4] = {0}, vibratopos[4] = {0};	//Used for parsing Sysex phrase markers
	int lc;
	long b = -1;
	unsigned long tp;

	eof_log("eof_import_midi() entered", 1);

	/* load MIDI */
	if(!fn)
	{
		return 0;
	}
	eof_work_midi = load_midi(fn);
	if(!eof_work_midi)
	{
		return 0;
	}
//	eof_log_level &= ~2;	//Disable verbose logging

	/* backup "notes.mid" if it exists in the folder with the imported MIDI
	   as it will be overwritten upon save */
	(void) replace_filename(eof_temp_filename, fn, "notes.mid", 1024);
	if(exists(eof_temp_filename))
	{
		/* do not overwrite an existing backup, this prevents the original backed up MIDI from
		   being overwritten if the user imports the MIDI again */
		(void) replace_filename(backup_filename, fn, "notes.mid.backup", 1024);
		if(!exists(backup_filename))
		{
			(void) eof_copy_file(eof_temp_filename, backup_filename);
		}
	}

	/* backup "song.ini" if it exists in the folder with the imported MIDI
	   as it will be overwritten upon save */
	(void) replace_filename(eof_temp_filename, fn, "song.ini", 1024);
	if(exists(eof_temp_filename))
	{
		/* do not overwrite an existing backup, this prevents the original backed up song.ini from
		   being overwritten if the user imports the MIDI again */
		(void) replace_filename(backup_filename, fn, "song.ini.backup", 1024);
		if(!exists(backup_filename))
		{
			(void) eof_copy_file(eof_temp_filename, backup_filename);
		}
	}

	sp = eof_create_song_populated();
	if(!sp)
	{
		destroy_midi(eof_work_midi);
		return NULL;
	}

	/* read INI file */
	(void) replace_filename(backup_filename, fn, "song.ini", 1024);
	(void) eof_import_ini(sp, backup_filename, 0);


	/* parse MIDI data */
	for(i = 0; i < EOF_MAX_IMPORT_MIDI_TRACKS; i++)
	{	//Note each of the first EOF_MAX_IMPORT_MIDI_TRACKS number of tracks that have MIDI events
		if(eof_work_midi->track[i].data)
		{
			track[tracks] = i;
			tracks++;
		}
	}

	/* first pass, build events list for each track */

//#ifdef EOF_DEBUG_MIDI_IMPORT
	eof_log("\tFirst pass, building events lists", 1);
//#endif

	eof_import_bpm_events = eof_import_create_events_list();
	if(!eof_import_bpm_events)
	{
		destroy_midi(eof_work_midi);
		eof_destroy_song(sp);
		return NULL;
	}
	eof_import_text_events = eof_import_create_events_list();
	if(!eof_import_text_events)
	{
		eof_import_destroy_events_list(eof_import_bpm_events);
		destroy_midi(eof_work_midi);
		eof_destroy_song(sp);
		return NULL;
	}
	eof_import_ks_events = eof_import_create_events_list();
	if(!eof_import_text_events)
	{
		eof_import_destroy_events_list(eof_import_bpm_events);
		eof_import_destroy_events_list(eof_import_text_events);
		destroy_midi(eof_work_midi);
		eof_destroy_song(sp);
		return NULL;
	}
	for(i = 0; i < tracks; i++)
	{	//For each imported track
//#ifdef EOF_DEBUG_MIDI_IMPORT
		(void) snprintf(debugstring, sizeof(debugstring) - 1, "\t\tParsing track #%lu of %d",i,tracks);
		eof_log(debugstring, 1);
//#endif
		last_event = 0;	//Running status resets at beginning of each track
		is_event_track = 0;	//This remains zero unless a track name event naming the track as "EVENTS" is encountered
		current_event = current_event_hi = 0;
		eof_import_events[i] = eof_import_create_events_list();
		eof_import_ts_changes[i] = eof_create_ts_list();
		if(!eof_import_events[i] || !eof_import_ts_changes[i])
		{
			eof_import_destroy_events_list(eof_import_bpm_events);
			eof_import_destroy_events_list(eof_import_text_events);
			eof_import_destroy_events_list(eof_import_ks_events);
			destroy_midi(eof_work_midi);
			eof_destroy_song(sp);
			return NULL;
		}
		track_pos = 0;
		absolute_pos = 0;
		while(track_pos < eof_work_midi->track[track[i]].len)
		{	//While the byte index of this MIDI track hasn't reached the end of the track data
			/* read delta */
#ifdef EOF_DEBUG_MIDI_IMPORT
			(void) snprintf(debugstring, sizeof(debugstring) - 1, "\t\t\tParsing byte #%d of %d",track_pos,eof_work_midi->track[track[i]].len);
			eof_log(debugstring, 1);
#endif

			bytes_used = 0;
			delta = eof_parse_var_len(eof_work_midi->track[track[i]].data, track_pos, &bytes_used);
			absolute_pos += delta;
			if(absolute_pos > last_delta_time)
				last_delta_time = absolute_pos;	//Remember the delta position of the latest event in the MIDI (among all tracks)
			track_pos += bytes_used;

			/* read event type */
			if((current_event_hi >= 0x80) && (current_event_hi < 0xF0))
			{	//If the last loop iteration's event was normal
				last_event = current_event;	//Store it (including the original channel number)
			}
			assert(track_pos < eof_work_midi->track[track[i]].len);
			current_event = eof_work_midi->track[track[i]].data[track_pos];

			if((current_event & 0xF0) < 0x80)	//If this event is a running status event
			{
				current_event = last_event;	//Recall the previous normal event
			}
			else
			{
				track_pos++;	//Increment buffer pointer past the status byte
			}
			current_event_hi = current_event & 0xF0;

			ptotal_events++;	//Any event that is parsed should increment this counter
			if(current_event_hi < 0xF0)
			{	//If it's not a Meta event, assume that two parameters are to be read for the event (this will be undone for Program Change and Channel Aftertouch, which don't have a second parameter)
				d1 = eof_work_midi->track[track[i]].data[track_pos++];
				d2 = eof_work_midi->track[track[i]].data[track_pos++];
			}
			switch(current_event_hi)
			{
				/* note off */
				case 0x80:
				{
					eof_midi_import_add_event(eof_import_events[i], absolute_pos, current_event, d1, d2, i);
					break;
				}

				/* note on */
				case 0x90:
				{
					if(d2 == 0)
					{	//Any Note On event with a velocity of 0 is to be treated as a Note Off event
						eof_midi_import_add_event(eof_import_events[i], absolute_pos, 0x80 + (current_event & 0xF), d1, d2, i);	//Retain the original channel number for this event
					}
					else
					{
						eof_midi_import_add_event(eof_import_events[i], absolute_pos, current_event, d1, d2, i);
					}
					break;
				}

				/* key aftertouch */
				case 0xA0:
				{
					break;
				}

				/* controller change */
				case 0xB0:
				{
					break;
				}

				/* program change */
				case 0xC0:
				{
					track_pos--;	//Rewind one byte, there is no second parameter
					break;
				}

				/* channel aftertouch */
				case 0xD0:
				{
					track_pos--;	//Rewind one byte, there is no second parameter
					break;
				}

				/* pitch wheel change */
				case 0xE0:
				{
					break;
				}

				/* meta event */
				case 0xF0:
				{
					if((current_event == 0xF0) || (current_event == 0xF7))
					{	//If it's a Sysex event
						bytes_used = 0;
						d3 = eof_parse_var_len(eof_work_midi->track[track[i]].data, track_pos, &bytes_used);
						track_pos += bytes_used;
						eof_midi_import_add_sysex_event(eof_import_events[i], absolute_pos, (char *)&eof_work_midi->track[track[i]].data[track_pos], d3, i);	//Store a copy of the Sysex data
						track_pos += d3;
					}
					else if(current_event == 0xFF)
					{
						current_meta_event = eof_work_midi->track[track[i]].data[track_pos];
						track_pos++;
						if(current_meta_event != 0x51)
						{
						}
						switch(current_meta_event)
						{

							/* set sequence number */
							case 0x00:
							{
								track_pos += 3;
								break;
							}

							/* text event */
							case 0x01:
							{
								for(j = 0; j < eof_work_midi->track[track[i]].data[track_pos]; j++)
								{
									if(j < EOF_MAX_MIDI_TEXT_SIZE)
									{	//If this wouldn't overflow the buffer
										text[j] = eof_work_midi->track[track[i]].data[track_pos + 1 + j];
									}
									else
									{
										break;
									}
								}
								if(j >= EOF_MAX_MIDI_TEXT_SIZE)
								{	//If the string needs to be truncated
									text[EOF_MAX_MIDI_TEXT_SIZE] = '\0';
								}
								else
								{
									text[j] = '\0';	//Terminate the string normally
								}
								if(is_event_track)
								{	//If the EVENTS track is being parsed, add it to the regular text event list
									eof_midi_import_add_text_event(eof_import_text_events, absolute_pos, 0x01, text, eof_work_midi->track[track[i]].data[track_pos], i);
								}
								else
								{	//Otherwise add it to the track specific events list
									eof_midi_import_add_text_event(eof_import_events[i], absolute_pos, 0x01, text, eof_work_midi->track[track[i]].data[track_pos], i);
								}
								track_pos += eof_work_midi->track[track[i]].data[track_pos] + 1;
								break;
							}

							/* copyright */
							case 0x02:
							{
								track_pos += eof_work_midi->track[track[i]].data[track_pos] + 1;
								break;
							}

							/* track name */
							case 0x03:
							{
								d3 = eof_work_midi->track[track[i]].data[track_pos];
								for(j = 0; j < d3; j++)
								{
									track_pos++;
									if(j < EOF_MAX_MIDI_TEXT_SIZE)			//If this wouldn't overflow the buffer
										text[j] = eof_work_midi->track[track[i]].data[track_pos];
								}
								if(j <= EOF_MAX_MIDI_TEXT_SIZE)				//If the string fit in the buffer
									text[j] = '\0';							//Terminate the string normally
								else
									text[EOF_MAX_MIDI_TEXT_SIZE] = '\0';	//Otherwise truncate it
								track_pos++;

								if(!ustricmp(text, "PART DRUM"))
								{	//If this MIDI track is using the incorrect name of "PART DRUM"
									(void) ustrcpy(text, "PART DRUMS");	//Correct the name
								}

								if(!ustricmp(text, "BEAT"))
								{	//If this is the BEAT track
									beat_track = track[i];	//Note the track number
								}

								/* detect what kind of track this is */
								eof_import_events[i]->type = 0;
								for(j = 1; j < EOF_TRACKS_MAX; j++)
								{	//Compare the track name against the tracks in eof_midi_tracks[]
									if(!ustricmp(text, eof_midi_tracks[j].name))
									{	//If this track name matches an expected name
										eof_import_events[i]->type = eof_midi_tracks[j].track_type;
										eof_import_events[i]->game = 0;	//Note that this is a FoF/RB/GH style MIDI
										eof_import_events[i]->diff = eof_midi_tracks[j].difficulty;
										eof_import_events[i]->tracknum = j;
										if(eof_midi_tracks[j].track_type == EOF_TRACK_GUITAR)
										{	//If this is the guitar track
											rbg = 1;	//Note that the track has been found
										}
									}
								}
								if(eof_import_events[i]->type == 0)
								{	//If the track name didn't match any of the standard Rock Band track names
									for(j = 1; j < EOF_POWER_GIG_TRACKS_MAX; j++)
									{	//Compare the track name against the tracks in eof_power_gig_tracks[]
										if(!ustricmp(text, eof_power_gig_tracks[j].name))
										{	//If this track name matches an expected name
											if(eof_midi_tracks[j].track_format == 0)
											{	//If this is a track that is to be skipped
												eof_import_events[i]->type = -1;	//Flag this as being a track that gets skipped
											}
											else
											{
												eof_import_events[i]->type = eof_power_gig_tracks[j].track_type;
												eof_import_events[i]->game = 1;	//Note that this is a Power Gig style MIDI
												eof_import_events[i]->diff = eof_power_gig_tracks[j].difficulty;
												eof_import_events[i]->tracknum = j;
												if(eof_midi_tracks[j].track_type == EOF_TRACK_GUITAR)
												{	//If this is the guitar track
													rbg = 1;	//Note that the track has been found
												}
											}
										}
									}
									if(eof_import_events[i]->type == 0)
									{	//If the track name didn't match any of the standard Power Gig track names either
										if(ustrstr(text,"PART") || (ustrstr(text,"HARM") == text))
										{	//If this is a track that contains the word "PART" in the name (or begins with "HARM")
											eof_clear_input();
											key[KEY_Y] = 0;
											key[KEY_N] = 0;
											if(alert("Unsupported track:", text, "Import raw data?", "&Yes", "&No", 'y', 'n') == 1)
											{	//If the user opts to import the raw track data
												eof_MIDI_add_track(sp, eof_get_raw_MIDI_data(eof_work_midi, track[i], 0));	//Add this to the linked list of raw MIDI track data
											}
											eof_import_events[i]->type = -1;	//Flag this as being a track that gets skipped
										}
										else if(!ustricmp(text, "EVENTS"))
										{	//This track is the EVENTS track
											is_event_track = 1;	//Store any encountered text events into the global events array
										}
									}
								}//If the track name didn't match any of the standard Rock Band track names
								break;
							}

							/* instrument name */
							case 0x04:
							{
								track_pos += eof_work_midi->track[track[i]].data[track_pos] + 1;
								break;
							}

							/* lyric */
							case 0x05:
							{
								for(j = 0; j < eof_work_midi->track[track[i]].data[track_pos]; j++)
								{
									if(j < EOF_MAX_MIDI_TEXT_SIZE)			//If this wouldn't overflow the buffer
										text[j] = eof_work_midi->track[track[i]].data[track_pos + 1 + j];
									else
										break;
								}
								if(j >= EOF_MAX_MIDI_TEXT_SIZE)	//If the string needs to be truncated
									text[EOF_MAX_MIDI_TEXT_SIZE] = '\0';
								eof_midi_import_add_text_event(eof_import_events[i], absolute_pos, 0x05, text, eof_work_midi->track[track[i]].data[track_pos], i);
								track_pos += eof_work_midi->track[track[i]].data[track_pos] + 1;
								break;
							}

							/* marker */
							case 0x06:
							{
								track_pos += eof_work_midi->track[track[i]].data[track_pos] + 1;
								break;
							}

							/* cue point */
							case 0x07:
							{
								track_pos++;
								track_pos += eof_work_midi->track[track[i]].data[track_pos] + 1;
								break;
							}

							/* MIDI channel prefix */
							case 0x20:
							{
								track_pos += 2;
								break;
							}

							/* end of track */
							case 0x2F:
							{
								track_pos += 1;
								break;
							}

							/* set tempo */
							case 0x51:
							{
								track_pos++;
								d1 = (eof_work_midi->track[track[i]].data[track_pos++]);	//MPQN byte 1
								d2 = (eof_work_midi->track[track[i]].data[track_pos++]);	//MPQN byte 2
								d3 = (eof_work_midi->track[track[i]].data[track_pos++]);	//MPQN byte 3
								d4 = (d1 << 16) | (d2 << 8) | (d3);

								if((eof_import_bpm_events->events == 0) && (absolute_pos > sp->tags->ogg[0].midi_offset))
								{	//If the first explicit Set Tempo event is not at the beginning of the track
									eof_midi_import_add_event(eof_import_bpm_events, 0, 0x51, 500000, 0, i);	//Insert the default tempo of 120BPM at the beginning of the tempo list
								}
								eof_midi_import_add_event(eof_import_bpm_events, absolute_pos, 0x51, d4, 0, i);
								break;
							}

							/* time signature */
							case 0x58:
							{
								unsigned long ctr,realden;

								track_pos++;
								d1 = (eof_work_midi->track[track[i]].data[track_pos++]);	//Numerator
								d2 = (eof_work_midi->track[track[i]].data[track_pos++]);	//Denominator
								d3 = (eof_work_midi->track[track[i]].data[track_pos++]);	//Metronome
								d4 = (eof_work_midi->track[track[i]].data[track_pos++]);	//32nds

								for(ctr=0,realden=1;ctr<d2;ctr++)
								{	//Find 2^(d2)
									realden = realden << 1;
								}

								eof_midi_add_ts_deltas(eof_import_ts_changes[i],absolute_pos,d1,realden,i);
								break;
							}

							/* key signature */
							case 0x59:
							{
								char key;
								track_pos++;
								key = (eof_work_midi->track[track[i]].data[track_pos++]);	//Key
								d2 = (eof_work_midi->track[track[i]].data[track_pos++]);	//Scale
								eof_midi_import_add_event(eof_import_ks_events, absolute_pos, 0x59, key, d2, i);
								break;
							}

							/* sequencer info */
							case 0x7F:
							{
								track_pos += eof_work_midi->track[track[i]].data[track_pos] + 1;
								break;
							}

							default:
							{
								ptotal_events--;	//This was not a valid MIDI Meta event, undo the event counter increment
								track_pos++;
								break;
							}
						}
					}
					break;
				}

				default:
				{
					ptotal_events--;	//This was not a valid MIDI event, undo the event counter increment
					track_pos--;		//Rewind one of the two bytes that were seeked
					break;
				}
			}//switch(current_event_hi)
		}//While the byte index of this MIDI track hasn't reached the end of the track data
	}//For each imported track


	/* second pass, create tempo map */
//#ifdef EOF_DEBUG_MIDI_IMPORT
//char debugtext[400];
//allegro_message("Pass two, adding beats.  last_delta_time = %lu",last_delta_time);
	eof_log("\tPass two, adding beats", 1);
//#endif

	while(deltapos <= last_delta_time)
	{//Add new beats until enough have been added to encompass the last MIDI event

#ifdef EOF_DEBUG_MIDI_IMPORT
snprintf(debugtext, sizeof(debugtext) - 1,"Start delta %lu / %lu: Adding beat",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		if(eof_song_add_beat(sp) == NULL)	//Add a new beat
		{					//Or return failure if that doesn't succeed

allegro_message("Fail point 1");

			eof_import_destroy_events_list(eof_import_bpm_events);
			eof_import_destroy_events_list(eof_import_text_events);
			eof_import_destroy_events_list(eof_import_ks_events);
			destroy_midi(eof_work_midi);
			eof_destroy_tempo_list(anchorlist);
			eof_destroy_song(sp);
			return NULL;
		}

	//Find the relevant tempo and time signature for the beat

#ifdef EOF_DEBUG_MIDI_IMPORT
snprintf(debugtext, sizeof(debugtext) - 1,"Start delta %lu / %lu: Finding tempo and TS",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		for(ctr = 0; ctr < eof_import_bpm_events->events; ctr++)
		{	//For each imported tempo change

assert(eof_import_bpm_events->event[ctr] != NULL);	//Prevent a NULL dereference below

			if(eof_import_bpm_events->event[ctr]->pos <= deltapos)
			{	//If the tempo change is at or before the current delta time
				curppqn = eof_import_bpm_events->event[ctr]->d1;	//Store the PPQN value
			}
		}
		for(ctr = 0; ctr < eof_import_ts_changes[0]->changes; ctr++)
		{	//For each imported TS change

assert(eof_import_ts_changes[0]->change[ctr] != NULL);	//Prevent a NULL dereference below

			if(eof_import_ts_changes[0]->change[ctr]->pos <= deltapos)
			{	//If the TS change is at or before the current delta time
				curnum = eof_import_ts_changes[0]->change[ctr]->num;	//Store the numerator and denominator
				curden = eof_import_ts_changes[0]->change[ctr]->den;
			}
		}

assert(sp->beats < EOF_MAX_BEATS);			//Prevent out of bounds reference below
assert(sp->beat[sp->beats - 1] != NULL);	//Prevent NULL dereference below

	//Store timing information in the beat structure

#ifdef EOF_DEBUG_MIDI_IMPORT
snprintf(debugtext, sizeof(debugtext) - 1,"Start delta %lu / %lu: Storing timing info",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

assert(sp->tags != NULL);	//Prevent a NULL dereference below

		sp->beat[sp->beats - 1]->fpos = realtimepos + sp->tags->ogg[0].midi_offset;
		sp->beat[sp->beats - 1]->pos = sp->beat[sp->beats - 1]->fpos + 0.5;	//Round up to nearest millisecond
		sp->beat[sp->beats - 1]->midi_pos = deltapos;
		sp->beat[sp->beats - 1]->ppqn = curppqn;
		if(eof_use_ts && ((lastnum != curnum) || (lastden != curden) || (sp->beats - 1 == 0)))
		{	//If the user opted to import TS changes, and this time signature is different than the last beat's time signature (or this is the first beat)

assert(sp->beats > 0);	//Prevent eof_apply_ts() below from failing

			(void) eof_apply_ts(curnum,curden,sp->beats - 1,sp,0);	//Set the TS flags for this beat
		}

assert(curppqn != 0);	//Avoid a division by 0 below

	//Update anchor linked list

#ifdef EOF_DEBUG_MIDI_IMPORT
snprintf(debugtext, sizeof(debugtext) - 1,"Start delta %lu / %lu: Updating anchor list",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		if(lastppqn != curppqn)
		{	//If this tempo is different than the last beat's tempo
			anchorlist=eof_add_to_tempo_list(deltapos,realtimepos,60000000.0 / curppqn,anchorlist);

assert(anchorlist != NULL);	//This would mean eof_add_to_tempo_list() failed

			sp->beat[sp->beats - 1]->flags |= EOF_BEAT_FLAG_ANCHOR;
			lastppqn = curppqn;
		}

	//Find the number of deltas to the next tempo or time signature change, in order to handle mid beat changes

#ifdef EOF_DEBUG_MIDI_IMPORT
snprintf(debugtext, sizeof(debugtext) - 1,"Start delta %lu / %lu: Calculate mid best tempo/TS change",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		midbeatchange = 0;
		beatlength = eof_work_midi->divisions;		//Determine the length of one full beat in delta ticks
		nextanchor = deltafpos + beatlength + 0.5;	//By default, the delta position of the next beat will be the standard length of delta ticks
		for(ctr = 0; ctr < eof_import_bpm_events->events; ctr++)
		{	//For each imported tempo change
			if(eof_import_bpm_events->event[ctr]->pos > deltapos)
			{	//If this tempo change is ahead of the current delta position
				if(eof_import_bpm_events->event[ctr]->pos < nextanchor)
				{	//If this tempo change occurs before the next beat marker
					(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\tMid beat tempo change at delta position %lu", eof_import_bpm_events->event[ctr]->pos);
					eof_log(eof_log_string, 1);
					nextanchor = eof_import_bpm_events->event[ctr]->pos;	//Store its delta time
					midbeatchange = 1;
				}
				break;
			}
		}
		for(ctr = 0; ctr < eof_import_ts_changes[0]->changes; ctr++)
		{	//For each imported TS change
			if(eof_import_ts_changes[0]->change[ctr]->pos > deltapos)
			{	//If this TS change is ahead of the current delta position
				if(eof_import_ts_changes[0]->change[ctr]->pos < nextanchor)
				{	//If this TS change occurs before the next beat marker or mid-beat tempo change
					(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\tMid beat TS change at delta position %lu", eof_import_ts_changes[0]->change[ctr]->pos);
					eof_log(eof_log_string, 1);
					nextanchor = eof_import_ts_changes[0]->change[ctr]->pos;	//store its delta time
					midbeatchange = 1;
				}
				break;
			}
		}
		if(midbeatchange)
		{	//If there is a mid-beat tempo/TS change, this beat needs to be anchored and its tempo (and the current tempo) altered
			//Also update beatlength to reflect that less than a full beat's worth of deltas will be used to advance to the next beat marker
			sp->beat[sp->beats - 1]->flags |= EOF_BEAT_FLAG_ANCHOR;
			curppqn = (double)curppqn * (((double)nextanchor - deltafpos) / beatlength) + 0.5;	//Scale the current beat's tempo based on the adjusted delta length (rounded to nearest whole number)
			sp->beat[sp->beats - 1]->ppqn = curppqn;		//Update the beat's (now an anchor) tempo
			beatlength = (double)nextanchor - deltafpos;	//This is the distance between the current beat, and the upcoming mid-beat change
			midbeatchangefound = 1;
		}

	//Update delta and realtime counters (the TS affects a beat's length in deltas, the tempo affects a beat's length in milliseconds)

#ifdef EOF_DEBUG_MIDI_IMPORT
snprintf(debugtext, sizeof(debugtext) - 1,"Start delta %lu / %lu: Updating counters",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		beatreallength = (60000.0 / (60000000.0 / (double)curppqn));		//Determine the length of this beat in milliseconds
		realtimepos += beatreallength;	//Add the realtime length of this beat to the time counter
		deltafpos += beatlength;	//Add the delta length of this beat to the delta counter
		deltapos = deltafpos + 0.5;	//Round up to nearest delta tick
		lastnum = curnum;
		lastden = curden;
	}//Add new beats until enough have been added to encompass the last MIDI event

	/* If a mid beat tempo or TS change was found, offer to store the tempo map and the BEAT track into the project */
	if(midbeatchangefound)
	{
		eof_clear_input();
		key[KEY_Y] = 0;
		key[KEY_N] = 0;
		if(alert("Warning:  There were one or more mid beat tempo/TS changes.", "Store the tempo track into the project?", "(Recommended if creating RB3 upgrades)", "&Yes", "&No", 'y', 'n') == 1)
		{	//If the user opts to import the tempo track
			struct eof_MIDI_data_track *ptr;
			char *tempotrackname = "(TEMPO)";
			ptr = eof_get_raw_MIDI_data(eof_work_midi, 0, 0);	//Parse the tempo track out of the file
			if(!ptr)		//If the import failed
				return 0;	//return error
			if(ptr->trackname)
			{	//If the track had a name
				free(ptr->trackname);	//release it as it will be replaced
			}
			ptr->trackname = malloc(strlen(tempotrackname) + 1);
			if(!ptr->trackname)
			{	//Allocation error
				eof_import_destroy_events_list(eof_import_bpm_events);
				eof_import_destroy_events_list(eof_import_text_events);
				eof_import_destroy_events_list(eof_import_ks_events);
				destroy_midi(eof_work_midi);
				eof_destroy_tempo_list(anchorlist);
				eof_destroy_song(sp);
				eof_MIDI_empty_event_list(ptr->events);
				free(ptr);
				return 0;	//Return error
			}
			strcpy(ptr->trackname, tempotrackname);	//Update the tempo track name
			eof_MIDI_add_track(sp, ptr);	//Add this to the linked list of raw MIDI track data
			if(beat_track)
			{	//If there was a beat track found
				char *beattrackname = "(BEAT)";
				eof_clear_input();
				key[KEY_Y] = 0;
				key[KEY_N] = 0;
				if(alert("Store the BEAT track as well?", "(Recommended)", NULL, "&Yes", "&No", 'y', 'n') == 1)
				{	//If the user opts to store the beat track
					ptr = eof_get_raw_MIDI_data(eof_work_midi, beat_track, 0);
					if(ptr->trackname)
					{	//If the track had a name (it should have)
						free(ptr->trackname);
					}
					ptr->trackname = malloc(strlen(beattrackname) + 1);
					if(!ptr->trackname)
					{	//Allocation error
						eof_import_destroy_events_list(eof_import_bpm_events);
						eof_import_destroy_events_list(eof_import_text_events);
						eof_import_destroy_events_list(eof_import_ks_events);
						destroy_midi(eof_work_midi);
						eof_destroy_tempo_list(anchorlist);
						eof_destroy_song(sp);
						eof_MIDI_empty_event_list(ptr->events);
						free(ptr);
						return 0;	//Return error
					}
					strcpy(ptr->trackname, beattrackname);	//Update the beat track name
					eof_MIDI_add_track(sp, ptr);	//Add this to the linked list of raw MIDI track data
				}
			}
			eof_clear_input();
			key[KEY_Y] = 0;
			key[KEY_N] = 0;
			if(alert("Lock the tempo map to preserve sync?", "(Recommended)", NULL, "&Yes", "&No", 'y', 'n') == 1)
			{	//If the user opts to lock the tempo map
				sp->tags->tempo_map_locked |= 1;
			}
		}
	}

	eof_log("\tPass two, configuring beat timings", 1);
	realtimepos=0.0;
	deltapos=0;
	curden=lastden=4;

	for(ctr = 0; ctr < eof_import_ts_changes[0]->changes; ctr++)
	{	//For each TS change parsed from track 0
		//Find the relevant tempo and nearest preceding beat's time stamp
		for(ctr2 = 0; ctr2 < sp->beats; ctr2++)
		{	//For each beat
			if(sp->beat[ctr2]->midi_pos <= eof_import_ts_changes[0]->change[ctr]->pos)
			{	//If this beat is at or before the target TS change
				BPM = 60000000.0 / sp->beat[ctr2]->ppqn;	//Store the tempo
				realtimepos = sp->beat[ctr2]->fpos;			//Store the realtime position
				realtimepos -= sp->tags->ogg[0].midi_offset;//Subtract the MIDI delay, so that both the TS and BPM change list are native timings without the offset
				deltapos = sp->beat[ctr2]->midi_pos;		//Store the delta time position
				(void) eof_get_ts(sp,NULL,&curden,ctr2);	//Find the TS denominator of this beat
			}
		}

		//Store the TS change's realtime position, using the appropriate formula to find the time beyond the beat real time position if the TS change is not on a beat marker:
		//time = deltas / (time division) * (60000.0 / (BPM * (TS denominator) / 4))
		eof_import_ts_changes[0]->change[ctr]->realtime = (double)realtimepos + (eof_import_ts_changes[0]->change[ctr]->pos - deltapos) / eof_work_midi->divisions * (60000.0 / (BPM * lastden / 4.0));
		lastden=curden;
	}

	/* apply any key signatures parsed in the MIDI */
	for(ctr = 0; ctr < eof_import_ks_events->events; ctr++)
	{	//For each key signature parsed
		event_realtime = eof_ConvertToRealTimeInt(eof_import_ks_events->event[ctr]->pos,anchorlist,eof_import_ts_changes[0],eof_work_midi->divisions,sp->tags->ogg[0].midi_offset);
		eof_chart_length = event_realtime;	//Satisfy eof_get_beat() by ensuring this variable isn't smaller than the looked up timestamp
		beat = eof_get_beat(sp, event_realtime);
		if(beat >= 0)
		{	//If the key signature is placed at a valid timestamp
			(void) eof_apply_key_signature(eof_import_ks_events->event[ctr]->d1, beat, sp);	//Apply it to the beat
		}
	}

	/* third pass, create EOF notes */
	eof_log("\tPass three, creating notes", 1);

	//Special case:  Very old charts created in Freetar Editor may only contain one track that includes all the tempo and note events
	if((tracks == 1) && (eof_import_events[0]->type < 0))
	{	//If there was only one track and it was not identified
		eof_import_events[0]->type = 0;	//Ensure it isn't marked to be skipped, the logic below will cause it to import as a guitar track
	}

	for(i = 0; i < tracks; i++)
	{	//For each imported track
		if(eof_import_events[i]->type < 0)
		{	//If this track is to be skipped (ie. unidentified track)
			continue;
		}
		picked_track = eof_import_events[i]->type >= 1 ? eof_import_events[i]->type : rbg == 0 ? EOF_TRACK_GUITAR : -1;
		if((picked_track >= 0) && !used_track[picked_track] && (picked_track < sp->tracks))
		{	//If this is a valid track to process
			int last_105 = 0;
			int last_106 = 0;
			int overdrive_pos = -1;
			char rb_pro_yellow = 0;					//Tracks the status of forced yellow pro drum notation
			unsigned long rb_pro_yellow_pos = 0;	//Tracks the last start time of a forced yellow pro drum phrase
			char rb_pro_blue = 0;					//Tracks the status of forced yellow pro drum notation
			unsigned long rb_pro_blue_pos = 0;		//Tracks the last start time of a forced blue pro drum phrase
			char rb_pro_green = 0;					//Tracks the status of forced yellow pro drum notation
			unsigned long rb_pro_green_pos = 0;		//Tracks the last start time of a forced green pro drum phrase
			unsigned long notenum = 0;
			char fretwarning = 0;					//Tracks whether the user was warned about the track violating its standard fret limit, if applicable
			unsigned long linestart = 0;			//Tracks the start of a line of lyrics for Power Gig MIDIs, which don't formally mark line placements with a note
			char linetrack = 0;						//Is set to nonzero when linestart is tracking the position of the current line of lyrics

			first_note = note_count[picked_track];
			tracknum = sp->track[picked_track]->tracknum;

//Detect whether Pro drum notation is being used
//Pro drum notation is that if a green, yellow or blue drum note is NOT to be marked as a cymbal,
//it must be marked with the appropriate MIDI note, otherwise the note defaults as a cymbal
			if(eof_ini_pro_drum_tag_present)
			{	//If the "pro_drums = True" ini tag was present
				prodrums = 1;	//Assume pro drum markers are present in the MIDI
			}
			else if(eof_midi_tracks[picked_track].track_behavior == EOF_DRUM_TRACK_BEHAVIOR)
			{	//Otherwise determine by scanning the notes.  If the track being imported is a drum track,
				for(j = 0; j < eof_import_events[i]->events; j++)
				{	//For each event in the track
					if(eof_import_events[i]->event[j]->type == 0x90)
					{	//If this is a Note on event
						if(	(eof_import_events[i]->event[j]->d1 == RB3_DRUM_YELLOW_FORCE) ||
							(eof_import_events[i]->event[j]->d1 == RB3_DRUM_BLUE_FORCE) ||
							(eof_import_events[i]->event[j]->d1 == RB3_DRUM_GREEN_FORCE))
						{	//If this is a pro drum marker
								prodrums = 1;
								break;
						}
					}
				}
			}

#ifdef EOF_DEBUG
			if(eof_import_events[i]->game == 0)
			{	//Rock Band format MIDI track
				(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\tParsing track \"%s\"", eof_midi_tracks[picked_track].name);
			}
			else
			{	//Power Gig format MIDI track
				(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\tParsing track \"%s\"", eof_power_gig_tracks[eof_import_events[i]->tracknum].name);
			}
			eof_log(eof_log_string, 1);
#endif

			for(j = 0; j < eof_import_events[i]->events; j++)
			{	//For each event in this track
				if(key[KEY_ESC])
				{	/* clean up and return */
					eof_import_destroy_events_list(eof_import_bpm_events);
					eof_import_destroy_events_list(eof_import_text_events);
					eof_import_destroy_events_list(eof_import_ks_events);
					eof_destroy_tempo_list(anchorlist);
					for(i = 0; i < tracks; i++)
					{
						eof_import_destroy_events_list(eof_import_events[i]);
					}
					destroy_midi(eof_work_midi);
					eof_destroy_song(sp);
					set_window_title("EOF - No Song");
					return NULL;
				}
				if(pticker % 200 == 0)
				{
					percent = (pticker * 100) / ptotal_events;
					(void) snprintf(ttit, sizeof(ttit) - 1, "MIDI Import (%d%%)", percent <= 100 ? percent : 100);
					set_window_title(ttit);
				}

				event_realtime = eof_ConvertToRealTimeInt(eof_import_events[i]->event[j]->pos,anchorlist,eof_import_ts_changes[0],eof_work_midi->divisions,sp->tags->ogg[0].midi_offset);
				eof_track_resize(sp, picked_track, note_count[picked_track] + 1);	//Ensure the track can accommodate another note

				if((eof_import_events[i]->event[j]->type == 0x01) && (ustrstr(eof_import_events[i]->event[j]->text, "[chrd") != eof_import_events[i]->event[j]->text))
				{	//If this is a track specific text event (chord names excluded, that will be handled by track specific logic)
					for(k = 0; k + 1 < sp->beats; k++)
					{	//For each beat
						if(event_realtime < sp->beat[k + 1]->pos)
						{	//If the text event is earlier than the next beat
							break;	//Stop parsing beats
						}
					}	//k now refers to the appropriate beat this event is assigned to
					(void) eof_song_add_text_event(sp, k, eof_import_events[i]->event[j]->text, picked_track, 0, 0);	//Store this as a text event specific to the track being parsed
				}

				if(eof_midi_tracks[picked_track].track_format == EOF_VOCAL_TRACK_FORMAT)
				{	//If parsing a vocal track
					/* note on */
					if(eof_import_events[i]->event[j]->type == 0x90)
					{
						/* lyric line indicator */
						if(eof_import_events[i]->event[j]->d1 == 105)
						{
							sp->vocal_track[0]->line[sp->vocal_track[0]->lines].start_pos = event_realtime;
							sp->vocal_track[0]->line[sp->vocal_track[0]->lines].flags=0;	//Init flags for this line as 0
							last_105 = sp->vocal_track[0]->lines;
						}
						else if(eof_import_events[i]->event[j]->d1 == 106)
						{
							sp->vocal_track[0]->line[sp->vocal_track[0]->lines].start_pos = event_realtime;
							sp->vocal_track[0]->line[sp->vocal_track[0]->lines].flags=0;	//Init flags for this line as 0
							last_106 = sp->vocal_track[0]->lines;
						}
						/* overdrive */
						else if(eof_import_events[i]->event[j]->d1 == 116)
						{
							overdrive_pos = event_realtime;
						}
						else if((eof_import_events[i]->event[j]->d1 == 96) || (eof_import_events[i]->event[j]->d1 == 97) || ((eof_import_events[i]->event[j]->d1 >= MINPITCH) && (eof_import_events[i]->event[j]->d1 <= MAXPITCH)))
						{	//If this is a vocal percussion note (96 or 97) or if it is a valid vocal pitch
							for(k = 0; k < note_count[picked_track]; k++)
							{
								if(sp->vocal_track[0]->lyric[k]->pos == event_realtime)
								{
									break;
								}
							}
							/* no note associated with this lyric so create a new note */
							if(k == note_count[picked_track])
							{
								sp->vocal_track[0]->lyric[note_count[picked_track]]->note = eof_import_events[i]->event[j]->d1;
								sp->vocal_track[0]->lyric[note_count[picked_track]]->pos = event_realtime;
								sp->vocal_track[0]->lyric[note_count[picked_track]]->length = 100;
								note_count[picked_track]++;
							}
							else
							{
								sp->vocal_track[0]->lyric[k]->note = eof_import_events[i]->event[j]->d1;
							}
						}
					}

					/* note off so get length of note */
					else if(eof_import_events[i]->event[j]->type == 0x80)
					{
						/* lyric line indicator */
						if(eof_import_events[i]->event[j]->d1 == 105)
						{
							sp->vocal_track[0]->line[last_105].end_pos = event_realtime;
							sp->vocal_track[0]->lines++;
							if(overdrive_pos == sp->vocal_track[0]->line[last_105].start_pos)
							{
								sp->vocal_track[0]->line[last_105].flags |= EOF_LYRIC_LINE_FLAG_OVERDRIVE;
							}
						}
						else if(eof_import_events[i]->event[j]->d1 == 106)
						{
							sp->vocal_track[0]->line[last_106].end_pos = event_realtime;
							sp->vocal_track[0]->lines++;
							if(overdrive_pos == sp->vocal_track[0]->line[last_106].start_pos)
							{
								sp->vocal_track[0]->line[last_106].flags |= EOF_LYRIC_LINE_FLAG_OVERDRIVE;
							}
						}
						/* overdrive */
						else if(eof_import_events[i]->event[j]->d1 == 116)
						{
						}
						/* percussion */
						else if((eof_import_events[i]->event[j]->d1 == 96) || (eof_import_events[i]->event[j]->d1 == 97))
						{
						}
						else if((eof_import_events[i]->event[j]->d1 >= MINPITCH) && (eof_import_events[i]->event[j]->d1 <= MAXPITCH))
						{
							if(note_count[picked_track] > 0)
							{
								sp->vocal_track[0]->lyric[note_count[picked_track] - 1]->length = event_realtime - sp->vocal_track[0]->lyric[note_count[picked_track] - 1]->pos;
							}
						}
					}

					/* lyric */
					else if(((eof_import_events[i]->event[j]->type == 0x05) || (eof_import_events[i]->event[j]->type == 0x01)) && (eof_import_events[i]->event[j]->text[0] != '['))
					{	//!Note: The text event import puts all text events in a global list instead of the track event list, so it's not currently possible for EOF to import text events as lyrics
						if(eof_import_events[i]->game == 1)
						{	//If a Power Gig MIDI is being imported
							unsigned long len = ustrlen(eof_import_events[i]->event[j]->text);
							if(len)
							{	//If the lyric has text
								for(k = 0; k < len; k++)
								{	//For each character in the text event
									if(ugetat(eof_import_events[i]->event[j]->text, k) == '*')
									{	//If it is an asterisk (what Power Gig uses to denote a pitch shift)
										usetat(eof_import_events[i]->event[j]->text, k, '+');	//Replace it with a plus sign, the Rock Band equivalent
									}
								}
								if(ugetat(eof_import_events[i]->event[j]->text, len - 1) == ' ')
								{	//If the last character in the string is a space
									usetat(eof_import_events[i]->event[j]->text, len - 1, '\0');	//Truncate the character from the string
								}
								else
								{	//Otherwise the next lyric groups with this one, append a hyphen
									if(len < EOF_MAX_MIDI_TEXT_SIZE)
									{	//If the array storing the string can accommodate one more character
										usetat(eof_import_events[i]->event[j]->text, len, '-');
										usetat(eof_import_events[i]->event[j]->text, len + 1, '\0');
									}
								}
							}
						}

						//Determine which note this lyric event lines up with
						for(k = 0; k < note_count[picked_track]; k++)
						{
							if(sp->vocal_track[0]->lyric[k]->pos == event_realtime)
							{
								break;
							}
						}

						/* no note associated with this lyric so create a new note */
						if(k == note_count[picked_track])
						{
							if(ustrstr(eof_import_events[i]->event[j]->text, "\\r"))
							{	//If this lyric contains the string Power Gig uses for a line break, add the line definition
								sp->vocal_track[0]->line[sp->vocal_track[0]->lines].start_pos = linestart;
								sp->vocal_track[0]->line[sp->vocal_track[0]->lines].end_pos = event_realtime;
								sp->vocal_track[0]->line[sp->vocal_track[0]->lines].flags = 0;	//Init flags for this line as 0
								sp->vocal_track[0]->lines++;
								linetrack = 0;
							}
							else
							{	//Otherwise add the lyric
								strcpy(sp->vocal_track[0]->lyric[note_count[picked_track]]->text, eof_import_events[i]->event[j]->text);
								sp->vocal_track[0]->lyric[note_count[picked_track]]->note = 0;
								sp->vocal_track[0]->lyric[note_count[picked_track]]->pos = event_realtime;
								sp->vocal_track[0]->lyric[note_count[picked_track]]->length = 100;
								if(!linetrack)
								{	//If this is the first lyric in this line of lyrics
									linestart = event_realtime;	//This is the starting position
									linetrack = 1;
								}
								note_count[picked_track]++;
							}
						}
						else
						{
							strcpy(sp->vocal_track[0]->lyric[k]->text, eof_import_events[i]->event[j]->text);
							if(!linetrack)
							{	//If this is the first lyric in this line of lyrics
								linestart = event_realtime;	//This is the starting position
								linetrack = 1;
							}
						}
					}
				}//If parsing a vocal track
				else if(eof_midi_tracks[picked_track].track_format == EOF_LEGACY_TRACK_FORMAT)
				{	//If parsing a legacy track
					if((eof_import_events[i]->event[j]->type == 0x80) || (eof_import_events[i]->event[j]->type == 0x90))
					{	//For note on/off events, determine the type (difficulty) of the event
						diff = -1;	//No difficulty is assumed until the note number is examined
						if(picked_track != EOF_TRACK_DANCE)
						{	//All legacy style tracks besides the dance track use the same offsets
							if(eof_import_events[i]->game == 1)
							{	//If the MIDI being parsed is a Power Gig MIDI, remap it to Rock Band numbering
								diff = eof_import_events[i]->diff;
								switch(eof_import_events[i]->event[j]->d1)
								{
									case 62:
										lane = 0;
									break;
									case 64:
										lane = 1;
									break;
									case 65:
										lane = 2;
									break;
									case 67:
										lane = 3;
									break;
									case 69:
										lane = 4;
									break;
									default:
										lane = 0;
										diff = -1;	//Unknown item
									break;
								}
							}
							else
							{	//Otherwise use the MIDI values used in Rock Band
								if((eof_import_events[i]->event[j]->d1 >= 60) && (eof_import_events[i]->event[j]->d1 < 60 + 6))
								{	//Lane 5 + 1 is used for a lane 5 drum gem, so include it in the difficulty checks here
									lane = eof_import_events[i]->event[j]->d1 - 60;
									diff = EOF_NOTE_SUPAEASY;
								}
								else if((eof_import_events[i]->event[j]->d1 >= 72) && (eof_import_events[i]->event[j]->d1 < 72 + 6))
								{
									lane = eof_import_events[i]->event[j]->d1 - 72;
									diff = EOF_NOTE_EASY;
								}
								else if((eof_import_events[i]->event[j]->d1 >= 84) && (eof_import_events[i]->event[j]->d1 < 84 + 6))
								{
									lane = eof_import_events[i]->event[j]->d1 - 84;
									diff = EOF_NOTE_MEDIUM;
								}
								else if((eof_import_events[i]->event[j]->d1 >= 96) && (eof_import_events[i]->event[j]->d1 < 102))
								{
									lane = eof_import_events[i]->event[j]->d1 - 96;
									diff = EOF_NOTE_AMAZING;
								}
								else if((eof_import_events[i]->event[j]->d1 >= 120) && (eof_import_events[i]->event[j]->d1 <= 124))
								{
									lane = eof_import_events[i]->event[j]->d1 - 120;
									diff = EOF_NOTE_SPECIAL;
								}
								else if(((picked_track == EOF_TRACK_DRUM) || (picked_track == EOF_TRACK_DRUM_PS)) && (eof_import_events[i]->event[j]->d1 == 95))
								{	//If the track being read is a drum track, and this note is marked for Expert+ double bass
									lane = eof_import_events[i]->event[j]->d1 - 96 + 1;	//Treat as gem 1 (bass drum)
									diff = EOF_NOTE_AMAZING;
								}
								else
								{
									lane = 0;	//No defined lane
									diff = -1;	//No defined difficulty
								}
							}
						}
						else
						{	//This is the dance track
							if((eof_import_events[i]->event[j]->d1 >= 48) && (eof_import_events[i]->event[j]->d1 < 60))
							{	//Notes 48-59
								lane = eof_import_events[i]->event[j]->d1 - 48;
								diff = EOF_NOTE_SUPAEASY;
							}
							else if((eof_import_events[i]->event[j]->d1 >= 60) && (eof_import_events[i]->event[j]->d1 < 72))
							{	//notes 60-71
								lane = eof_import_events[i]->event[j]->d1 - 60;
								diff = EOF_NOTE_EASY;
							}
							else if((eof_import_events[i]->event[j]->d1 >= 72) && (eof_import_events[i]->event[j]->d1 < 84))
							{	//notes 72-83
								lane = eof_import_events[i]->event[j]->d1 - 72;
								diff = EOF_NOTE_MEDIUM;
							}
							else if((eof_import_events[i]->event[j]->d1 >= 84) && (eof_import_events[i]->event[j]->d1 < 96))
							{	//notes 84-95
								lane = eof_import_events[i]->event[j]->d1 - 84;
								diff = EOF_NOTE_AMAZING;
							}
							else if((eof_import_events[i]->event[j]->d1 >= 96) && (eof_import_events[i]->event[j]->d1 < 108))
							{	//notes 96-107
								lane = eof_import_events[i]->event[j]->d1 - 96;
								diff = EOF_NOTE_CHALLENGE;
							}
							else
							{
								lane = 0;	//No defined lane
								diff = -1;	//No defined difficulty
							}
						}
					}//Note on or note off

					/* note on */
					if(eof_import_events[i]->event[j]->type == 0x90)
					{
#ifdef EOF_DEBUG
						(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tNote on:  %d (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->d1, eof_import_events[i]->event[j]->pos, event_realtime);
						eof_log(eof_log_string, 1);
#endif
						char doublebass = 0;

						if(eof_import_events[i]->game == 0)
						{	//If the MIDI is in Rock Band/FoF/Phase Shift notation
							if(((picked_track == EOF_TRACK_DRUM) || (picked_track == EOF_TRACK_DRUM_PS)) && (eof_import_events[i]->event[j]->d1 == 95))
							{	//If the track being read is a drum track, and this note is marked for Expert+ double bass
								doublebass = 1;	//Track that double bass was found for this note, and apply it after the note flag for a newly created note is initialized to zero
							}
							if(eof_midi_tracks[picked_track].track_behavior == EOF_DRUM_TRACK_BEHAVIOR)
							{	//If this is a drum track, lane 6 is used for the fifth drum lane and not a HOPO marker
								if(lane == 5)
								{	//A lane 6 gem encountered for a drum track will cause the track to be marked as being a "five lane" drum track
									sp->track[picked_track]->flags |= EOF_TRACK_FLAG_SIX_LANES;	//Set the six lane flag
									sp->legacy_track[tracknum]->numlanes = 6;
								}
							}
							else
							{	/* store forced HOPO marker, when the note off for this marker occurs, search for note with same position and apply it to that note */
								//Forced HOPO on are marked as lane 1 + 5
								if(eof_import_events[i]->event[j]->d1 == 60 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[0] = event_realtime;
									hopotype[0] = 0;
								}
								else if(eof_import_events[i]->event[j]->d1 == 72 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[1] = event_realtime;
									hopotype[1] = 0;
								}
								else if(eof_import_events[i]->event[j]->d1 == 84 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[2] = event_realtime;
									hopotype[2] = 0;
								}
								else if(eof_import_events[i]->event[j]->d1 == 96 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[3] = event_realtime;
									hopotype[3] = 0;
								}
								//Forced HOPO off are marked as lane 1 + 6
								else if(eof_import_events[i]->event[j]->d1 == 60 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[0] = event_realtime;
									hopotype[0] = 1;
								}
								else if(eof_import_events[i]->event[j]->d1 == 72 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[1] = event_realtime;
									hopotype[1] = 1;
								}
								else if(eof_import_events[i]->event[j]->d1 == 84 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[2] = event_realtime;
									hopotype[2] = 1;
								}
								else if(eof_import_events[i]->event[j]->d1 == 96 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopopos[3] = event_realtime;
									hopotype[3] = 1;
								}
							}//store forced HOPO marker

							/* solos, star power, tremolos and trills */
							phraseptr = NULL;
							if((eof_import_events[i]->event[j]->d1 == 103) && (eof_get_num_solos(sp, picked_track) < EOF_MAX_PHRASES))
							{	//Legacy solos are marked with note 103
								phraseptr = eof_get_solo(sp, picked_track, eof_get_num_solos(sp, picked_track));
								phraseptr->start_pos = event_realtime;
							}
							else if((eof_import_events[i]->event[j]->d1 == 116) && (eof_get_num_star_power_paths(sp, picked_track) < EOF_MAX_PHRASES))
							{	//Star power is marked with note 116
								phraseptr = eof_get_star_power_path(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track));
								phraseptr->start_pos = event_realtime;
							}
							else if((eof_import_events[i]->event[j]->d1 == 126) && (eof_get_num_tremolos(sp, picked_track) < EOF_MAX_PHRASES))
							{	//Tremolos are marked with note 126
								phraseptr = eof_get_tremolo(sp, picked_track, eof_get_num_tremolos(sp, picked_track));
								phraseptr->start_pos = event_realtime;
								phraseptr->difficulty = 0xFF;	//Tremolo phrases imported from MIDI apply to all difficulties
							}
							else if((eof_import_events[i]->event[j]->d1 == 127) && (eof_get_num_trills(sp, picked_track) < EOF_MAX_PHRASES))
							{	//Trills are marked with note 127
								phraseptr = eof_get_trill(sp, picked_track, eof_get_num_trills(sp, picked_track));
								phraseptr->start_pos = event_realtime;
							}
							if(phraseptr)
							{	//If a phrase was created, initialize some other values
								phraseptr->flags = 0;
								phraseptr->name[0] = '\0';
							}

							/* rb pro markers */
							if(prodrums)
							{	//If the track was already found to have these markers
								if(eof_import_events[i]->event[j]->d1 == RB3_DRUM_YELLOW_FORCE)
								{
									rb_pro_yellow = 1;
									rb_pro_yellow_pos = event_realtime;
								}
								else if(eof_import_events[i]->event[j]->d1 == RB3_DRUM_BLUE_FORCE)
								{
									rb_pro_blue = 1;
									rb_pro_blue_pos = event_realtime;
								}
								else if(eof_import_events[i]->event[j]->d1 == RB3_DRUM_GREEN_FORCE)
								{
									rb_pro_green = 1;
									rb_pro_green_pos = event_realtime;
								}
							}
						}//If the MIDI is in Rock Band/FoF/Phase Shift notation

						if(diff != -1)
						{	//If a note difficulty was identified above
							char match = 0;	//Is set to nonzero if this note on is matched with a previously added note

							k = 0;
							if(note_count[picked_track] > 0)
							{	//If at least one note has been added already
								for(k = note_count[picked_track] - 1; k >= first_note; k--)
								{	//Traverse the note list in reverse order to identify the appropriate note to modify
									if((eof_get_note_pos(sp, picked_track, k) == event_realtime) && (eof_get_note_type(sp, picked_track, k) == diff))
									{	//If the note is at the same timestamp and difficulty
										match = 1;
										break;	//End the search, as a match was found
									}
								}
							}
							if(!match)
							{	//If the note doesn't match the same time and difficulty as an existing note, this MIDI event represents a new note, initialize it now and increment the note count
								notenum = note_count[picked_track];
								eof_set_note_note(sp, picked_track, notenum, lane_chart[lane]);
								eof_set_note_pos(sp, picked_track, notenum, event_realtime);
								eof_set_note_length(sp, picked_track, notenum, 0);				//The length will be kept at 0 until the end of the note is found
								eof_set_note_flags(sp, picked_track, notenum, 0);				//Clear the flag here so that the flag can be set later (ie. if it's an Expert+ double bass note)
								eof_set_note_type(sp, picked_track, notenum, diff);				//Apply the determined difficulty
								note_count[picked_track]++;
#ifdef EOF_DEBUG
								(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tInitializing note #%lu (Diff=%d):  Mask=%u, Pos=%lu, Length=%d", notenum, eof_get_note_type(sp, picked_track, notenum), lane_chart[lane], event_realtime, 0);
								eof_log(eof_log_string, 1);
#endif
							}
							else
							{	//Otherwise just modify the existing note by adding a gem
								notenum = k;
#ifdef EOF_DEBUG
								(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tModifying note #%lu (Diff=%d, Pos=%lu) from mask %lu to %lu", notenum, eof_get_note_type(sp, picked_track, notenum), event_realtime, eof_get_note_note(sp, picked_track, notenum), eof_get_note_note(sp, picked_track, notenum) | lane_chart[lane]);
								eof_log(eof_log_string, 1);
#endif
								eof_set_note_note(sp, picked_track, notenum, eof_get_note_note(sp, picked_track, notenum) | lane_chart[lane]);
							}
							if((picked_track == EOF_TRACK_DRUM) || (picked_track == EOF_TRACK_DRUM_PS))
							{
								if(doublebass)
								{	//If the note was found to be double bass
									eof_set_note_flags(sp, picked_track, notenum, eof_get_note_flags(sp, picked_track, notenum) | EOF_NOTE_FLAG_DBASS);	//Set the double bass flag
								}
								if(prodrums && (eof_get_note_type(sp, picked_track, notenum) != EOF_NOTE_SPECIAL))
								{	//If pro drum notation is in effect and this was a non BRE drum note, assume cymbal notation until a tom marker ending is found
									if(eof_get_note_note(sp, picked_track, notenum) & 4)
									{	//This is a yellow drum note, assume it is a cymbal unless a pro drum phrase indicates otherwise
										eof_set_note_flags(sp, picked_track, notenum, eof_get_note_flags(sp, picked_track, notenum) | EOF_NOTE_FLAG_Y_CYMBAL);	//Ensure the cymbal flag is set
									}
									if(eof_get_note_note(sp, picked_track, notenum) & 8)
									{	//This is a blue drum note, assume it is a cymbal unless a pro drum phrase indicates otherwise
										eof_set_note_flags(sp, picked_track, notenum, eof_get_note_flags(sp, picked_track, notenum) | EOF_NOTE_FLAG_B_CYMBAL);	//Ensure the cymbal flag is set
									}
									if(eof_get_note_note(sp, picked_track, notenum) & 16)
									{	//This is a purle drum note (green in Rock Band), assume it is a cymbal unless a pro drum phrase indicates otherwise
										eof_set_note_flags(sp, picked_track, notenum, eof_get_note_flags(sp, picked_track, notenum) | EOF_NOTE_FLAG_G_CYMBAL);	//Ensure the cymbal flag is set
									}
								}
							}
						}//If a note difficulty was identified above
					}//Note on event

					/* note off so get length of note */
					else if(eof_import_events[i]->event[j]->type == 0x80)
					{
#ifdef EOF_DEBUG
						(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tNote off:  %d (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->d1, eof_import_events[i]->event[j]->pos, event_realtime);
						eof_log(eof_log_string, 1);
#endif

						if(eof_import_events[i]->game == 0)
						{	//If the MIDI is in Rock Band/FoF/Phase Shift notation
							if(eof_midi_tracks[picked_track].track_behavior == EOF_DRUM_TRACK_BEHAVIOR)
							{	//If this is a drum track, lane 6 is used for the fifth drum lane and not a HOPO marker
							}
							else
							{	/* detect forced HOPO */
								hopodiff = -1;
								if(eof_import_events[i]->event[j]->d1 == 60 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 0;
								}
								else if(eof_import_events[i]->event[j]->d1 == 72 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 1;
								}
								else if(eof_import_events[i]->event[j]->d1 == 84 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 2;
								}
								else if(eof_import_events[i]->event[j]->d1 == 96 + 5)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 3;
								}
								else if(eof_import_events[i]->event[j]->d1 == 60 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 0;
								}
								else if(eof_import_events[i]->event[j]->d1 == 72 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 1;
								}
								else if(eof_import_events[i]->event[j]->d1 == 84 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 2;
								}
								else if(eof_import_events[i]->event[j]->d1 == 96 + 6)
								{
									diff = -1;	//HOPO Markers will not be processed as regular gems
									hopodiff = 3;
								}
								if(hopodiff >= 0)
								{
									for(k = note_count[picked_track] - 1; k >= first_note; k--)
									{	//Check (in reverse) for each note that has been imported
										if((eof_get_note_type(sp, picked_track, k) == hopodiff) && (eof_get_note_pos(sp, picked_track, k) >= hopopos[hopodiff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
										{	//If the note is in the same difficulty as the HOPO phrase, and its timestamp falls between the HOPO On and HOPO Off marker
											if(hopotype[hopodiff] == 0)
											{	//Forced HOPO on
												eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_NOTE_FLAG_F_HOPO);
											}
											else
											{	//Forced HOPO off
												eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_NOTE_FLAG_NO_HOPO);
											}
										}
									}
								}
							}/* detect forced HOPO */

							/* rb pro markers */
							if(prodrums)
							{	//If the track was already found to have these markers
								if((eof_import_events[i]->event[j]->d1 == RB3_DRUM_YELLOW_FORCE) && rb_pro_yellow)
								{	//If this event ends a pro yellow drum phrase
									for(k = note_count[picked_track] - 1; k >= first_note; k--)
									{	//Check for each note that has been imported for this track
										if((sp->legacy_track[tracknum]->note[k]->pos >= rb_pro_yellow_pos) && (sp->legacy_track[tracknum]->note[k]->pos <= event_realtime))
										{	//If the note is positioned within this pro yellow drum phrase
											sp->legacy_track[tracknum]->note[k]->flags &= (~EOF_NOTE_FLAG_Y_CYMBAL);	//Clear the yellow cymbal marker on the note
										}
									}
									rb_pro_yellow = 0;
								}
								else if((eof_import_events[i]->event[j]->d1 == RB3_DRUM_BLUE_FORCE) && rb_pro_blue)
								{	//If this event ends a pro blue drum phrase
									for(k = note_count[picked_track] - 1; k >= first_note; k--)
									{	//Check for each note that has been imported for this track
										if((sp->legacy_track[tracknum]->note[k]->pos >= rb_pro_blue_pos) && (sp->legacy_track[tracknum]->note[k]->pos <= event_realtime))
										{	//If the note is positioned within this pro blue drum phrase
											sp->legacy_track[tracknum]->note[k]->flags &= (~EOF_NOTE_FLAG_B_CYMBAL);	//Clear the blue cymbal marker on the note
										}
									}
									rb_pro_blue = 0;
								}
								else if((eof_import_events[i]->event[j]->d1 == RB3_DRUM_GREEN_FORCE) && rb_pro_green)
								{	//If this event ends a pro green drum phrase
									for(k = note_count[picked_track] - 1; k >= first_note; k--)
									{	//Check for each note that has been imported for this track, in reverse order
										if((sp->legacy_track[tracknum]->note[k]->pos >= rb_pro_green_pos) && (sp->legacy_track[tracknum]->note[k]->pos <= event_realtime))
										{	//If the note is positioned within this pro green drum phrase
											sp->legacy_track[tracknum]->note[k]->flags &= (~EOF_NOTE_FLAG_G_CYMBAL);	//Clear the green cymbal marker on the note
										}
									}
									rb_pro_green = 0;
								}
							}

							if((eof_import_events[i]->event[j]->d1 == 103) && (eof_get_num_solos(sp, picked_track) < EOF_MAX_PHRASES))
							{	//End of a solo phrase
								phraseptr = eof_get_solo(sp, picked_track, eof_get_num_solos(sp, picked_track));
								phraseptr->end_pos = event_realtime;
								eof_set_num_solos(sp, picked_track, eof_get_num_solos(sp, picked_track) + 1);
							}
							else if((eof_import_events[i]->event[j]->d1 == 116) && (eof_get_num_star_power_paths(sp, picked_track) < EOF_MAX_PHRASES))
							{	//End of a star power phrase
								phraseptr = eof_get_star_power_path(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track));
								phraseptr->end_pos = event_realtime;
								eof_set_num_star_power_paths(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track) + 1);
							}
							else if((eof_import_events[i]->event[j]->d1 == 126) && (eof_get_num_tremolos(sp, picked_track) < EOF_MAX_PHRASES))
							{	//End of a tremolo phrase
								phraseptr = eof_get_tremolo(sp, picked_track, eof_get_num_tremolos(sp, picked_track));
								phraseptr->end_pos = event_realtime;
								eof_set_num_tremolos(sp, picked_track, eof_get_num_tremolos(sp, picked_track) + 1);
							}
							else if((eof_import_events[i]->event[j]->d1 == 127) && (eof_get_num_trills(sp, picked_track) < EOF_MAX_PHRASES))
							{	//End of a trill phrase
								phraseptr = eof_get_trill(sp, picked_track, eof_get_num_trills(sp, picked_track));
								phraseptr->end_pos = event_realtime;
								eof_set_num_trills(sp, picked_track, eof_get_num_trills(sp, picked_track) + 1);
							}
						}//If the MIDI is in Rock Band/FoF/Phase Shift notation

						if((note_count[picked_track] > 0) && (diff != -1))
						{	//If there's at least one note on found for this track, and this note off event has a defined difficulty
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{	//Check for each note that has been imported, in reverse order
								if((eof_get_note_type(sp, picked_track, k) == diff) && (eof_get_note_note(sp, picked_track, k) & lane_chart[lane]))
								{	//If the note is in the same difficulty as this note off event and it contains one of the same gems
//									allegro_message("break %d, %d, %d", k, sp->legacy_track[picked_track]->note[k]->note, sp->legacy_track[picked_track]->note[note_count[picked_track]]->note);	//Debug
#ifdef EOF_DEBUG
									(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tModifying note #%ld (Diff=%d, Pos=%lu, Mask=%lu) from length %ld to %ld", k, eof_get_note_type(sp, picked_track, k), eof_get_note_pos(sp, picked_track, k), eof_get_note_note(sp, picked_track, k), eof_get_note_length(sp, picked_track, k), event_realtime - eof_get_note_pos(sp, picked_track, k));
									eof_log(eof_log_string, 1);
#endif
									eof_set_note_length(sp, picked_track, k, event_realtime - eof_get_note_pos(sp, picked_track, k));
									if(eof_get_note_length(sp, picked_track, k ) <= 0)
									{	//If the note somehow received a zero or negative length
										eof_set_note_length(sp, picked_track, k, 1);
									}
									break;
								}
							}
						}
					}//Note off event

					/* Sysex event (custom phrase markers for Phase Shift) */
					else if((eof_import_events[i]->event[j]->type == 0xF0) && eof_import_events[i]->event[j]->dp)
					{	//Sysex event
						if((eof_import_events[i]->event[j]->d1 == 8) && (!strncmp(eof_import_events[i]->event[j]->dp, "PS", 3)))
						{	//If this is a custom Sysex Phase Shift marker (8 bytes long, beginning with the NULL terminated string "PS")
							switch(eof_import_events[i]->event[j]->dp[3])
							{	//Check the value of the message ID
								case 0:	//Phrase marker
									phrasediff = eof_import_events[i]->event[j]->dp[4];	//Store the difficulty ID
									switch(eof_import_events[i]->event[j]->dp[5])
									{	//Check the value of the phrase ID
										case 1:	//Open strum bass
											if(picked_track == EOF_TRACK_BASS)
											{	//Only parse open strum bass phrases for the bass track
#ifdef EOF_DEBUG
												(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Open strum bass (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
												eof_log(eof_log_string, 1);
#endif
												if(eof_import_events[i]->event[j]->dp[6] == 1)
												{	//Start of open strum bass phrase
													openstrumpos[phrasediff] = event_realtime;
												}
												else if(eof_import_events[i]->event[j]->dp[6] == 0)
												{	//End of open strum bass phrase
													for(k = note_count[picked_track] - 1; k >= first_note; k--)
													{	//Check for each note that has been imported
														if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= openstrumpos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
														{	//If the note is in the same difficulty as the open strum bass phrase, and its timestamp falls between the phrase on and phrase off marker
#ifdef EOF_DEBUG
															(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tModifying note #%ld (Diff=%d, Pos=%lu, Mask=%lu, Length=%ld) to have a note mask of 33", k, eof_get_note_type(sp, picked_track, k), eof_get_note_pos(sp, picked_track, k), eof_get_note_note(sp, picked_track, k), eof_get_note_length(sp, picked_track, k));
															eof_log(eof_log_string, 1);
#endif
															eof_set_note_note(sp, picked_track, k, 33);	//Change this note to a lane 1+6 chord (the cleanup logic should later correct this to just a lane 6 gem, EOF's in-editor notation for open strum bass).  This modification is necessary so that the note off event representing the end of the lane 1 gem for an open bass note can be processed properly.
															sp->track[picked_track]->flags = EOF_TRACK_FLAG_SIX_LANES;	//Set this flag
															tracknum = sp->track[EOF_TRACK_BASS]->tracknum;
															sp->legacy_track[tracknum]->numlanes = 6;	//Set this track to have 6 lanes instead of 5
														}
													}
												}
											}
										break;
										case 4:	//Slider
											if((sp->track[picked_track]->track_behavior == EOF_GUITAR_TRACK_BEHAVIOR) && (sp->track[picked_track]->track_format == EOF_LEGACY_TRACK_FORMAT))
											{	//Only parse slider phrases for legacy guitar tracks
#ifdef EOF_DEBUG
												(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Slider (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
												eof_log(eof_log_string, 1);
#endif
												if(eof_import_events[i]->event[j]->dp[6] == 1)
												{	//Start of phrase
													sliderpos[0] = event_realtime;
												}
												else if(eof_import_events[i]->event[j]->dp[6] == 0)
												{	//End of phrase
													phraseptr = eof_get_slider(sp, picked_track, eof_get_num_sliders(sp, picked_track));
													phraseptr->start_pos = sliderpos[0];
													phraseptr->end_pos = event_realtime;
													eof_set_num_sliders(sp, picked_track, eof_get_num_sliders(sp, picked_track) + 1);
												}
											}
										break;
										case 5:	//Open hi hat
											if((picked_track == EOF_TRACK_DRUM) || (picked_track == EOF_TRACK_DRUM_PS))
											{	//Only parse hi hat phrases for the drum tracks
#ifdef EOF_DEBUG
												(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Open hi hat (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
												eof_log(eof_log_string, 1);
#endif
												if(eof_import_events[i]->event[j]->dp[6] == 1)
												{	//Start of phrase
													openhihatpos[phrasediff] = event_realtime;
												}
												else if(eof_import_events[i]->event[j]->dp[6] == 0)
												{	//End of phrase
													for(k = note_count[picked_track] - 1; k >= first_note; k--)
													{	//Check for each note that has been imported
														if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= openhihatpos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
														{	//If the note is in the same difficulty as the phrase, and its timestamp falls between the phrase on and phrase off marker
															eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_DRUM_NOTE_FLAG_Y_HI_HAT_OPEN);	//Set the open hi hat flag
														}
													}
												}
											}
										break;
										case 6:	//Pedal controlled hi hat
											if((picked_track == EOF_TRACK_DRUM) || (picked_track == EOF_TRACK_DRUM_PS))
											{	//Only parse hi hat phrases for the drum tracks
#ifdef EOF_DEBUG
												(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Pedal hi hat (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
												eof_log(eof_log_string, 1);
#endif
												if(eof_import_events[i]->event[j]->dp[6] == 1)
												{	//Start of phrase
													pedalhihatpos[phrasediff] = event_realtime;
												}
												else if(eof_import_events[i]->event[j]->dp[6] == 0)
												{	//End of phrase
													for(k = note_count[picked_track] - 1; k >= first_note; k--)
													{	//Check for each note that has been imported
														if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= pedalhihatpos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
														{	//If the note is in the same difficulty as the phrase, and its timestamp falls between the phrase on and phrase off marker
															eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_DRUM_NOTE_FLAG_Y_HI_HAT_PEDAL);	//Set the pedal controlled hi hat flag
														}
													}
												}
											}
										break;
										case 7:	//Snare rim shot
											if((picked_track == EOF_TRACK_DRUM) || (picked_track == EOF_TRACK_DRUM_PS))
											{	//Only parse rim shot phrases for the drum tracks
#ifdef EOF_DEBUG
												(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Rim shot (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
												eof_log(eof_log_string, 1);
#endif
												if(eof_import_events[i]->event[j]->dp[6] == 1)
												{	//Start of phrase
													rimshotpos[phrasediff] = event_realtime;
												}
												else if(eof_import_events[i]->event[j]->dp[6] == 0)
												{	//End of phrase
													for(k = note_count[picked_track] - 1; k >= first_note; k--)
													{	//Check for each note that has been imported
														if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= rimshotpos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
														{	//If the note is in the same difficulty as the phrase, and its timestamp falls between the phrase on and phrase off marker
															eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_DRUM_NOTE_FLAG_R_RIMSHOT);	//Set the rim shot flag
														}
													}
												}
											}
										break;
										case 8:	//Sizzle hi hat
											if((picked_track == EOF_TRACK_DRUM) || (picked_track == EOF_TRACK_DRUM_PS))
											{	//Only parse hi hat phrases for the drum tracks
#ifdef EOF_DEBUG
												(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Sizzle hi hat (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
												eof_log(eof_log_string, 1);
#endif
												if(eof_import_events[i]->event[j]->dp[6] == 1)
												{	//Start of phrase
													openhihatpos[phrasediff] = event_realtime;
												}
												else if(eof_import_events[i]->event[j]->dp[6] == 0)
												{	//End of phrase
													for(k = note_count[picked_track] - 1; k >= first_note; k--)
													{	//Check for each note that has been imported
														if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= openhihatpos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
														{	//If the note is in the same difficulty as the phrase, and its timestamp falls between the phrase on and phrase off marker
															eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_DRUM_NOTE_FLAG_Y_SIZZLE);	//Set the sizzle hi hat flag
														}
													}
												}
											}
										break;
									}
								break;
							}
						}
						free(eof_import_events[i]->event[j]->dp);	//The the memory allocated to store this Sysex message's data
						eof_import_events[i]->event[j]->dp = NULL;
					}//Sysex event
				}//If parsing a legacy track
				else if(eof_midi_tracks[picked_track].track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
				{	//If parsing a pro guitar track
					if((eof_import_events[i]->event[j]->type == 0x80) || (eof_import_events[i]->event[j]->type == 0x90))
					{	//For note on/off events, determine the type (difficulty) of the event
						if((eof_import_events[i]->event[j]->d1 >= 24) && (eof_import_events[i]->event[j]->d1 <= 29))
						{	//Notes 24 through 29 represent supaeasy pro guitar
							lane = eof_import_events[i]->event[j]->d1 - 24;
							diff = EOF_NOTE_SUPAEASY;
							chordname = chord0name;		//Have this pointer reference the supaeasy note name array
						}
						else if((eof_import_events[i]->event[j]->d1 >= 48) && (eof_import_events[i]->event[j]->d1 <= 53))
						{	//Notes 48 through 53 represent easy pro guitar
							lane = eof_import_events[i]->event[j]->d1 - 48;
							diff = EOF_NOTE_EASY;
							chordname = chord1name;		//Have this pointer reference the easy note name array
						}
						else if((eof_import_events[i]->event[j]->d1 >= 72) && (eof_import_events[i]->event[j]->d1 <= 77))
						{	//Notes 72 through 77 represent medium pro guitar
							lane = eof_import_events[i]->event[j]->d1 - 72;
							diff = EOF_NOTE_MEDIUM;
							chordname = chord2name;		//Have this pointer reference the medium note name array
						}
						else if((eof_import_events[i]->event[j]->d1 >= 96) && (eof_import_events[i]->event[j]->d1 <= 101))
						{	//Notes 96 through 101 represent amazing pro guitar
							lane = eof_import_events[i]->event[j]->d1 - 96;
							diff = EOF_NOTE_AMAZING;
							chordname = chord3name;		//Have this pointer reference the amazing note name array
						}
						else if((eof_import_events[i]->event[j]->d1 >= 120) && (eof_import_events[i]->event[j]->d1 <= 125))
						{
							lane = eof_import_events[i]->event[j]->d1 - 120;
							diff = EOF_NOTE_SPECIAL;
							chordname = NULL;		//BRE notes do not store chord names
						}
						else
						{
							lane = 0;	//No defined lane
							diff = -1;	//No defined difficulty
						}
					}

					/* note on */
					if(eof_import_events[i]->event[j]->type == 0x90)
					{	//Note on event
						unsigned long *slideptr;	//This will point to either the up or down slide position array as appropriate, so the velocity can be checked just once
						char strum = 0;	//Will store the strum type, to reduce duplicated logic below

#ifdef EOF_DEBUG
						(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tNote on:  %d (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->d1, eof_import_events[i]->event[j]->pos, event_realtime);
						eof_log(eof_log_string, 1);
#endif
						/* store forced Ho/Po markers, when the note off for this marker occurs, search for note with same position and apply it to that note */
						//Pro guitar forced Ho/Po are both marked as lane 1 + 6
						if(eof_import_events[i]->event[j]->d1 == 24 + 6)
						{
							hopopos[0] = event_realtime;
							hopotype[0] = 1;
						}
						else if(eof_import_events[i]->event[j]->d1 == 48 + 6)
						{
							hopopos[1] = event_realtime;
							hopotype[1] = 1;
						}
						else if(eof_import_events[i]->event[j]->d1 == 72 + 6)
						{
							hopopos[2] = event_realtime;
							hopotype[2] = 1;
						}
						else if(eof_import_events[i]->event[j]->d1 == 96 + 6)
						{
							hopopos[3] = event_realtime;
							hopotype[3] = 1;
						}

						/* store slide marker, when the note off for this marker occurs, search for notes within the phrase and apply the status to them */
						//Slide phrases are marked as lane (1 + 7)
						if(eof_import_events[i]->event[j]->d2 >= 108)
						{	//For slide markers, velocity 108 or higher represents a down slide
							slideptr = slidedownpos;	//The slide marker (if present) would be a down slide
						}
						else
						{	//Other velocities represent an up slide
							slideptr = slideuppos;		//The slide marker (if present) would be an up slide
						}
						if(eof_import_events[i]->event[j]->d1 == 24 + 7)
						{
							slideptr[0] = event_realtime;
							slidevelocity[0] = eof_import_events[i]->event[j]->d2;	//Cache the velocity of the slide, because the slide end marker won't use it
						}
						else if(eof_import_events[i]->event[j]->d1 == 48 + 7)
						{
							slideptr[1] = event_realtime;
							slidevelocity[1] = eof_import_events[i]->event[j]->d2;	//Cache the velocity of the slide, because the slide end marker won't use it
						}
						else if(eof_import_events[i]->event[j]->d1 == 72 + 7)
						{
							slideptr[2] = event_realtime;
							slidevelocity[2] = eof_import_events[i]->event[j]->d2;	//Cache the velocity of the slide, because the slide end marker won't use it
						}
						else if(eof_import_events[i]->event[j]->d1 == 96 + 7)
						{
							slideptr[3] = event_realtime;
							slidevelocity[3] = eof_import_events[i]->event[j]->d2;	//Cache the velocity of the slide, because the slide end marker won't use it
						}

						/* store arpeggio marker, when the note off for this marker occurs, search for notes with same position and apply it to them */
						//Arpeggio phrases on are marked as lane (1 + 8)
						if(eof_import_events[i]->event[j]->d1 == 24 + 8)
						{
							arpegpos[0] = event_realtime;
						}
						else if(eof_import_events[i]->event[j]->d1 == 48 + 8)
						{
							arpegpos[1] = event_realtime;
						}
						else if(eof_import_events[i]->event[j]->d1 == 72 + 8)
						{
							arpegpos[2] = event_realtime;
						}
						else if(eof_import_events[i]->event[j]->d1 == 96 + 8)
						{
							arpegpos[3] = event_realtime;
						}

						/* store strum direction marker, when the note off for this marker occurs, search for notes with same position and apply it to them */
						//Lane (1 + 9), channel 13 is used in up strum markers
						if(eof_import_events[i]->event[j]->channel == 13)
						{	//Channel 13 is used in up strum markers
							strum = 0;
						}
						else if(eof_import_events[i]->event[j]->channel == 14)
						{	//Channel 14 is used in mid strum markers
							strum = 1;
						}
						else if(eof_import_events[i]->event[j]->channel == 15)
						{	//Channel 15 is used in down strum markers
							strum = 2;
						}
						if(eof_import_events[i]->event[j]->d1 == 24 + 9)
						{
							strumpos[0] = event_realtime;
							strumtype[0] = strum;
						}
						else if(eof_import_events[i]->event[j]->d1 == 48 + 9)
						{
							strumpos[1] = event_realtime;
							strumtype[1] = strum;
						}
						else if(eof_import_events[i]->event[j]->d1 == 72 + 9)
						{
							strumpos[2] = event_realtime;
							strumtype[2] = strum;
						}
						else if(eof_import_events[i]->event[j]->d1 == 96 + 9)
						{
							strumpos[3] = event_realtime;
							strumtype[3] = strum;
						}

						/* arpeggios, solos, star power, tremolos and trills */
						phraseptr = NULL;
						if((eof_import_events[i]->event[j]->d1 == 115) && (eof_get_num_solos(sp, picked_track) < EOF_MAX_PHRASES))
						{	//Pro guitar solos are marked with note 115
							phraseptr = eof_get_solo(sp, picked_track, eof_get_num_solos(sp, picked_track));
							phraseptr->start_pos = event_realtime;
						}
						else if((eof_import_events[i]->event[j]->d1 == 116) && (eof_get_num_star_power_paths(sp, picked_track) < EOF_MAX_PHRASES))
						{	//Star power is marked with note 116
							phraseptr = eof_get_star_power_path(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track));
							phraseptr->start_pos = event_realtime;
						}
						else if((eof_import_events[i]->event[j]->d1 == 126) && (eof_get_num_tremolos(sp, picked_track) < EOF_MAX_PHRASES))
						{	//Tremolos are marked with note 126
							phraseptr = eof_get_tremolo(sp, picked_track, eof_get_num_tremolos(sp, picked_track));
							phraseptr->start_pos = event_realtime;
							phraseptr->difficulty = 0xFF;	//Tremolo phrases imported from MIDI apply to all difficulties
						}
						else if((eof_import_events[i]->event[j]->d1 == 127) && (eof_get_num_trills(sp, picked_track) < EOF_MAX_PHRASES))
						{	//Trills are marked with note 127
							phraseptr = eof_get_trill(sp, picked_track, eof_get_num_trills(sp, picked_track));
							phraseptr->start_pos = event_realtime;
						}
						if(phraseptr)
						{	//If a phrase was created, initialize some other values
							phraseptr->flags = 0;
							phraseptr->name[0] = '\0';
						}

						/* fret hand positions */
						if(eof_import_events[i]->event[j]->d1 == 108)
						{	//Pro guitar fret hand positions are marked with note 108
							if(eof_import_events[i]->event[j]->d2 < 100)
							{	//If this is an invalid velocity for a fret hand position
#ifdef EOF_DEBUG
								(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tModifying fret hand position at pos=%lu from velocity %d to 101", event_realtime, eof_import_events[i]->event[j]->d2);
								eof_log(eof_log_string, 1);
#endif
								eof_import_events[i]->event[j]->d2 = 101;	//Reset it to a position of 1
							}
							(void) eof_track_add_section(sp, picked_track, EOF_FRET_HAND_POS_SECTION, EOF_NOTE_AMAZING, event_realtime, eof_import_events[i]->event[j]->d2 - 100, 0, NULL);
						}

						if(diff != -1)
						{	//If a note difficulty was identified above
							char match = 0;	//Is set to nonzero if this note on is matched with a previously added note

							k = 0;
							if(note_count[picked_track] > 0)
							{	//If at least one note has been added already
								for(k = note_count[picked_track] - 1; k >= first_note; k--)
								{	//Traverse the note list in reverse to find the appropriate note to modify
									if((eof_get_note_pos(sp, picked_track, k) == event_realtime) && (eof_get_note_type(sp, picked_track, k) == diff))
									{
										match = 1;
										break;	//End the search, as a match was found
									}
								}
							}
							if(!match)
							{	//If the note doesn't match the same time and difficulty as an existing note, this MIDI event represents a new note, initialize it now and increment the note count
								notenum = note_count[picked_track];
								eof_set_note_note(sp, picked_track, notenum, lane_chart[lane]);
								eof_set_note_pos(sp, picked_track, notenum, event_realtime);
								eof_set_note_length(sp, picked_track, notenum, 0);	//The length will be kept at 0 until the end of the note is found
								eof_set_note_flags(sp, picked_track, notenum, 0);	//Clear the flag here so that the flag can be set if it has a special status
								eof_set_note_type(sp, picked_track, notenum, diff);	//Apply the determined difficulty
								if(chordname && (chordname[0] != '\0'))
								{	//If there is a chord name stored for this difficulty
									eof_set_note_name(sp, picked_track, notenum, chordname);	//Populate the note name with whatever name was read last
									chordname[0] = '\0';										//Empty the chord name array for this difficulty
								}
								else
								{	//Otherwise ensure the note has an empty name string
									eof_set_note_name(sp, picked_track, notenum, "");
								}
								note_count[picked_track]++;
#ifdef EOF_DEBUG
								(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tInitializing note #%lu (Diff=%d):  Mask=%u, Pos=%lu, Length=%d", notenum, eof_get_note_type(sp, picked_track, notenum), lane_chart[lane], event_realtime, 0);
								eof_log(eof_log_string, 1);
#endif
							}
							else
							{	//Otherwise just modify the existing note by adding a gem
								notenum = k;
#ifdef EOF_DEBUG
								(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tModifying note #%lu (Diff=%d, Pos=%lu) from mask %lu to %lu", notenum, eof_get_note_type(sp, picked_track, notenum), event_realtime, eof_get_note_note(sp, picked_track, notenum), eof_get_note_note(sp, picked_track, notenum) | lane_chart[lane]);
								eof_log(eof_log_string, 1);
#endif
								eof_set_note_note(sp, picked_track, notenum, eof_get_note_note(sp, picked_track, notenum) | lane_chart[lane]);
							}
							if(eof_import_events[i]->event[j]->d2 < 100)
							{	//Some other editors may have an invalid velocity defined
#ifdef EOF_DEBUG
								(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tModifying note #%lu (Diff=%d, Pos=%lu) from velocity %d to 100", notenum, eof_get_note_type(sp, picked_track, notenum), event_realtime, eof_import_events[i]->event[j]->d2);
								eof_log(eof_log_string, 1);
#endif
								eof_import_events[i]->event[j]->d2 = 100;	//In which case, revert the velocity to the lowest usable value
							}
							sp->pro_guitar_track[tracknum]->note[notenum]->frets[lane] = eof_import_events[i]->event[j]->d2 - 100;	//Velocity (100 + X) represents fret # X
							if(sp->pro_guitar_track[tracknum]->note[notenum]->frets[lane] > sp->pro_guitar_track[tracknum]->numfrets)
							{	//If this fret value is higher than this track's recorded maximum
								if(!fretwarning)
								{	//If the user hasn't been warned about this problem for this track
									(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "Warning:  At least one note (at %lu ms) in %s breaks the track's fret limit of %u.  This can cause Rock Band to crash.", event_realtime, eof_midi_tracks[picked_track].name, sp->pro_guitar_track[tracknum]->numfrets);
									eof_log(eof_log_string, 1);
									allegro_message("Warning:  At least one note (at %lu ms) in %s breaks the track's fret limit of %u.\nThis can cause Rock Band to crash.", event_realtime, eof_midi_tracks[picked_track].name, sp->pro_guitar_track[tracknum]->numfrets);
									fretwarning = 1;
								}
								sp->pro_guitar_track[tracknum]->numfrets = sp->pro_guitar_track[tracknum]->note[notenum]->frets[lane];	//Increase the maximum to reflect this fret value
							}
							if(eof_import_events[i]->event[j]->channel == 1)
							{	//If this note was sent over channel 1, it is a ghost note
								sp->pro_guitar_track[tracknum]->note[notenum]->ghost |= lane_chart[lane];	//Set the ghost flag for this gem's string
							}
							else if(eof_import_events[i]->event[j]->channel == 2)
							{	//If this note was sent over channel 2, it is a bend
								sp->pro_guitar_track[tracknum]->note[notenum]->flags |= EOF_PRO_GUITAR_NOTE_FLAG_BEND;	//Set the bend flag
							}
							else if(eof_import_events[i]->event[j]->channel == 3)
							{	//If this note was sent over channel 3, the gem is muted
								sp->pro_guitar_track[tracknum]->note[notenum]->frets[lane] |= 0x80;	//Set the MSB to indicate the string is muted
							}
							else if(eof_import_events[i]->event[j]->channel == 4)
							{	//If this note was sent over channel 4, it is tapped
								sp->pro_guitar_track[tracknum]->note[notenum]->flags |= EOF_PRO_GUITAR_NOTE_FLAG_TAP;	//Set the tap flag
							}
							else if(eof_import_events[i]->event[j]->channel == 5)
							{	//If this note was sent over channel 5, it is a harmonic
								sp->pro_guitar_track[tracknum]->note[notenum]->flags |= EOF_PRO_GUITAR_NOTE_FLAG_HARMONIC;	//Set the harmonic flag
							}
							else if(eof_import_events[i]->event[j]->channel == 6)
							{	//If this note was sent over channel 6, it is a pinch harmonic
								sp->pro_guitar_track[tracknum]->note[notenum]->flags |= EOF_PRO_GUITAR_NOTE_FLAG_HARMONIC;	//Set the harmonic flag
							}
///KEEP FOR DEBUGGING
/*							else if(eof_import_events[i]->event[j]->channel == 13)
							{
								sp->pro_guitar_track[tracknum]->note[notenum]->flags |= EOF_NOTE_FLAG_HIGHLIGHT;	//Set the highlight flag
							}
*/
						}//If a note difficulty was identified above
					}//Note on event

					/* note off so get length of note */
					else if(eof_import_events[i]->event[j]->type == 0x80)
					{	//Note off event
#ifdef EOF_DEBUG
						(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tNote off:  %d (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->d1, eof_import_events[i]->event[j]->pos, event_realtime);
						eof_log(eof_log_string, 1);
#endif
						/* detect forced HOPO */
						hopodiff = -1;
						if(eof_import_events[i]->event[j]->d1 == 24 + 6)
						{
							hopodiff = 0;
						}
						else if(eof_import_events[i]->event[j]->d1 == 48 + 6)
						{
							hopodiff = 1;
						}
						else if(eof_import_events[i]->event[j]->d1 == 72 + 6)
						{
							hopodiff = 2;
						}
						else if(eof_import_events[i]->event[j]->d1 == 96 + 6)
						{
							hopodiff = 3;
						}
						if(hopodiff >= 0)
						{
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{	//Check (in reverse) for each note that has been imported
								if((eof_get_note_type(sp, picked_track, k) == hopodiff) && (eof_get_note_pos(sp, picked_track, k) >= hopopos[hopodiff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
								{	//If the note is in the same difficulty as the HOPO phrase, and its timestamp falls within the Ho/Po marker
									eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_HO);	//RB3 marks forced hammer ons the same as forced pull offs
								}
							}
						}/* detect forced HOPO */

						/* detect slide markers */
						slidediff = -1;	//Don't process a slide marker unless one was found
						if(eof_import_events[i]->event[j]->d1 == 24 + 7)
						{
							slidediff = 0;
						}
						else if(eof_import_events[i]->event[j]->d1 == 48 + 7)
						{
							slidediff = 1;
						}
						else if(eof_import_events[i]->event[j]->d1 == 72 + 7)
						{
							slidediff = 2;
						}
						else if(eof_import_events[i]->event[j]->d1 == 96 + 7)
						{
							slidediff = 3;
						}
						if(slidediff >= 0)
						{	//If a slide marker was found
							unsigned long *slideptr;	//This will point to either the up or down slide position array as appropriate, so the velocity can be checked just once
							unsigned long slideflag;	//Cache the slide flag to avoid having to do redundant checks

							if(slidevelocity[slidediff] >= 108)
							{	//For slide markers, velocity 108 or higher represents a down slide.  Read the slide velocity that was cached at the start of the slide
								slideptr = slidedownpos;	//The slide marker is a down slide
								slideflag = EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN;
								if(eof_import_events[i]->event[j]->channel == 11)
								{	//Unless the channel for the marker is 11, in which case it's reversed
									slideflag |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_REVERSE;
								}
							}
							else
							{	//Other velocities represent an up slide
								slideptr = slideuppos;		//The slide marker is an up slide
								slideflag = EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP;
								if(eof_import_events[i]->event[j]->channel == 11)
								{	//Unless the channel for the marker is 11, in which case it's reversed
									slideflag |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_REVERSE;
								}
							}
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{	//Check for each note that has been imported
								if((eof_get_note_type(sp, picked_track, k) == slidediff) && (eof_get_note_pos(sp, picked_track, k) >= slideptr[slidediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
								{	//If the note is in the same difficulty as the slide marker, and its timestamp falls between the start and stop of the marker
									eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | slideflag);	//Set the appropriate slide flag for the note
								}
							}
						}

						/* detect arpeggio markers */
						arpegdiff = -1;	//Don't process an arpeggio marker unless one was found
						if(eof_import_events[i]->event[j]->d1 == 24 + 8)
						{
							arpegdiff = 0;
						}
						else if(eof_import_events[i]->event[j]->d1 == 48 + 8)
						{
							arpegdiff = 1;
						}
						else if(eof_import_events[i]->event[j]->d1 == 72 + 8)
						{
							arpegdiff = 2;
						}
						else if(eof_import_events[i]->event[j]->d1 == 96 + 8)
						{
							arpegdiff = 3;
						}
						if((arpegdiff >= 0) && (eof_get_num_arpeggios(sp, picked_track) < EOF_MAX_PHRASES))
						{	//If an arpeggio marker was found, and the max number of arpeggio phrases haven't already been added
							phraseptr = eof_get_arpeggio(sp, picked_track, eof_get_num_arpeggios(sp, picked_track));
							phraseptr->start_pos = arpegpos[arpegdiff];
							phraseptr->end_pos = event_realtime;
							phraseptr->difficulty = arpegdiff;	//Set the difficulty for this arpeggio
							eof_set_num_arpeggios(sp, picked_track, eof_get_num_arpeggios(sp, picked_track) + 1);
						}

						/* detect strum direction markers */
						strumdiff = -1;	//Don't process a strum marker unless one was found
						if((eof_import_events[i]->event[j]->channel >= 13) && (eof_import_events[i]->event[j]->channel <= 15))
						{	//Lane (1 + 9), channel 13, 14 and 15 are used in up, mid and down strum markers
							if(eof_import_events[i]->event[j]->d1 == 24 + 9)
							{
								strumdiff = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 48 + 9)
							{
								strumdiff = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 72 + 9)
							{
								strumdiff = 2;
							}
							else if(eof_import_events[i]->event[j]->d1 == 96 + 9)
							{
								strumdiff = 3;
							}
						}
						if(strumdiff >= 0)
						{	//If a strum marker was found
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{	//Check for each note that has been imported
								if((eof_get_note_type(sp, picked_track, k) == strumdiff) && (eof_get_note_pos(sp, picked_track, k) >= strumpos[strumdiff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
								{	//If the note is in the same difficulty as the strum direction marker, and its timestamp falls between the start and stop of the marker
									if(strumtype[strumdiff] == 0)
									{	//This was an up strum marker
										eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_UP_STRUM);
									}
									else if(strumtype[strumdiff] == 1)
									{	//This was a mid strum marker
										eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_MID_STRUM);
									}
									else
									{	//This was a down strum marker
										eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_DOWN_STRUM);
									}
								}
							}
						}

						if((eof_import_events[i]->event[j]->d1 == 115) && (eof_get_num_solos(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a solo phrase
							phraseptr = eof_get_solo(sp, picked_track, eof_get_num_solos(sp, picked_track));
							phraseptr->end_pos = event_realtime;
							eof_set_num_solos(sp, picked_track, eof_get_num_solos(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 116) && (eof_get_num_star_power_paths(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a star power phrase
							phraseptr = eof_get_star_power_path(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track));
							phraseptr->end_pos = event_realtime;
							eof_set_num_star_power_paths(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 126) && (eof_get_num_tremolos(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a tremolo phrase
							phraseptr = eof_get_tremolo(sp, picked_track, eof_get_num_tremolos(sp, picked_track));
							phraseptr->end_pos = event_realtime;
							eof_set_num_tremolos(sp, picked_track, eof_get_num_tremolos(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 127) && (eof_get_num_trills(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a trill phrase
							phraseptr = eof_get_trill(sp, picked_track, eof_get_num_trills(sp, picked_track));
							phraseptr->end_pos = event_realtime;
							eof_set_num_trills(sp, picked_track, eof_get_num_trills(sp, picked_track) + 1);
						}

						if((note_count[picked_track] > 0) && (diff != -1))
						{	//If there's at least one note on found for this track, and this note off event has a defined difficulty
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{	//Check for each note that has been imported, in reverse order
								if((eof_get_note_type(sp, picked_track, k) == diff) && (eof_get_note_note(sp, picked_track, k) & lane_chart[lane]))
								{	//If the note is in the same difficulty as this note off event and it contains one of the same gems
//									allegro_message("break %d, %d, %d", k, sp->legacy_track[picked_track]->note[k]->note, sp->legacy_track[picked_track]->note[note_count[picked_track]]->note);	//Debug
#ifdef EOF_DEBUG
									(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\t\tModifying note #%ld (Diff=%d, Pos=%lu, Mask=%lu) from length %ld to %ld", k, eof_get_note_type(sp, picked_track, k), eof_get_note_pos(sp, picked_track, k), eof_get_note_note(sp, picked_track, k), eof_get_note_length(sp, picked_track, k), event_realtime - eof_get_note_pos(sp, picked_track, k));
									eof_log(eof_log_string, 1);
#endif
									eof_set_note_length(sp, picked_track, k, event_realtime - eof_get_note_pos(sp, picked_track, k));
									if(eof_get_note_length(sp, picked_track, k ) <= 0)
									{	//If the note somehow received a zero or negative length
										eof_set_note_length(sp, picked_track, k, 1);
									}
									break;
								}
							}
						}
					}//Note off event

					else if((eof_import_events[i]->event[j]->type == 0x01) && (ustrstr(eof_import_events[i]->event[j]->text, "[chrd") == eof_import_events[i]->event[j]->text))
					{	//If this is a text event that begins with RB3's note name notation "[chrd"
						char *ptr = NULL;	//This will point to the destination name array
						char *ptr2 = NULL;	//This will be used to index into the text event
						unsigned long index = 0;

						if(eof_import_events[i]->event[j]->text[5] != '\0')
						{	//If the string is long enough to have a difficulty ID
							switch(eof_import_events[i]->event[j]->text[5])
							{
								case '0':	//Easy
									ptr = chord0name;
								break;
								case '1':	//Medium
									ptr = chord1name;
								break;
								case '2':	//Hard
									ptr = chord2name;
								break;
								case '3':	//Expert
									ptr = chord3name;
								break;
							}
							if((ptr != NULL) && (eof_import_events[i]->event[j]->text[6] == ' '))
							{	//If a valid difficulty ID was parsed and the following character is a space
								ptr2 = ustrchr(eof_import_events[i]->event[j]->text, ' ');	//Get the address of the string's first space character
								if(ptr2 != NULL)
								{
									ptr2++;	//Advance past the first space character
									while((ptr2[0] != '\0') && (ptr2[0] != ']'))
									{	//Until the end of string or end of chord name are reached
										if(index >= 99)
											break;	//Prevent a buffer overflow
										ptr[index++] = ptr2[0];	//Store this character into the appropriate string
										ptr2++;	//Advance to the next character
									}
									ptr[index]='\0';	//Terminate the string
								}
							}
						}
					}//If this is a text event that begins with RB3's note name notation "[chrd"

					/* Sysex event (custom phrase markers for Phase Shift) */
					if((eof_import_events[i]->event[j]->type == 0xF0) && eof_import_events[i]->event[j]->dp)
					{	//Sysex event
						if((eof_import_events[i]->event[j]->d1 == 8) && (!strncmp(eof_import_events[i]->event[j]->dp, "PS", 3)))
						{	//If this is a custom Sysex Phase Shift marker (8 bytes long, beginning with the NULL terminated string "PS")
							switch(eof_import_events[i]->event[j]->dp[3])
							{	//Check the value of the message ID
								case 0:	//Phrase marker
									phrasediff = eof_import_events[i]->event[j]->dp[4];	//Store the difficulty ID
									switch(eof_import_events[i]->event[j]->dp[5])
									{	//Check the value of the phrase ID
										case 2:	//Pro guitar slide up
#ifdef EOF_DEBUG
											(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Pro guitar slide up (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
											eof_log(eof_log_string, 1);
#endif
											if(eof_import_events[i]->event[j]->dp[6] == 1)
											{	//Start of pro guitar slide up phrase
												slideuppos[phrasediff] = event_realtime;
											}
											else if(eof_import_events[i]->event[j]->dp[6] == 0)
											{	//End of pro guitar slide up phrase
												for(k = note_count[picked_track] - 1; k >= first_note; k--)
												{	//Check for each note that has been imported
													if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= slideuppos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
													{	//If the note is in the same difficulty as the pro guitar slide up phrase, and its timestamp falls between the phrase on and phrase off marker
														eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP);	//Set the slide up flag
													}
												}
											}
										break;
										case 3:	//Pro guitar slide down
#ifdef EOF_DEBUG
											(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Pro guitar slide down (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
											eof_log(eof_log_string, 1);
#endif
											if(eof_import_events[i]->event[j]->dp[6] == 1)
											{	//Start of pro guitar slide down phrase
												slidedownpos[phrasediff] = event_realtime;
											}
											else if(eof_import_events[i]->event[j]->dp[6] == 0)
											{	//End of pro guitar slide down phrase
												for(k = note_count[picked_track] - 1; k >= first_note; k--)
												{	//Check for each note that has been imported
													if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= slidedownpos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
													{	//If the note is in the same difficulty as the pro guitar slide down phrase, and its timestamp falls between the phrase on and phrase off marker
														eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN);	//Set the slide up flag
													}
												}
											}
										break;
										case 9:	//Pro guitar palm mute
#ifdef EOF_DEBUG
											(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Pro guitar palm mute (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
											eof_log(eof_log_string, 1);
#endif
											if(eof_import_events[i]->event[j]->dp[6] == 1)
											{	//Start of pro guitar palm mute phrase
												palmmutepos[phrasediff] = event_realtime;
											}
											else if(eof_import_events[i]->event[j]->dp[6] == 0)
											{	//End of pro guitar palm mute phrase
												for(k = note_count[picked_track] - 1; k >= first_note; k--)
												{	//Check for each note that has been imported
													if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= palmmutepos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
													{	//If the note is in the same difficulty as the pro guitar palm mute phrase, and its timestamp falls between the phrase on and phrase off marker
														eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_PALM_MUTE);	//Set the palm mute flag
													}
												}
											}
										break;
										case 10:	//Pro guitar vibrato
#ifdef EOF_DEBUG
											(void) snprintf(eof_log_string, sizeof(eof_log_string) - 1, "\t\t\tSysex marker:  Pro guitar vibrato (deltapos=%lu, pos=%lu)", eof_import_events[i]->event[j]->pos, event_realtime);
											eof_log(eof_log_string, 1);
#endif
											if(eof_import_events[i]->event[j]->dp[6] == 1)
											{	//Start of pro guitar vibrato phrase
												vibratopos[phrasediff] = event_realtime;
											}
											else if(eof_import_events[i]->event[j]->dp[6] == 0)
											{	//End of pro guitar vibrato phrase
												for(k = note_count[picked_track] - 1; k >= first_note; k--)
												{	//Check for each note that has been imported
													if((eof_get_note_type(sp, picked_track, k) == phrasediff) && (eof_get_note_pos(sp, picked_track, k) >= vibratopos[phrasediff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
													{	//If the note is in the same difficulty as the pro guitar vibrato phrase, and its timestamp falls between the phrase on and phrase off marker
														eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_VIBRATO);	//Set the vibrato flag
													}
												}
											}
										break;
									}
								break;
							}
						}//If this is a custom Sysex Phase Shift marker (8 bytes long, beginning with the NULL terminated string "PS")
						free(eof_import_events[i]->event[j]->dp);	//The the memory allocated to store this Sysex message's data
						eof_import_events[i]->event[j]->dp = NULL;
					}//Sysex event
				}//If parsing a pro guitar track
				pticker++;
			}//For each event in this track

			eof_track_resize(sp, picked_track, note_count[picked_track]);
			if(eof_import_events[i]->game == 0)
			{	//If this is not a Power Gig formatted MIDI, only allow one MIDI track to import into each track in the project
				if(eof_get_track_size(sp, picked_track) > 0)
				{	//If at least one note was imported
					eof_track_find_crazy_notes(sp, picked_track);
					used_track[picked_track] = 1;	//Note that this track has been imported, duplicate instances of the track will be ignored
				}
			}
		}//If this is a valid track to process
	}//For each imported track

//#ifdef EOF_DEBUG_MIDI_IMPORT
eof_log("\tThird pass complete", 1);
//#endif

	/* delete empty lyric lines */
	for(i = sp->vocal_track[0]->lines; i > 0; i--)
	{
		lc = 0;
		for(j = 0; j < sp->vocal_track[0]->lyrics; j++)
		{
			if((sp->vocal_track[0]->lyric[j]->pos >= sp->vocal_track[0]->line[i-1].start_pos) && (sp->vocal_track[0]->lyric[j]->pos <= sp->vocal_track[0]->line[i-1].end_pos))
			{
				lc++;
			}
		}
		if(lc <= 0)
		{
			eof_vocal_track_delete_line(sp->vocal_track[0], i-1);
		}
	}

//Perform an additional check to ensure pro drum notations are correct
	if(prodrums)
	{
		tracknum = sp->track[EOF_TRACK_DRUM]->tracknum;
		eof_legacy_track_sort_notes(sp->legacy_track[tracknum]);	//Ensure this track is sorted
		for(k = 0; k < sp->legacy_track[tracknum]->notes; k++)
		{	//For each note in the drum track
			if(sp->legacy_track[tracknum]->note[k]->type != EOF_NOTE_SPECIAL)
			{	//Only process non BRE notes
				if((sp->legacy_track[tracknum]->note[k]->note & 4) && ((sp->legacy_track[tracknum]->note[k]->flags & EOF_NOTE_FLAG_Y_CYMBAL) == 0))
				{	//If this note has a yellow gem with the cymbal marker cleared
					eof_set_flags_at_legacy_note_pos(sp->legacy_track[tracknum],k,EOF_NOTE_FLAG_Y_CYMBAL,0,1);	//Ensure all drum notes at this position have the flag cleared
				}
				if((sp->legacy_track[tracknum]->note[k]->note & 8) && ((sp->legacy_track[tracknum]->note[k]->flags & EOF_NOTE_FLAG_B_CYMBAL) == 0))
				{	//If this note has a blue gem with the cymbal marker cleared
					eof_set_flags_at_legacy_note_pos(sp->legacy_track[tracknum],k,EOF_NOTE_FLAG_B_CYMBAL,0,1);	//Ensure all drum notes at this position have the flag cleared
				}
				if((sp->legacy_track[tracknum]->note[k]->note & 16) && ((sp->legacy_track[tracknum]->note[k]->flags & EOF_NOTE_FLAG_G_CYMBAL) == 0))
				{	//If this note has a green gem with the cymbal marker cleared
					eof_set_flags_at_legacy_note_pos(sp->legacy_track[tracknum],k,EOF_NOTE_FLAG_G_CYMBAL,0,1);	//Ensure all drum notes at this position have the flag cleared
				}
			}
		}
	}

//Ensure that any notes imported from PART KEYS are marked as "crazy"
	tracknum = sp->track[EOF_TRACK_KEYS]->tracknum;
	for(k = 0; k < sp->legacy_track[tracknum]->notes; k++)
	{	//For each note in the keys track
		sp->legacy_track[tracknum]->note[k]->flags |= EOF_NOTE_FLAG_CRAZY;	//Set the crazy status flag
	}

//Check for forced HOPO on lane 1 in PART BASS and prompt for whether to treat as open bass strumming (this is how GH MIDIs mark open bass)
	tracknum = sp->track[EOF_TRACK_BASS]->tracknum;
	for(i = 0; i < sp->legacy_track[tracknum]->notes; i++)
	{	//For each note in the bass track
		if(!eof_ini_sysex_open_bass_present && (sp->legacy_track[tracknum]->note[i]->note & 1) && (sp->legacy_track[tracknum]->note[i]->flags & EOF_NOTE_FLAG_F_HOPO))
		{	//If Sysex open strum notation isn't present, and this note has a gem in lane one that is forced as a HOPO, prompt the user how to handle them
			eof_cursor_visible = 0;
			eof_pen_visible = 0;
			eof_show_mouse(screen);
			eof_clear_input();
			key[KEY_Y] = 0;
			key[KEY_N] = 0;
			if(alert(NULL, "Import lane 1 forced HOPO bass notes as open strums (ie. this is a Guitar Hero chart)?", NULL, "&Yes", "&No", 'y', 'n') == 1)
			{	//If the user opts to import lane 1 HOPO bass notes as open strums
				sp->legacy_track[tracknum]->numlanes = 6;						//Set this track to have 6 lanes instead of 5
				sp->track[EOF_TRACK_BASS]->flags |= EOF_TRACK_FLAG_SIX_LANES;	//Set this flag
				for(k = 0; k < sp->legacy_track[tracknum]->notes; k++)
				{	//For each note in the bass track
					if((sp->legacy_track[tracknum]->note[k]->note & 1) && (sp->legacy_track[tracknum]->note[k]->flags & EOF_NOTE_FLAG_F_HOPO))
					{	//If this note has a gem in lane one and is forced as a HOPO, convert it to open bass
						sp->legacy_track[tracknum]->note[k]->note &= ~(1);	//Clear lane 1
						sp->legacy_track[tracknum]->note[k]->note |= 32;		//Set lane 6
						sp->legacy_track[tracknum]->note[k]->flags &= ~(EOF_NOTE_FLAG_F_HOPO);	//Clear the forced HOPO on flag
					}
				}
			}
			else
			{	//Otherwise ensure that no open bass notation is present in the bass track
				for(k = 0; k < sp->legacy_track[tracknum]->notes; k++)
				{	//For each note in the bass track
					if(sp->legacy_track[tracknum]->note[k]->note & 32)
					{	//If this note has a gem in lane 6 (used as the HOPO on marker if not as a lane 6 gem)
						sp->legacy_track[tracknum]->note[k]->note &= ~32;	//Clear lane 6
					}
				}
			}
			eof_show_mouse(NULL);
			eof_cursor_visible = 1;
			eof_pen_visible = 1;
			break;
		}
	}

//Check for string muted notes and force them to use the 0xFF (muted) fret value so the pro guitar fixup logic won't remove the string mute status
	for(i = 0; i < sp->pro_guitar_tracks; i++)
	{	//For each pro guitar track
		for(j = 0; j < sp->pro_guitar_track[i]->notes; j++)
		{	//For each note in the track
			if(sp->pro_guitar_track[i]->note[j]->flags & EOF_PRO_GUITAR_NOTE_FLAG_STRING_MUTE)
			{	//If the note imported as being string muted
				for(k = 0, bitmask = 1; k < 6; k++, bitmask<<=1)
				{	//For each of the 6 usable strings
					if(sp->pro_guitar_track[i]->note[j]->note & bitmask)
					{	//If the string is used in this note
						sp->pro_guitar_track[i]->note[j]->frets[k] |= 0x80;	//Set this string to be muted
					}
				}
			}
		}
	}

//Load guitar.ogg automatically if it's present, otherwise prompt user to browse for audio
	(void) replace_filename(eof_song_path, fn, "", 1024);
	(void) append_filename(nfn, eof_song_path, "guitar.ogg", 1024);
	if(!eof_load_ogg(nfn, 1))	//If user does not provide audio, fail over to using silent audio
	{
		eof_destroy_song(sp);
		eof_import_destroy_events_list(eof_import_bpm_events);
		eof_import_destroy_events_list(eof_import_text_events);
		eof_import_destroy_events_list(eof_import_ks_events);
		for(i = 0; i < tracks; i++)
		{
			eof_import_destroy_events_list(eof_import_events[i]);
		}
		destroy_midi(eof_work_midi);
		eof_destroy_tempo_list(anchorlist);
		return NULL;
	}
	eof_song_loaded = 1;
	eof_chart_length = alogg_get_length_msecs_ogg(eof_music_track);

	/* create text events */
	for(i = 0; i < eof_import_text_events->events; i++)
	{
		if(eof_import_text_events->event[i]->type == 0x01)
		{
			tp = eof_ConvertToRealTimeInt(eof_import_text_events->event[i]->pos,anchorlist,eof_import_ts_changes[0],eof_work_midi->divisions,sp->tags->ogg[0].midi_offset);
			b = eof_import_closest_beat(sp, tp);
			if(b >= 0)
			{
//				allegro_message("%s", eof_import_text_events->event[i]->text);	//Debug
				(void) eof_song_add_text_event(sp, b, eof_import_text_events->event[i]->text, 0, 0, 0);
			}
		}
	}

	/* convert solos to star power for GH charts using FoFiX's method */
	for(i = 1; i < sp->tracks; i++)
	{	//For each track
		if(sp->track[i]->track_format == EOF_LEGACY_TRACK_FORMAT)
		{	//Only perform this conversion for legacy tracks
			if(!eof_ini_star_power_tag_present && (eof_get_num_star_power_paths(sp, i) < 2))
			{	//If EOF's star power INI tag wasn't read, and this track has less than two star power sections
				for(j = 0; j < eof_get_num_solos(sp, i); j++)
				{	//For each solo, convert to a star power section
					phraseptr = eof_get_solo(sp, i, j);
					phraseptr2 = eof_get_star_power_path(sp, i, j);
					phraseptr2->start_pos = phraseptr->start_pos;
					phraseptr2->end_pos = phraseptr->end_pos;
				}
				eof_set_num_star_power_paths(sp, i, eof_get_num_star_power_paths(sp, i) + eof_get_num_solos(sp, i));
				eof_set_num_solos(sp, i, 0);
			}
		}
	}

	eof_import_destroy_events_list(eof_import_bpm_events);
	eof_import_destroy_events_list(eof_import_text_events);
	eof_import_destroy_events_list(eof_import_ks_events);
	for(i = 0; i < tracks; i++)
	{
		eof_import_destroy_events_list(eof_import_events[i]);
	}
	for(i = 0; i < tracks; i++)
	{
		eof_destroy_ts_list(eof_import_ts_changes[i]);
	}

	/* free MIDI */
	destroy_midi(eof_work_midi);
	eof_destroy_tempo_list(anchorlist);

	eof_selected_ogg = 0;

	//Check for conflicts between the imported pro guitar/bass track (if any) and their tuning tags
	tracknum = sp->track[EOF_TRACK_PRO_GUITAR]->tracknum;
	if(eof_detect_string_gem_conflicts(sp->pro_guitar_track[tracknum], sp->pro_guitar_track[tracknum]->numstrings))
	{
		allegro_message("Warning:  17 fret pro guitar tuning tag defines too few strings.  Reverting to 6 string standard tuning.");
		sp->pro_guitar_track[tracknum]->numstrings = 6;
		memset(sp->pro_guitar_track[tracknum]->tuning, 0, EOF_TUNING_LENGTH);
	}
	tracknum = sp->track[EOF_TRACK_PRO_GUITAR_22]->tracknum;
	if(eof_detect_string_gem_conflicts(sp->pro_guitar_track[tracknum], sp->pro_guitar_track[tracknum]->numstrings))
	{
		allegro_message("Warning:  22 fret pro guitar tuning tag defines too few strings.  Reverting to 6 string standard tuning.");
		sp->pro_guitar_track[tracknum]->numstrings = 6;
		memset(sp->pro_guitar_track[tracknum]->tuning, 0, EOF_TUNING_LENGTH);
	}
	tracknum = sp->track[EOF_TRACK_PRO_BASS]->tracknum;
	if(eof_detect_string_gem_conflicts(sp->pro_guitar_track[tracknum], sp->pro_guitar_track[tracknum]->numstrings))
	{
		allegro_message("Warning:  17 fret pro bass tuning tag defines too few strings.  Reverting to 6 string standard tuning.");
		sp->pro_guitar_track[tracknum]->numstrings = 6;
		memset(sp->pro_guitar_track[tracknum]->tuning, 0, EOF_TUNING_LENGTH);
	}
	tracknum = sp->track[EOF_TRACK_PRO_BASS_22]->tracknum;
	if(eof_detect_string_gem_conflicts(sp->pro_guitar_track[tracknum], sp->pro_guitar_track[tracknum]->numstrings))
	{
		allegro_message("Warning:  22 fret pro bass tuning tag defines too few strings.  Reverting to 6 string standard tuning.");
		sp->pro_guitar_track[tracknum]->numstrings = 6;
		memset(sp->pro_guitar_track[tracknum]->tuning, 0, EOF_TUNING_LENGTH);
	}

//#ifdef EOF_DEBUG_MIDI_IMPORT
eof_log("\tMIDI import complete", 1);
//#endif

//	eof_log_level |= 2;	//Enable verbose logging
	return sp;
}
