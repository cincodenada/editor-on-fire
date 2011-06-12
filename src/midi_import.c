#include <allegro.h>
#include <assert.h>
#include "main.h"
#include "utility.h"
#include "song.h"
#include "midi_import.h"
#include "ini_import.h"
#include "menu/note.h"
#include "foflc/Lyric_storage.h"

#ifdef USEMEMWATCH
#include "memwatch.h"
#endif

typedef struct
{

	unsigned long pos;
	unsigned char type;
	unsigned char channel;
	int d1, d2, d3, d4;
	unsigned long track;	//The track number this event is from
	char text[EOF_MAX_MIDI_TEXT_SIZE+1];

} EOF_IMPORT_MIDI_EVENT;

typedef struct
{

	EOF_IMPORT_MIDI_EVENT * event[EOF_IMPORT_MAX_EVENTS];
	unsigned long events;
	int type;

} EOF_IMPORT_MIDI_EVENT_LIST;

static MIDI * eof_work_midi = NULL;
static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_events[EOF_MAX_IMPORT_MIDI_TRACKS];
static EOF_MIDI_TS_LIST * eof_import_ts_changes[EOF_MAX_IMPORT_MIDI_TRACKS];
static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_bpm_events;
static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_text_events;
double eof_import_bpm_pos[1024] = {0.0};
int eof_import_bpm_count = 0;

