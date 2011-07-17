#include <allegro.h>
#include "main.h"
#include "note.h"
#include "beat.h"
#include "event.h"
#include "ini.h"
#include "legacy.h"
#include "tuning.h"

#ifdef USEMEMWATCH
#include "memwatch.h"
#endif

char *eof_difficulty_ini_tags[EOF_TRACKS_MAX + 1] = {"", "diff_guitar", "diff_bass", "", "", "diff_drums", "diff_vocals", "diff_keys", "diff_bass_real", "diff_guitar_real", "", "diff_keys_real"};

int eof_save_ini(EOF_SONG * sp, char * fn)
{
	eof_log("eof_save_ini() entered", 1);

	PACKFILE * fp;
	char buffer[256] = {0};
	char ini_string[4096] = {0};
	unsigned long i, j, tracknum;
	char *tuning_name = NULL;

	if(!sp || !fn)
	{
		return 0;
	}
	fp = pack_fopen(fn, "w");
	if(!fp)
	{
		return 0;
	}

	/* write artist name */
	if(ustrlen(sp->tags->artist) > 0)
	{
		ustrcpy(ini_string, "[song]\r\nartist = ");
		ustrcat(ini_string, sp->tags->artist);
	}
	else
	{
		ustrcpy(ini_string, "[song]\r\nartist = ");
		ustrcat(ini_string, "Unknown Artist");
	}

	/* write song name */
	if(ustrlen(sp->tags->title) > 0)
	{
		ustrcat(ini_string, "\r\nname = ");
		ustrcat(ini_string, sp->tags->title);
	}
	else
	{
		ustrcat(ini_string, "\r\nname = ");
		ustrcat(ini_string, "Untitled");
	}

	/* write frettist name */
	if(ustrlen(sp->tags->frettist) > 0)
	{
		ustrcat(ini_string, "\r\nfrets = ");
		ustrcat(ini_string, sp->tags->frettist);
	}

	/* write midi offset */
	ustrcat(ini_string, "\r\ndelay = ");
	sprintf(buffer, "%ld", sp->tags->ogg[eof_selected_ogg].midi_offset);
	ustrcat(ini_string, buffer);

	/* write year */
	if(ustrlen(sp->tags->year) > 0)
	{
		ustrcat(ini_string, "\r\nyear = ");
		ustrcat(ini_string, sp->tags->year);
	}

	/* write loading text */
	if(ustrlen(sp->tags->loading_text) > 0)
	{
		ustrcat(ini_string, "\r\nloading_phrase = ");
		ustrcat(ini_string, sp->tags->loading_text);
	}

	/* write difficulty tags */
	for(i = 0; i < sp->tracks; i++)
	{	//For each track in the chart
		if(i == 0)
			continue;	//Until the band difficulty is implemented, skip this iteration

		if((eof_difficulty_ini_tags[i][0] != '\0') && (sp->track[i]->difficulty != 0xFF))
		{	//If this track has a supported difficulty tag and the difficulty is defined
			sprintf(buffer, "\r\n%s = %d", eof_difficulty_ini_tags[i], sp->track[i]->difficulty);
			ustrcat(ini_string, buffer);
		}
	}
	if(((eof_song->track[EOF_TRACK_DRUM]->flags & 0xFF000000) >> 24) != 0xFF)
	{	//If there is a defined pro drum difficulty
		sprintf(buffer, "\r\ndiff_drums_real = %lu", (eof_song->track[EOF_TRACK_DRUM]->flags & 0xFF000000) >> 24);
		ustrcat(ini_string, buffer);
	}
	if(((eof_song->track[EOF_TRACK_VOCALS]->flags & 0xFF000000) >> 24) != 0xFF)
	{	//If there is a defined harmony difficulty
		sprintf(buffer, "\r\ndiff_vocals_harm = %lu", (eof_song->track[EOF_TRACK_VOCALS]->flags & 0xFF000000) >> 24);
		ustrcat(ini_string, buffer);
	}
	if(eof_song->tags->difficulty != 0xFF)
	{	//If there is a defined band difficulty
		sprintf(buffer, "\r\ndiff_band = %lu", eof_song->tags->difficulty);
		ustrcat(ini_string, buffer);
	}

	/* write other settings */
	if(sp->tags->lyrics)
	{
		ustrcat(ini_string, "\r\nlyrics = True");
	}
	if(sp->tags->eighth_note_hopo)
	{
		ustrcat(ini_string, "\r\neighthnote_hopo = 1");
	}

	/* check for use of open bass strumming and write a tag if necessary */
	if(eof_open_bass_enabled())
	{	//If open bass was enabled during the time of the save
		tracknum = sp->track[EOF_TRACK_BASS]->tracknum;
		for(i = 0; i < sp->legacy_track[tracknum]->notes; i++)
		{	//For each note in the bass guitar track
			if(sp->legacy_track[tracknum]->note[i]->note & 32)
			{	//If lane 6 (open bass) is populated
				ustrcat(ini_string, "\r\nsysex_open_bass = True");	//Write the open strum Sysex presence tag (used in Phase Shift) to identify that this Sysex phrase will be written to MIDI
				break;	//Exit loop
			}
		}
	}

	/* check for use of pro guitar slides and write a tag if necessary */
	char slidesfound = 0;
	for(i = 1; i < sp->tracks; i++)
	{	//For each track in the chart (skipping track 0)
		if(sp->track[i]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
			continue;	//If this isn't a pro guitar/bass track

		tracknum = sp->track[i]->tracknum;
		for(j = 0; j < sp->pro_guitar_track[tracknum]->notes; j++)
		{	//For each note in the pro guitar/bass track
			if((sp->pro_guitar_track[tracknum]->note[j]->flags & EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP) || (sp->pro_guitar_track[tracknum]->note[j]->flags & EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN))
			{	//If this note slides up or down
				slidesfound = 1;
				break;
			}
		}
	}
	if(slidesfound)
	{	//If this chart has at least one pro guitar slide
		ustrcat(ini_string, "\r\nsysex_pro_slide = True");	//Write the pro guitar slide Sysex presence tag (used in Phase Shift) to identify that up/down slide Sysex phrases will be written to MIDI
	}

	ustrcat(ini_string, "\r\nstar_power_note = 116");	//Write this tag to indicate to Phase Shift that EOF is using Rock Band's notation for star power

	/* check for use of cymbal notation */
	if(eof_track_has_cymbals(sp, EOF_TRACK_DRUM))
	{	//If the drum track has any notes marked as cymbals
		ustrcat(ini_string, "\r\npro_drums = True");	//Write the pro drum tag, which indicates notes default as cymbals unless marked as toms (used in Phase Shift)
	}

	/* write tuning tags */
	if(eof_get_track_size(sp, EOF_TRACK_PRO_GUITAR))
	{	//If there is one or more notes in the pro guitar track
		ustrcat(ini_string, "\r\n");
		tracknum = sp->track[EOF_TRACK_PRO_GUITAR]->tracknum;
		ustrcat(ini_string, "real_guitar_tuning =");	//Write the pro guitar tuning tag
		for(i = 0; i < sp->pro_guitar_track[tracknum]->numstrings; i++)
		{	//For each string used in the track
			sprintf(buffer, " %d", sp->pro_guitar_track[tracknum]->tuning[i]);	//Write the string's tuning value (signed integer)
			ustrcat(ini_string, buffer);	//Append the string's tuning value to the ongoing INI string
		}
		tuning_name = eof_lookup_tuning_name(sp, EOF_TRACK_PRO_GUITAR, sp->pro_guitar_track[tracknum]->tuning);	//Check to see if this track's tuning has a defined name
		if(tuning_name != eof_tuning_unknown)
		{	//If the lookup found a name
			sprintf(buffer, " \"%s\"", tuning_name);
			ustrcat(ini_string, buffer);
		}
	}
	if(eof_get_track_size(sp, EOF_TRACK_PRO_BASS))
	{	//If there is one or more notes in the pro bass track
		ustrcat(ini_string, "\r\n");
		tracknum = sp->track[EOF_TRACK_PRO_BASS]->tracknum;
		ustrcat(ini_string, "real_bass_tuning =");	//Write the pro bass tuning tag
		for(i = 0; i < sp->pro_guitar_track[tracknum]->numstrings; i++)
		{	//For each string used in the track
			sprintf(buffer, " %d", sp->pro_guitar_track[tracknum]->tuning[i]);	//Write the string's tuning value (signed integer)
			ustrcat(ini_string, buffer);	//Append the string's tuning value to the ongoing INI string
		}
		tuning_name = eof_lookup_tuning_name(sp, EOF_TRACK_PRO_BASS, sp->pro_guitar_track[tracknum]->tuning);	//Check to see if this track's tuning has a defined name
		if(tuning_name != eof_tuning_unknown)
		{	//If the lookup found a name
			sprintf(buffer, " \"%s\"", tuning_name);
			ustrcat(ini_string, buffer);
		}
	}

	/* write custom INI settings */
	ustrcat(ini_string, "\r\n");
	for(i = 0; i < sp->tags->ini_settings; i++)
	{
		if(ustrlen(sp->tags->ini_setting[i]) > 0)
		{
			ustrcat(ini_string, sp->tags->ini_setting[i]);
			ustrcat(ini_string, "\r\n");
		}
	}

	pack_fwrite(ini_string, ustrlen(ini_string), fp);
	pack_fclose(fp);
	return 1;
}