static EOF_IMPORT_MIDI_EVENT_LIST * eof_import_create_events_list(void)
{
	eof_log("eof_import_create_events_list() entered", 1);

	EOF_IMPORT_MIDI_EVENT_LIST * lp;
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
	eof_log("eof_import_destroy_events_list() entered", 1);

	int i;

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
static unsigned long eof_parse_var_len(unsigned char * data, unsigned long pos, unsigned long * bytes_used)
{
//	eof_log("eof_parse_var_len() entered");

	if(!data || !bytes_used)
	{
		return 0;
	}

	int cpos = pos;
	unsigned long val = *(&data[cpos]) & 0x7F;

	while(data[cpos] & 0x80)
	{
		cpos++;
		(*bytes_used)++;
//		(*data)++;
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
	EOF_IMPORT_MIDI_EVENT *ptr = malloc(sizeof(EOF_IMPORT_MIDI_EVENT));

	if(events && (events->events < EOF_IMPORT_MAX_EVENTS))
	{
		if(ptr != NULL)
		{
			events->event[events->events] = ptr;
			events->event[events->events]->pos = pos;
			events->event[events->events]->type = event & 0xF0;		//The event type
			events->event[events->events]->channel = event & 0xF;	//The channel number
			events->event[events->events]->d1 = d1;
			events->event[events->events]->d2 = d2;
			events->event[events->events]->track = track;	//Store the event's track number

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
			memcpy(events->event[events->events]->text, text, size);
			events->event[events->events]->text[size] = '\0';
			events->event[events->events]->track = track;	//Store the event's track number

			events->events++;
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
	unsigned int den=4;			//Stores the denominator of the closest TS change before the specified delta time (defaults to 4 as per MIDI specification)
	unsigned long ctr=0;

//Find the last time signature change at or before the target delta time
	if((tslist != NULL) && (tslist->changes > 0))
	{	//If there's at least one TS change
		for(ctr=0;ctr < tslist->changes;ctr++)
		{
			if(absolutedelta >= tslist->change[ctr]->pos)
			{	//If the TS change is at or before the target delta time
				den = tslist->change[ctr]->den;		//Store this time signature's denominator for use in the conversion
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
//The updated theoretical conversion formula that takes the time signature into account is: deltas / (time division) * (60000.0 / (BPM * (TS denominator) / 4))
//This commented out line was causing timing errors when importing a RB MIDI that used #/8 TS
//	time+=(double)reldelta / (double)timedivision * ((double)60000.0 / (temp->BPM * den / 4.0));
	time+=(double)reldelta / (double)timedivision * ((double)60000.0 / (temp->BPM));

//The old conversion formula that doesn't take time signature into account
//	temptimer+=(double)tempdelta / (double)timedivision * ((double)60000.0 / tempBPM);
	return time+offset;
}

inline unsigned long eof_ConvertToRealTimeInt(unsigned long absolutedelta,struct Tempo_change *anchorlist,EOF_MIDI_TS_LIST *tslist,unsigned long timedivision,unsigned long offset)
{
	return eof_ConvertToRealTime(absolutedelta,anchorlist,tslist,timedivision,offset) + 0.5;
}

//#define EOF_DEBUG_MIDI_IMPORT

EOF_SONG * eof_import_midi(const char * fn)
{
	eof_log("eof_import_midi() entered", 1);

	EOF_SONG * sp = NULL;;
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
//	EOF_PHRASE_SECTION *starpowerptr = NULL;
	unsigned long bitmask;
	char chord0name[100] = "", chord1name[100] = "", chord2name[100] = "", chord3name[100] = "", *chordname = NULL;	//Used for chord name import

//#ifdef EOF_DEBUG_MIDI_IMPORT
	char debugstring[100];
//#endif

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
	eof_log_level &= ~2;	//Disable verbose logging

	/* backup "notes.mid" if it exists in the folder with the imported MIDI
	   as it will be overwritten upon save */
	replace_filename(eof_temp_filename, fn, "notes.mid", 1024);
	if(exists(eof_temp_filename))
	{
		/* do not overwrite an existing backup, this prevents the original backed up MIDI from
		   being overwritten if the user imports the MIDI again */
		replace_filename(backup_filename, fn, "notes.mid.backup", 1024);
		if(!exists(backup_filename))
		{
			eof_copy_file(eof_temp_filename, backup_filename);
		}
	}

	sp = eof_create_song_populated();
	if(!sp)
	{
		destroy_midi(eof_work_midi);
		return NULL;
	}

	/* read INI file */
	replace_filename(backup_filename, fn, "song.ini", 1024);
	eof_import_ini(sp, backup_filename);


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

#ifdef EOF_DEBUG_MIDI_IMPORT
	eof_log("\tFirst pass, building events lists", 1);
#endif

	eof_import_bpm_events = eof_import_create_events_list();
	if(!eof_import_bpm_events)
	{
		destroy_midi(eof_work_midi);
		return NULL;
	}
	eof_import_text_events = eof_import_create_events_list();
	if(!eof_import_text_events)
	{
		eof_import_destroy_events_list(eof_import_bpm_events);
		destroy_midi(eof_work_midi);
		return NULL;
	}
	for(i = 0; i < tracks; i++)
	{	//For each imported track
//#ifdef EOF_DEBUG_MIDI_IMPORT
		sprintf(debugstring, "\t\tParsing track #%lu of %d",i,tracks);
		eof_log(debugstring, 1);
//#endif
		last_event = 0;	//Running status resets at beginning of each track
		current_event = current_event_hi = 0;
		eof_import_events[i] = eof_import_create_events_list();
		eof_import_ts_changes[i] = eof_create_ts_list();
		if(!eof_import_events[i] || !eof_import_ts_changes[i])
		{
			eof_import_destroy_events_list(eof_import_bpm_events);
			eof_import_destroy_events_list(eof_import_text_events);
			destroy_midi(eof_work_midi);
			return NULL;
		}
		track_pos = 0;
		absolute_pos = 0;
		while(track_pos < eof_work_midi->track[track[i]].len)
		{	//While the byte index of this MIDI track hasn't reached the end of the track data
			/* read delta */
#ifdef EOF_DEBUG_MIDI_IMPORT
		sprintf(debugstring, "\t\t\tParsing byte #%d of %d",track_pos,eof_work_midi->track[track[i]].len);
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
//					eof_midi_import_add_event(eof_import_events[i], absolute_pos, 0x80, d1, d2, i);
					eof_midi_import_add_event(eof_import_events[i], absolute_pos, current_event, d1, d2, i);
					break;
				}

				/* note on */
				case 0x90:
				{
					if(d2 <= 0)
					{	//Any Note On event with a velocity of 0 is to be treated as a Note Off event
//						eof_midi_import_add_event(eof_import_events[i], absolute_pos, 0x80, d1, d2, i);
						eof_midi_import_add_event(eof_import_events[i], absolute_pos, 0x80 + (current_event & 0xF), d1, d2, i);	//Retain the original channel number for this event
					}
					else
					{
//						eof_midi_import_add_event(eof_import_events[i], absolute_pos, 0x90, d1, d2, i);
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
									if(j < EOF_MAX_MIDI_TEXT_SIZE)			//If this wouldn't overflow the buffer
										text[j] = eof_work_midi->track[track[i]].data[track_pos + 1 + j];
									else
										break;
								}
								if(j >= EOF_MAX_MIDI_TEXT_SIZE)	//If the string needs to be truncated
									text[EOF_MAX_MIDI_TEXT_SIZE] = '\0';
								else
									text[j] = '\0';	//Truncate the string normally
								if(ustrstr(text, "[chrd") != NULL)
								{	//If this is a chord name text event
									eof_midi_import_add_text_event(eof_import_events[i], absolute_pos, 0x01, text, eof_work_midi->track[track[i]].data[track_pos], i);
								}
								else
								{	//Otherwise add it to the regular text event list
									eof_midi_import_add_text_event(eof_import_text_events, absolute_pos, 0x01, text, eof_work_midi->track[track[i]].data[track_pos], i);
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
									ustrcpy(text, "PART DRUMS");	//Correct the name
								}

								/* detect what kind of track this is */
								eof_import_events[i]->type = 0;
								for(j = 1; j < EOF_TRACKS_MAX + 1; j++)
								{	//Compare the track name against the tracks in eof_midi_tracks[]
									if(!ustricmp(text, eof_midi_tracks[j].name))
									{	//If this track name matches an expected name
										eof_import_events[i]->type = eof_midi_tracks[j].track_type;
										if(eof_midi_tracks[j].track_type == EOF_TRACK_GUITAR)
										{
											rbg = 1;
										}
									}
								}
								if((eof_import_events[i]->type == 0) && ustrstr(text,"PART"))
								{
									allegro_message("Unidentified track \"%s\"",text);
									eof_import_events[i]->type = -1;	//Flag this as being a track that gets skipped
								}
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
								track_pos += 1;
								d1 = (eof_work_midi->track[track[i]].data[track_pos]);
								track_pos++;
								d2 = (eof_work_midi->track[track[i]].data[track_pos]);
								track_pos++;
								d3 = (eof_work_midi->track[track[i]].data[track_pos]);
								d4 = (d1 << 16) | (d2 << 8) | (d3);

								if((eof_import_bpm_events->events <= 0) && (absolute_pos > sp->tags->ogg[0].midi_offset))
								{	//If the first explicit Set Tempo event is not at the beginning of the track
//									eof_midi_import_add_event(eof_import_bpm_events, sp->tags->ogg[0].midi_offset, 0x51, 500000, 0);	//Insert the default tempo of 120BPM at the beginning of the tempo list
									eof_midi_import_add_event(eof_import_bpm_events, 0, 0x51, 500000, 0, i);	//Insert the default tempo of 120BPM at the beginning of the tempo list
								}
								eof_midi_import_add_event(eof_import_bpm_events, absolute_pos, 0x51, d4, 0, i);
								track_pos++;
								break;
							}

							/* time signature */
							case 0x58:
							{
								unsigned long ctr,realden;

//								track_pos += 5;
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
								track_pos += 3;
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

struct Tempo_change *anchorlist=NULL;	//Anchor linked list

	/* second pass, create tempo map */
	unsigned long deltapos = 0;		//Stores the ongoing delta time
	double deltafpos = 0.0;			//Stores the ongoing delta time (with double floating precision)
	double realtimepos = 0.0;		//Stores the ongoing real time (start at 0s, displace by the MIDI delay where appropriate)
	unsigned lastnum=4,lastden=4;	//Stores the last applied time signature details (default is 4/4)
	unsigned curnum=4,curden=4;		//Stores the current time signature details (default is 4/4)
	unsigned long lastppqn=0;		//Stores the last applied tempo information
	unsigned long curppqn=500000;	//Stores the current tempo in PPQN (default is 120BPM)

	unsigned long ctr,ctr2,nextanchor;
	char midbeatchange;
	double beatlength, beatreallength;;

#ifdef EOF_DEBUG_MIDI_IMPORT
char debugtext[400];
allegro_message("Pass two, adding beats.  last_delta_time = %lu",last_delta_time);
	eof_log("Pass two, adding beats", 1);
#endif

	while(deltapos <= last_delta_time)
	{//Add new beats until enough have been added to encompass the last MIDI event

#ifdef EOF_DEBUG_MIDI_IMPORT
sprintf(debugtext,"Start delta %lu / %lu: Adding beat",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		if(eof_song_add_beat(sp) == NULL)	//Add a new beat
		{					//Or return failure if that doesn't succeed

allegro_message("Fail point 1");

			eof_import_destroy_events_list(eof_import_bpm_events);
			eof_import_destroy_events_list(eof_import_text_events);
			destroy_midi(eof_work_midi);
			eof_destroy_tempo_list(anchorlist);
			return NULL;
		}

	//Find the relevant tempo and time signature for the beat

#ifdef EOF_DEBUG_MIDI_IMPORT
sprintf(debugtext,"Start delta %lu / %lu: Finding tempo and TS",deltapos,last_delta_time);
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
sprintf(debugtext,"Start delta %lu / %lu: Storing timing info",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

assert(sp->tags != NULL);	//Prevent a NULL dereference below

		sp->beat[sp->beats - 1]->fpos = realtimepos + sp->tags->ogg[0].midi_offset;
		sp->beat[sp->beats - 1]->pos = realtimepos + sp->tags->ogg[0].midi_offset + 0.5;	//Round up to nearest millisecond
		sp->beat[sp->beats - 1]->midi_pos = deltapos;
		sp->beat[sp->beats - 1]->ppqn = curppqn;
		if(eof_use_ts && ((lastnum != curnum) || (lastden != curden) || (sp->beats - 1 == 0)))
		{	//If the user opted to import TS changes, and this time signature is different than the last beat's time signature (or this is the first beat)

assert(sp->beats > 0);	//Prevent eof_apply_ts() below from failing

			eof_apply_ts(curnum,curden,sp->beats - 1,sp,0);	//Set the TS flags for this beat
		}

assert(curppqn != 0);	//Avoid a division by 0 below

	//Update anchor linked list

#ifdef EOF_DEBUG_MIDI_IMPORT
sprintf(debugtext,"Start delta %lu / %lu: Updating anchor list",deltapos,last_delta_time);
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
sprintf(debugtext,"Start delta %lu / %lu: Calculate mid best tempo/TS change",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		midbeatchange = 0;
//This commented out line was causing timing errors when importing a RB MIDI that used #/8 TS
//		beatlength = ((double)eof_work_midi->divisions * curden / 4.0);		//Determine the length of one full beat in delta ticks
		beatlength = eof_work_midi->divisions;		//Determine the length of one full beat in delta ticks
		nextanchor = deltafpos + beatlength + 0.5;	//By default, the delta position of the next beat will be the standard length of delta ticks
		for(ctr = 0; ctr < eof_import_bpm_events->events; ctr++)
		{	//For each imported tempo change
			if(eof_import_bpm_events->event[ctr]->pos > deltapos)
			{	//If this tempo change is ahead of the current delta position
				if(eof_import_bpm_events->event[ctr]->pos < nextanchor)
				{	//If this tempo change occurs before the next beat marker
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
		}

	//Update delta and realtime counters (the TS affects a beat's length in deltas, the tempo affects a beat's length in milliseconds)

#ifdef EOF_DEBUG_MIDI_IMPORT
sprintf(debugtext,"Start delta %lu / %lu: Updating counters",deltapos,last_delta_time);
set_window_title(debugtext);
#endif

		beatreallength = (60000.0 / (60000000.0 / (double)curppqn));		//Determine the length of this beat in milliseconds
		realtimepos += beatreallength;	//Add the realtime length of this beat to the time counter
		deltafpos += beatlength;	//Add the delta length of this beat to the delta counter
		deltapos = deltafpos + 0.5;	//Round up to nearest delta tick
		lastnum = curnum;
		lastden = curden;
	}//Add new beats until enough have been added to encompass the last MIDI event

#ifdef EOF_DEBUG_MIDI_IMPORT
allegro_message("Pass two, configuring beat timings");
#endif

	double BPM=120.0;	//Assume a default tempo of 120BPM and TS of 4/4 at 0 deltas
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
				deltapos = sp->beat[ctr2]->midi_pos;		//Store the delta time position
				eof_get_ts(sp,NULL,&curden,ctr2);			//Find the TS denominator of this beat
			}
		}

		//Store the TS change's realtime position, using the appropriate formula to find the time beyond the beat real time position if the TS change is not on a beat marker:
		//time = deltas / (time division) * (60000.0 / (BPM * (TS denominator) / 4))
		eof_import_ts_changes[0]->change[ctr]->realtime = (double)realtimepos + (eof_import_ts_changes[0]->change[ctr]->pos - deltapos) / eof_work_midi->divisions * (60000.0 / (BPM * lastden / 4.0));
		lastden=curden;
	}

#ifdef EOF_DEBUG_MIDI_IMPORT
allegro_message("Second pass complete");
#endif

	/* third pass, create EOF notes */
	int picked_track;

	/* for Rock Band songs, we need to ignore certain tracks */
	char used_track[EOF_MAX_IMPORT_MIDI_TRACKS] = {0};

	unsigned char diff = 0;
	unsigned char diff_chart[EOF_MAX_FRETS] = {1, 2, 4, 8, 16, 32};
	long note_count[EOF_MAX_IMPORT_MIDI_TRACKS] = {0};
	int first_note;
	unsigned long hopopos[4];			//Used for forced HOPO On/Off parsing
	char hopotype[4];					//Used for forced HOPO On/Off parsing
	int hopodiff;						//Used for forced HOPO On/Off parsing
	unsigned long strumpos[4];			//Used for pro guitar strum direction parsing
	char strumtype[4];					//Used for pro guitar strum direction parsing
	int strumdiff;						//Used for pro guitar strum direction parsing
	unsigned long event_realtime;		//Store the delta time converted to realtime to avoid having to convert multiple times per note
	char prodrums = 0;					//Tracks whether the drum track being written includes Pro drum notation
	unsigned long tracknum;				//Used to de-obfuscate the legacy track number
	EOF_PRO_GUITAR_NOTE *currentsupaeasy, *currenteasy, *currentmedium, *currentamazing, **lastaddednotedifficulty;	//Used to import pro guitar slides, stores the pointer to the active note for each difficulty and set back to NULL when the note's first note off is reached

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
			first_note = note_count[picked_track];
			tracknum = sp->track[picked_track]->tracknum;

//Detect whether Pro drum notation is being used
//Pro drum notation is that if a green, yellow or blue drum note is NOT to be marked as a cymbal,
//it must be marked with the appropriate MIDI note, otherwise the note defaults as a cymbal
			if(eof_midi_tracks[picked_track].track_behavior == EOF_DRUM_TRACK_BEHAVIOR)
			{	//If the track being imported is a drum track
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

			currentsupaeasy = currenteasy = currentmedium = currentamazing = NULL;	//These point to nothing at the start of each track
			lastaddednotedifficulty = NULL;
			for(j = 0; j < eof_import_events[i]->events; j++)
			{	//For each event in this track
				if(key[KEY_ESC])
				{	/* clean up and return */
					eof_import_destroy_events_list(eof_import_bpm_events);
					eof_import_destroy_events_list(eof_import_text_events);
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
					sprintf(ttit, "MIDI Import (%d%%)", percent <= 100 ? percent : 100);
					set_window_title(ttit);
				}

				event_realtime = eof_ConvertToRealTimeInt(eof_import_events[i]->event[j]->pos,anchorlist,eof_import_ts_changes[0],eof_work_midi->divisions,sp->tags->ogg[0].midi_offset);
				eof_track_resize(sp, picked_track, note_count[picked_track] + 1);
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
							strcpy(sp->vocal_track[0]->lyric[note_count[picked_track]]->text, eof_import_events[i]->event[j]->text);
							sp->vocal_track[0]->lyric[note_count[picked_track]]->note = 0;
							sp->vocal_track[0]->lyric[note_count[picked_track]]->pos = event_realtime;
							sp->vocal_track[0]->lyric[note_count[picked_track]]->length = 100;
							note_count[picked_track]++;
						}
						else
						{
							strcpy(sp->vocal_track[0]->lyric[k]->text, eof_import_events[i]->event[j]->text);
						}
					}
				}//If parsing a vocal track
				else if(eof_midi_tracks[picked_track].track_format == EOF_LEGACY_TRACK_FORMAT)
				{	//If parsing a legacy track
					if((eof_import_events[i]->event[j]->type == 0x80) || (eof_import_events[i]->event[j]->type == 0x90))
					{	//For note on/off events, determine the type (difficulty) of the event
						if(eof_import_events[i]->event[j]->type == 0x90)
						{	//If this is a Note on event
							eof_set_note_flags(sp, picked_track, note_count[picked_track], 0);	//Clear the flag here so that the flag can be set later (ie. if it's an Expert+ double bass note)
						}
						if((eof_import_events[i]->event[j]->d1 >= 0x3C) && (eof_import_events[i]->event[j]->d1 < 0x3C + 6))
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_SUPAEASY);
							diff = eof_import_events[i]->event[j]->d1 - 0x3C;
						}
						else if((eof_import_events[i]->event[j]->d1 >= 0x48) && (eof_import_events[i]->event[j]->d1 < 0x48 + 6))
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_EASY);
							diff = eof_import_events[i]->event[j]->d1 - 0x48;
						}
						else if((eof_import_events[i]->event[j]->d1 >= 0x54) && (eof_import_events[i]->event[j]->d1 < 0x54 + 6))
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_MEDIUM);
							diff = eof_import_events[i]->event[j]->d1 - 0x54;
						}
						else if((eof_import_events[i]->event[j]->d1 >= 0x60) && (eof_import_events[i]->event[j]->d1 < 0x60 + 6))
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_AMAZING);
							diff = eof_import_events[i]->event[j]->d1 - 0x60;
						}
						else if((eof_import_events[i]->event[j]->d1 >= 120) && (eof_import_events[i]->event[j]->d1 <= 124))
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_SPECIAL);
							diff = eof_import_events[i]->event[j]->d1 - 120;
						}
						else if((picked_track == EOF_TRACK_DRUM) && (eof_import_events[i]->event[j]->d1 == 95))
						{	//If the track being read is PART DRUMS, and this note is marked for Expert+ double bass
							sp->legacy_track[tracknum]->note[note_count[picked_track]]->type = EOF_NOTE_AMAZING;
							sp->legacy_track[tracknum]->note[note_count[picked_track]]->flags |= EOF_NOTE_FLAG_DBASS;	//Apply this status flag
							diff = eof_import_events[i]->event[j]->d1 - 0x60 + 1;	//Treat as gem 1 (bass drum)
						}
						else
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], -1);
						}
					}

					/* note on */
					if(eof_import_events[i]->event[j]->type == 0x90)
					{
						if(eof_midi_tracks[picked_track].track_behavior == EOF_DRUM_TRACK_BEHAVIOR)
						{	//If this is a drum track, lane 6 is used for the fifth drum lane and not a HOPO marker
							if(diff == 5)
							{	//A lane 6 gem encountered for a drum track will cause the track to be marked as being a "five lane" drum track
								sp->track[picked_track]->flags |= EOF_TRACK_FLAG_FIVE_LANE_DRUM;	//Set the five lane drum flag
								sp->legacy_track[tracknum]->numlanes = 6;
							}
						}
						else
						{	/* store forced HOPO marker, when the note off for this marker occurs, search for note with same position and apply it to that note */
							if(eof_import_events[i]->event[j]->d1 == 0x3C + 5)
							{
								hopopos[0] = event_realtime;
								hopotype[0] = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x48 + 5)
							{
								hopopos[1] = event_realtime;
								hopotype[1] = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x54 + 5)
							{
								hopopos[2] = event_realtime;
								hopotype[2] = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x60 + 5)
							{
								hopopos[3] = event_realtime;
								hopotype[3] = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x3C + 6)
							{
								hopopos[0] = event_realtime;
								hopotype[0] = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x48 + 6)
							{
								hopopos[1] = event_realtime;
								hopotype[1] = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x54 + 6)
							{
								hopopos[2] = event_realtime;
								hopotype[2] = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x60 + 6)
							{
								hopopos[3] = event_realtime;
								hopotype[3] = 1;
							}
						}

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

						if(eof_get_note_type(sp, picked_track, note_count[picked_track]) != -1)
						{	//If there was a note added
							for(k = first_note; k < note_count[picked_track]; k++)
							{	//Traverse the note list in reverse to find the appropriate note to modify
								if((eof_get_note_pos(sp, picked_track, k) == event_realtime) && (eof_get_note_type(sp, picked_track, k) == eof_get_note_type(sp, picked_track, note_count[picked_track])))
								{
									break;
								}
							}
							if(k == note_count[picked_track])
							{
								notenum = note_count[picked_track];
								eof_set_note_note(sp, picked_track, notenum, diff_chart[diff]);
								eof_set_note_pos(sp, picked_track, notenum, event_realtime);
								eof_set_note_length(sp, picked_track, notenum, 100);
								note_count[picked_track]++;
							}
							else
							{
								notenum = k;
								eof_set_note_note(sp, picked_track, notenum, eof_get_note_note(sp, picked_track, notenum) | diff_chart[diff]);
							}
							if(prodrums && (picked_track == EOF_TRACK_DRUM) && (eof_get_note_type(sp, picked_track, notenum) != EOF_NOTE_SPECIAL))
							{	//If pro drum notation is in effect and this was a non BRE drum note
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
					}

					/* note off so get length of note */
					else if(eof_import_events[i]->event[j]->type == 0x80)
					{
						if(eof_midi_tracks[picked_track].track_behavior == EOF_DRUM_TRACK_BEHAVIOR)
						{	//If this is a drum track, lane 6 is used for the fifth drum lane and not a HOPO marker
						}
						else
						{	/* detect forced HOPO */
							hopodiff = -1;
							if(eof_import_events[i]->event[j]->d1 == 0x3C + 5)
							{
								hopodiff = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x48 + 5)
							{
								hopodiff = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x54 + 5)
							{
								hopodiff = 2;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x60 + 5)
							{
								hopodiff = 3;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x3C + 6)
							{
								hopodiff = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x48 + 6)
							{
								hopodiff = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x54 + 6)
							{
								hopodiff = 2;
							}
							else if(eof_import_events[i]->event[j]->d1 == 0x60 + 6)
							{
								hopodiff = 3;
							}
							if(hopodiff >= 0)
							{
								for(k = note_count[picked_track] - 1; k >= first_note; k--)
								{	//Check for each note that has been imported
									if((eof_get_note_type(sp, picked_track, k) == hopodiff) && (eof_get_note_pos(sp, picked_track, k) >= hopopos[hopodiff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
									{	//If the note is in the same difficulty as the HOPO phrase, and its timestamp falls between the HOPO On and HOPO Off marker
										if(hopotype[hopodiff] == 0)
										{
											eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_NOTE_FLAG_F_HOPO);
										}
										else
										{
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
								{	//Check for each note that has been imported for this track
									if((sp->legacy_track[tracknum]->note[k]->pos >= rb_pro_green_pos) && (sp->legacy_track[tracknum]->note[k]->pos <= event_realtime))
									{	//If the note is positioned within this pro green drum phrase
										sp->legacy_track[tracknum]->note[k]->flags &= (~EOF_NOTE_FLAG_G_CYMBAL);	//Clear the green cymbal marker on the note
									}
								}
								rb_pro_green = 0;
							}
						}

						if((eof_import_events[i]->event[j]->d1 == 103) && (eof_get_num_solos(sp, picked_track) < EOF_MAX_PHRASES))
						{
							phraseptr = eof_get_solo(sp, picked_track, eof_get_num_solos(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_solos(sp, picked_track, eof_get_num_solos(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 116) && (eof_get_num_star_power_paths(sp, picked_track) < EOF_MAX_PHRASES))
						{
							phraseptr = eof_get_star_power_path(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_star_power_paths(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 126) && (eof_get_num_tremolos(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a tremolo phrase
							phraseptr = eof_get_tremolo(sp, picked_track, eof_get_num_tremolos(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_tremolos(sp, picked_track, eof_get_num_tremolos(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 127) && (eof_get_num_trills(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a trill phrase
							phraseptr = eof_get_trill(sp, picked_track, eof_get_num_trills(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_trills(sp, picked_track, eof_get_num_trills(sp, picked_track) + 1);
						}

						if((note_count[picked_track] > 0) && (eof_get_note_type(sp, picked_track, note_count[picked_track] - 1) != -1))
						{
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{
								if((eof_get_note_type(sp, picked_track, k) == eof_get_note_type(sp, picked_track, note_count[picked_track])) && (eof_get_note_note(sp, picked_track, k) & diff_chart[diff]))
								{
	//								allegro_message("break %d, %d, %d", k, sp->legacy_track[picked_track]->note[k]->note, sp->legacy_track[picked_track]->note[note_count[picked_track]]->note);
									eof_set_note_length(sp, picked_track, k, event_realtime - eof_get_note_pos(sp, picked_track, k));
									if(eof_get_note_length(sp, picked_track, k ) <= 0)
									{
										eof_set_note_length(sp, picked_track, k, 1);
									}
									break;
								}
							}
						}
					}
				}//If parsing a legacy track
				else if(eof_midi_tracks[picked_track].track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
				{	//If parsing a pro guitar track
					if((eof_import_events[i]->event[j]->type == 0x80) || (eof_import_events[i]->event[j]->type == 0x90))
					{	//For note on/off events, determine the type (difficulty) of the event
						if((eof_import_events[i]->event[j]->d1 >= 24) && (eof_import_events[i]->event[j]->d1 <= 29))
						{	//Notes 24 through 29 represent supaeasy pro guitar
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_SUPAEASY);
							diff = eof_import_events[i]->event[j]->d1 - 24;
							chordname = chord0name;		//Have this pointer reference the supaeasy note name array
							lastaddednotedifficulty = &currentsupaeasy;	//Remember that this is the note that will be added
						}
						else if((eof_import_events[i]->event[j]->d1 >= 48) && (eof_import_events[i]->event[j]->d1 <= 53))
						{	//Notes 48 through 53 represent easy pro guitar
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_EASY);
							diff = eof_import_events[i]->event[j]->d1 - 48;
							chordname = chord1name;		//Have this pointer reference the easy note name array
							lastaddednotedifficulty = &currenteasy;	//Remember that this is the note that will be added
						}
						else if((eof_import_events[i]->event[j]->d1 >= 72) && (eof_import_events[i]->event[j]->d1 <= 77))
						{	//Notes 72 through 77 represent medium pro guitar
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_MEDIUM);
							diff = eof_import_events[i]->event[j]->d1 - 72;
							chordname = chord2name;		//Have this pointer reference the medium note name array
							lastaddednotedifficulty = &currentmedium;	//Remember that this is the note that will be added
						}
						else if((eof_import_events[i]->event[j]->d1 >= 96) && (eof_import_events[i]->event[j]->d1 <= 101))
						{	//Notes 96 through 101 represent amazing pro guitar
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_AMAZING);
							diff = eof_import_events[i]->event[j]->d1 - 96;
							chordname = chord3name;		//Have this pointer reference the amazing note name array
							lastaddednotedifficulty = &currentamazing;	//Remember that this is the note that will be added
						}
						else if((eof_import_events[i]->event[j]->d1 >= 120) && (eof_import_events[i]->event[j]->d1 <= 124))
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], EOF_NOTE_SPECIAL);
							diff = eof_import_events[i]->event[j]->d1 - 120;
							chordname = NULL;		//BRE notes do not store chord names
						}
						else
						{
							eof_set_note_type(sp, picked_track, note_count[picked_track], -1);
						}
					}

					/* note on */
					if(eof_import_events[i]->event[j]->type == 0x90)
					{
						/* store strum direction marker, when the note off for this marker occurs, search for notes with same position and apply it to them */
						if((eof_import_events[i]->event[j]->d2 == 96) && (eof_import_events[i]->event[j]->channel == 13))
						{	//Lane (1+9), Velocity 96 and channel 13 are used in up strum markers
							if(eof_import_events[i]->event[j]->d1 == 24 + 9)
							{
								strumpos[0] = event_realtime;
								strumtype[0] = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 48 + 9)
							{
								strumpos[1] = event_realtime;
								strumtype[1] = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 72 + 9)
							{
								strumpos[2] = event_realtime;
								strumtype[2] = 0;
							}
							else if(eof_import_events[i]->event[j]->d1 == 96 + 9)
							{
								strumpos[3] = event_realtime;
								strumtype[3] = 0;
							}
						}
						if((eof_import_events[i]->event[j]->d2 == 114) && (eof_import_events[i]->event[j]->channel == 15))
						{	//Lane (1+9), Velocity 114 and channel 15 are used in down strum markers
							if(eof_import_events[i]->event[j]->d1 == 24 + 9)
							{
								strumpos[0] = event_realtime;
								strumtype[0] = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 48 + 9)
							{
								strumpos[1] = event_realtime;
								strumtype[1] = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 72 + 9)
							{
								strumpos[2] = event_realtime;
								strumtype[2] = 1;
							}
							else if(eof_import_events[i]->event[j]->d1 == 96 + 9)
							{
								strumpos[3] = event_realtime;
								strumtype[3] = 1;
							}
						}

						/* arpeggios, solos, star power, tremolos and trills */
						phraseptr = NULL;
						if((eof_import_events[i]->event[j]->d1 == 104) && (eof_get_num_arpeggios(sp, picked_track) < EOF_MAX_PHRASES))
						{	//Arpeggios are marked with note 104
							phraseptr = eof_get_arpeggio(sp, picked_track, eof_get_num_arpeggios(sp, picked_track));
							phraseptr->start_pos = event_realtime;
						}
						else if((eof_import_events[i]->event[j]->d1 == 115) && (eof_get_num_solos(sp, picked_track) < EOF_MAX_PHRASES))
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

						for(k = first_note; k < note_count[picked_track]; k++)
						{	//Traverse the note list in reverse to find the appropriate note to modify
							if((eof_get_note_pos(sp, picked_track, k) == event_realtime) && (eof_get_note_type(sp, picked_track, k) == eof_get_note_type(sp, picked_track, note_count[picked_track])))
							{
								break;
							}
						}
						if(eof_get_note_type(sp, picked_track, note_count[picked_track]) != -1)
						{	//If there is a gem to add
							if(k == note_count[picked_track])
							{	//If a new note was created, add the note
								notenum = note_count[picked_track];
								eof_set_note_note(sp, picked_track, notenum, diff_chart[diff]);
								eof_set_note_pos(sp, picked_track, notenum, event_realtime);
								eof_set_note_length(sp, picked_track, notenum, 100);
								eof_set_note_flags(sp, picked_track, notenum, 0);	//Clear the flag here so that the flag can be set if it has a special status
								if(chordname && (chordname[0] != '\0'))
								{	//If there is a chord name stored for this difficulty
									eof_set_note_name(sp, picked_track, notenum, chordname);	//Populate the note name with whatever name was read last
									chordname[0] = '\0';										//Empty the chord name array for this difficulty
								}
								else
								{	//Otherwise ensure the note has an empty name string
									eof_set_note_name(sp, picked_track, notenum, "");
								}
								assert(lastaddednotedifficulty != NULL);	//lastaddednotedifficulty should be correctly set from above
								*lastaddednotedifficulty = sp->pro_guitar_track[tracknum]->note[notenum];	//Store a pointer to this new note into the appropriate "current" pro note pointer
								note_count[picked_track]++;
							}
							else
							{	//Otherwise edit the existing note
								notenum = k;
								eof_set_note_note(sp, picked_track, notenum, eof_get_note_note(sp, picked_track, notenum) | diff_chart[diff]);
							}
							sp->pro_guitar_track[tracknum]->note[notenum]->frets[diff] = eof_import_events[i]->event[j]->d2 - 100;	//Velocity (100 + X) represents fret # X
							if(sp->pro_guitar_track[tracknum]->note[notenum]->frets[diff] > sp->pro_guitar_track[tracknum]->numfrets)
							{	//If this fret value is higher than this track's recorded maximum
								sp->pro_guitar_track[tracknum]->numfrets = sp->pro_guitar_track[tracknum]->note[notenum]->frets[diff];	//Increase the maximum to reflect this fret value
							}
							if(eof_import_events[i]->event[j]->channel == 1)
							{	//If this note was sent over channel 1, it is a ghost note
								sp->pro_guitar_track[tracknum]->note[notenum]->ghost |= diff_chart[diff];	//Set the ghost flag for this gem's string
							}
							else if(eof_import_events[i]->event[j]->channel == 2)
							{	//If this note was sent over channel 2, it is a forced hammer on
								sp->pro_guitar_track[tracknum]->note[notenum]->flags |= EOF_PRO_GUITAR_NOTE_FLAG_HO;	//Set the forced HO flag
							}
							else if(eof_import_events[i]->event[j]->channel == 3)
							{	//If this note was sent over channel 3, the gem is muted
//								sp->pro_guitar_track[tracknum]->note[notenum]->flags |= EOF_PRO_GUITAR_NOTE_FLAG_STRING_MUTE;	//Set the string mute flag
								sp->pro_guitar_track[tracknum]->note[notenum]->frets[diff] = 0xFF;
							}
						}
						else
						{	//Apply other markers
							if((eof_import_events[i]->event[j]->d2 == 104) || (eof_import_events[i]->event[j]->d2 == 105) || (eof_import_events[i]->event[j]->d2 == 106) || (eof_import_events[i]->event[j]->d2 == 107) || (eof_import_events[i]->event[j]->d2 == 108) || (eof_import_events[i]->event[j]->d2 == 109) || (eof_import_events[i]->event[j]->d2 == 112) || (eof_import_events[i]->event[j]->d2 == 115) || (eof_import_events[i]->event[j]->d2 == 116) || (eof_import_events[i]->event[j]->d2 == 117))
							{	//If this note uses any of the velocities representing a slide down section
								if((eof_import_events[i]->event[j]->d1 == 31) && currentsupaeasy)
								{	//If this is a supaeasy slide down and a supaeasy difficulty note is in progess
									currentsupaeasy->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN;
								}
								else if((eof_import_events[i]->event[j]->d1 == 55) && currenteasy)
								{	//If this is an easy slide down and an easy difficulty note is in progess
									currenteasy->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN;
								}
								else if((eof_import_events[i]->event[j]->d1 == 79) && currentmedium)
								{	//If this is a medium slide down and a medium difficulty note is in progess
									currentmedium->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN;
								}
								else if((eof_import_events[i]->event[j]->d1 == 103) && currentamazing)
								{	//If this is an expert slide down and an amazing difficulty note is in progess
									currentamazing->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN;
								}
							}
							else if((eof_import_events[i]->event[j]->d2 == 102) || (eof_import_events[i]->event[j]->d2 == 103) || (eof_import_events[i]->event[j]->d2 == 110))
							{	//If this note uses any of the velocities representing a slide up section
								if((eof_import_events[i]->event[j]->d1 == 31) && currentsupaeasy)
								{	//If this is a supaeasy slide up and a supaeasy difficulty note is in progess
									currentsupaeasy->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP;
								}
								else if((eof_import_events[i]->event[j]->d1 == 55) && currenteasy)
								{	//If this is an easy slide up and an easy difficulty note is in progess
									currenteasy->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP;
								}
								else if((eof_import_events[i]->event[j]->d1 == 79) && currentmedium)
								{	//If this is a medium slide up and a medium difficulty note is in progess
									currentmedium->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP;
								}
								else if((eof_import_events[i]->event[j]->d1 == 103) && currentamazing)
								{	//If this is an expert slide up and an amazing difficulty note is in progess
									currentamazing->flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP;
								}
							}
						}
					}

					/* note off so get length of note */
					else if(eof_import_events[i]->event[j]->type == 0x80)
					{
						strumdiff = -1;
						/* detect strum direction markers */
						if(eof_import_events[i]->event[j]->channel == 13)
						{	//Lane (1+9), Velocity 96 and channel 13 are used in up strum markers
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
						if(eof_import_events[i]->event[j]->channel == 15)
						{	//Lane (1+9), Velocity 114 and channel 15 are used in down strum markers
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
						{
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{	//Check for each note that has been imported
								if((eof_get_note_type(sp, picked_track, k) == strumdiff) && (eof_get_note_pos(sp, picked_track, k) >= strumpos[strumdiff]) && (eof_get_note_pos(sp, picked_track, k) <= event_realtime))
								{	//If the note is in the same difficulty as the strum direction marker, and its timestamp falls between the start and stop of the marker
									if(strumtype[strumdiff] == 0)
									{	//This was an up strum marker
										eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_UP_STRUM);
									}
									else
									{	//This was a down strum marker
										eof_set_note_flags(sp, picked_track, k, eof_get_note_flags(sp, picked_track, k) | EOF_PRO_GUITAR_NOTE_FLAG_DOWN_STRUM);
									}
								}
							}
						}

						if((eof_import_events[i]->event[j]->d1 == 104) && (eof_get_num_arpeggios(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of an arpeggio phrase
							phraseptr = eof_get_arpeggio(sp, picked_track, eof_get_num_arpeggios(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_arpeggios(sp, picked_track, eof_get_num_arpeggios(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 115) && (eof_get_num_solos(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a solo phrase
							phraseptr = eof_get_solo(sp, picked_track, eof_get_num_solos(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_solos(sp, picked_track, eof_get_num_solos(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 116) && (eof_get_num_star_power_paths(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a star power phrase
							phraseptr = eof_get_star_power_path(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_star_power_paths(sp, picked_track, eof_get_num_star_power_paths(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 126) && (eof_get_num_tremolos(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a tremolo phrase
							phraseptr = eof_get_tremolo(sp, picked_track, eof_get_num_tremolos(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_tremolos(sp, picked_track, eof_get_num_tremolos(sp, picked_track) + 1);
						}
						else if((eof_import_events[i]->event[j]->d1 == 127) && (eof_get_num_trills(sp, picked_track) < EOF_MAX_PHRASES))
						{	//End of a trill phrase
							phraseptr = eof_get_trill(sp, picked_track, eof_get_num_trills(sp, picked_track));
							phraseptr->end_pos = event_realtime - 1;
							eof_set_num_trills(sp, picked_track, eof_get_num_trills(sp, picked_track) + 1);
						}

						if((note_count[picked_track] > 0) && (eof_get_note_type(sp, picked_track, note_count[picked_track] - 1) != -1))
						{
							for(k = note_count[picked_track] - 1; k >= first_note; k--)
							{
								if((eof_get_note_type(sp, picked_track, k) == eof_get_note_type(sp, picked_track, note_count[picked_track])) && (eof_get_note_note(sp, picked_track, k) & diff_chart[diff]))
								{
									switch(eof_get_note_type(sp, picked_track, k))
									{
										case EOF_NOTE_AMAZING:
											currentamazing = NULL;	//The end of this note has been reached
										break;
										case EOF_NOTE_MEDIUM:
											currentmedium = NULL;	//The end of this note has been reached
										break;
										case EOF_NOTE_EASY:
											currenteasy = NULL;		//The end of this note has been reached
										break;
										case EOF_NOTE_SUPAEASY:
											currentsupaeasy = NULL;	//The end of this note has been reached
										break;
									}
	//								allegro_message("break %d, %d, %d", k, sp->legacy_track[picked_track]->note[k]->note, sp->legacy_track[picked_track]->note[note_count[picked_track]]->note);
									eof_set_note_length(sp, picked_track, k, event_realtime - eof_get_note_pos(sp, picked_track, k));
									if(eof_get_note_length(sp, picked_track, k ) <= 0)
									{
										eof_set_note_length(sp, picked_track, k, 1);
									}
									break;
								}
							}
						}
					}

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
//For now, prefer RB3's system, where "NC" isn't automatically written for un-named chords
//							if(ustrcmp(chordname, "NC") == 0)
//							{	//If the name is a "No Chord" marker
//								chordname[0] = '\0';	//Empty the string
//							}
								}
							}
						}
					}
				}//If parsing a pro guitar track
				pticker++;
			}//For each event in this track

			eof_track_resize(sp, picked_track, note_count[picked_track]);
			if(eof_get_track_size(sp, picked_track) > 0)
			{
				eof_track_find_crazy_notes(sp, picked_track);
				used_track[picked_track] = 1;
			}
		}//If this is a valid track to process
	}//For each imported track

#ifdef EOF_DEBUG_MIDI_IMPORT
allegro_message("Third pass complete");
#endif

	/* delete empty lyric lines */
	int lc;
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
			{	//Only process not BRE notes
				if((sp->legacy_track[tracknum]->note[k]->note & 4) && ((sp->legacy_track[tracknum]->note[k]->flags & EOF_NOTE_FLAG_Y_CYMBAL) == 0))
				{	//If this note has a yellow gem with the cymbal marker cleared
					eof_set_flags_at_legacy_note_pos(sp->legacy_track[tracknum],k,EOF_NOTE_FLAG_Y_CYMBAL,0);	//Ensure all drum notes at this position have the flag cleared
				}
				if((sp->legacy_track[tracknum]->note[k]->note & 8) && ((sp->legacy_track[tracknum]->note[k]->flags & EOF_NOTE_FLAG_B_CYMBAL) == 0))
				{	//If this note has a blue gem with the cymbal marker cleared
					eof_set_flags_at_legacy_note_pos(sp->legacy_track[tracknum],k,EOF_NOTE_FLAG_B_CYMBAL,0);	//Ensure all drum notes at this position have the flag cleared
				}
				if((sp->legacy_track[tracknum]->note[k]->note & 16) && ((sp->legacy_track[tracknum]->note[k]->flags & EOF_NOTE_FLAG_G_CYMBAL) == 0))
				{	//If this note has a green gem with the cymbal marker cleared
					eof_set_flags_at_legacy_note_pos(sp->legacy_track[tracknum],k,EOF_NOTE_FLAG_G_CYMBAL,0);	//Ensure all drum notes at this position have the flag cleared
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

//Check for forced HOPO on lane 1 in PART BASS and prompt for whether to treat as open bass strumming
	tracknum = sp->track[EOF_TRACK_BASS]->tracknum;
	for(i = 0; i < sp->legacy_track[tracknum]->notes; i++)
	{	//For each note in the bass track
		if((sp->legacy_track[tracknum]->note[i]->note & 1) && (sp->legacy_track[tracknum]->note[i]->flags & EOF_NOTE_FLAG_F_HOPO))
		{	//If this note has a gem in lane one and is forced as a HOPO, prompt the user how to handle them
			eof_cursor_visible = 0;
			eof_pen_visible = 0;
			eof_show_mouse(screen);
			if((sp->track[EOF_TRACK_BASS]->flags & EOF_TRACK_FLAG_OPEN_STRUM) || (alert(NULL, "Import lane 1 forced HOPO bass notes as open strums?", NULL, "&Yes", "&No", 'y', 'n') == 1))
			{	//If the open bass strum flag was set during INI import or if the user opts to import lane 1 HOPO bass notes as open strums
				sp->legacy_track[tracknum]->numlanes = 6;						//Set this track to have 6 lanes instead of 5
				sp->track[EOF_TRACK_BASS]->flags |= EOF_TRACK_FLAG_OPEN_STRUM;	//Set this flag
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
						sp->pro_guitar_track[i]->note[j]->frets[k] = 0xFF;	//Set this string to be muted
					}
				}
			}
		}
	}
	replace_filename(eof_song_path, fn, "", 1024);
	append_filename(nfn, eof_song_path, "guitar.ogg", 1024);
	if(!eof_load_ogg(nfn))
	{
		eof_destroy_song(sp);
		eof_import_destroy_events_list(eof_import_bpm_events);
		eof_import_destroy_events_list(eof_import_text_events);
		for(i = 0; i < tracks; i++)
		{
			eof_import_destroy_events_list(eof_import_events[i]);
		}
		destroy_midi(eof_work_midi);
		eof_destroy_tempo_list(anchorlist);
		return NULL;
	}
	eof_song_loaded = 1;
	eof_music_length = alogg_get_length_msecs_ogg(eof_music_track);

	/* create text events */
	long b = -1;
	unsigned long tp;
	for(i = 0; i < eof_import_text_events->events; i++)
	{
		if(eof_import_text_events->event[i]->type == 0x01)
		{
			tp = eof_ConvertToRealTimeInt(eof_import_text_events->event[i]->pos,anchorlist,eof_import_ts_changes[0],eof_work_midi->divisions,sp->tags->ogg[0].midi_offset);
			b = eof_import_closest_beat(sp, tp);
			if(b >= 0)
			{
//				allegro_message("%s", eof_import_text_events->event[i]->text);
				eof_song_add_text_event(sp, b, eof_import_text_events->event[i]->text);
				sp->beat[b]->flags |= EOF_BEAT_FLAG_EVENTS;
			}
		}
	}

	/* convert solos to star power for GH charts using FoFiX's method */
	for(i = 1; i < sp->tracks; i++)
	{	//For each track
		if(sp->track[i]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		{	//Only perform this conversion for non pro guitar tracks, since they use different markers
			if(eof_get_num_star_power_paths(sp, i) < 2)
			{	//If this track has less than two star power sections
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
		allegro_message("Warning:  Pro guitar tuning tag defines too few strings.  Reverting to 6 string standard tuning.");
		sp->pro_guitar_track[tracknum]->numstrings = 6;
		memset(sp->pro_guitar_track[tracknum]->tuning, 0, EOF_TUNING_LENGTH);
	}
	tracknum = sp->track[EOF_TRACK_PRO_BASS]->tracknum;
	if(eof_detect_string_gem_conflicts(sp->pro_guitar_track[tracknum], sp->pro_guitar_track[tracknum]->numstrings))
	{
		allegro_message("Warning:  Pro bass tuning tag defines too few strings.  Reverting to 6 string standard tuning.");
		sp->pro_guitar_track[tracknum]->numstrings = 6;
		memset(sp->pro_guitar_track[tracknum]->tuning, 0, EOF_TUNING_LENGTH);
	}

#ifdef EOF_DEBUG_MIDI_IMPORT
allegro_message("MIDI import complete");
#endif

	eof_log_level |= 2;	//Enable verbose logging
	return sp;
}
