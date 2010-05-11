#include <allegro.h>
#include "main.h"
#include "editor.h"
#include "beat.h"
#include "song.h"
#include "legacy.h"

/* sort all notes according to position */
int eof_song_qsort_notes(const void * e1, const void * e2)
{
    EOF_NOTE ** thing1 = (EOF_NOTE **)e1;
    EOF_NOTE ** thing2 = (EOF_NOTE **)e2;

//	allegro_message("%d\n%d", (*thing1)->pos, (*thing2)->pos);
    if((*thing1)->pos < (*thing2)->pos)
	{
        return -1;
    }
    if((*thing1)->pos > (*thing2)->pos)
    {
        return 1;
    }

    // they are equal...
    return 0;
}


int eof_song_qsort_events(const void * e1, const void * e2)
{
	EOF_TEXT_EVENT ** thing1 = (EOF_TEXT_EVENT **)e1;
	EOF_TEXT_EVENT ** thing2 = (EOF_TEXT_EVENT **)e2;

	if((*thing1)->beat < (*thing2)->beat)
	{
		return -1;
	}
	else if((*thing1)->beat == (*thing2)->beat)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

int eof_song_qsort_lyrics(const void * e1, const void * e2)
{
    EOF_LYRIC ** thing1 = (EOF_LYRIC **)e1;
    EOF_LYRIC ** thing2 = (EOF_LYRIC **)e2;

    if((*thing1)->pos < (*thing2)->pos)
	{
        return -1;
    }
    if((*thing1)->pos > (*thing2)->pos)
    {
        return 1;
    }

    // they are equal...
    return 0;
}


EOF_SONG * eof_create_song(void)
{
	EOF_SONG * sp;
	int i;

	sp = malloc(sizeof(EOF_SONG));
	if(!sp)
	{
		return NULL;
	}
	sp->tags = malloc(sizeof(EOF_SONG_TAGS));
	if(!sp->tags)
	{
		return NULL;
	}
	ustrcpy(sp->tags->artist, "");
	ustrcpy(sp->tags->title, "");
	ustrcpy(sp->tags->frettist, "");
	ustrcpy(sp->tags->year, "");
	ustrcpy(sp->tags->loading_text, "");
	sp->tags->lyrics = 0;
	sp->tags->eighth_note_hopo = 0;
	sp->tags->ini_settings = 0;
	sp->tags->ogg[0].midi_offset = 0;
	ustrcpy(sp->tags->ogg[0].filename, "guitar.ogg");
	sp->tags->oggs = 1;
	sp->tags->revision = 0;
	for(i = 0; i < EOF_MAX_TRACKS; i++)
	{
		sp->track[i] = malloc(sizeof(EOF_TRACK));
		if(!sp->track[i])
		{
			return NULL;
		}
		sp->track[i]->notes = 0;
		sp->track[i]->solos = 0;
		sp->track[i]->star_power_paths = 0;
	}
	sp->vocal_track = malloc(sizeof(EOF_VOCAL_TRACK));
	if(!sp->vocal_track)
	{
		return NULL;
	}
	sp->vocal_track->lyrics = 0;
	sp->vocal_track->lines = 0;
	sp->beats = 0;
	sp->text_events = 0;
	sp->catalog = malloc(sizeof(EOF_CATALOG));
	if(!sp->catalog)
	{
		return NULL;
	}
	sp->catalog->entries = 0;
	for(i = 0; i < 10; i++)
	{
		sp->bookmark_pos[i] = 0;
	}
	return sp;
}

void eof_destroy_song(EOF_SONG * sp)
{
	free(sp);
}

int eof_save_song(EOF_SONG * sp, const char * fn)
{
	PACKFILE * fp;
	int i, j, tl;
	char header[16] = {'E', 'O', 'F', 'S', 'O', 'N', 'F', 0};

	fp = pack_fopen(fn, "w");
	if(!fp)
	{
		return 0;
	}

	pack_fwrite(header, 16, fp);

	/* write revision number */
	pack_iputl(sp->tags->revision, fp);

	/* write song info */
	tl = ustrlen(sp->tags->artist);
	pack_iputw(tl, fp);
	pack_fwrite(sp->tags->artist, tl, fp);
	tl = ustrlen(sp->tags->title);
	pack_iputw(tl, fp);
	pack_fwrite(sp->tags->title, tl, fp);
	tl = ustrlen(sp->tags->frettist);
	pack_iputw(tl, fp);
	pack_fwrite(sp->tags->frettist, tl, fp);
	tl = ustrlen(sp->tags->year);
	pack_iputw(tl, fp);
	pack_fwrite(sp->tags->year, tl, fp);
	tl = ustrlen(sp->tags->loading_text);
	pack_iputw(tl, fp);
	pack_fwrite(sp->tags->loading_text, tl, fp);
	pack_putc(sp->tags->lyrics, fp);
	pack_putc(sp->tags->eighth_note_hopo, fp);

	/* write OGG data */
	pack_iputw(sp->tags->oggs, fp);
	for(i = 0; i < sp->tags->oggs; i++)
	{
		pack_fwrite(sp->tags->ogg[i].filename, 256, fp);
		pack_iputl(sp->tags->ogg[i].midi_offset, fp);
	}

	/* write INI settings */
	pack_iputw(sp->tags->ini_settings, fp);
	for(i = 0; i < sp->tags->ini_settings; i++)
	{
		pack_fwrite(sp->tags->ini_setting[i], 512, fp);
	}

	/* write beat info */
	pack_iputl(sp->beats, fp);
	for(i = 0; i < sp->beats; i++)
	{
		pack_iputl(sp->beat[i]->ppqn, fp);
		pack_iputl(sp->beat[i]->pos, fp);
		pack_iputl(sp->beat[i]->flags, fp);
	}

	/* write events info */
	pack_iputl(sp->text_events, fp);
	for(i = 0; i < sp->text_events; i++)
	{
		pack_fwrite(sp->text_event[i]->text, 256, fp);
		pack_iputl(sp->text_event[i]->beat, fp);
	}

	/* write tracks */
	for(i = 0; i < EOF_MAX_TRACKS; i++)
	{

		/* write solo sections */
		pack_iputw(sp->track[i]->solos, fp);
		for(j = 0; j < sp->track[i]->solos; j++)
		{
			pack_iputl(sp->track[i]->solo[j].start_pos, fp);
			pack_iputl(sp->track[i]->solo[j].end_pos, fp);
		}

		/* write star power sections */
		pack_iputw(sp->track[i]->star_power_paths, fp);
		for(j = 0; j < sp->track[i]->star_power_paths; j++)
		{
			pack_iputl(sp->track[i]->star_power_path[j].start_pos, fp);
			pack_iputl(sp->track[i]->star_power_path[j].end_pos, fp);
		}

		/* write notes */
		pack_iputl(sp->track[i]->notes, fp);
		for(j = 0; j < sp->track[i]->notes; j++)
		{
			pack_putc(sp->track[i]->note[j]->type, fp);
			pack_putc(sp->track[i]->note[j]->note, fp);
			pack_iputl(sp->track[i]->note[j]->pos, fp);
			pack_iputl(sp->track[i]->note[j]->length, fp);
			pack_putc(sp->track[i]->note[j]->flags, fp);
		}
	}

	/* write lyric track */
	pack_iputl(sp->vocal_track->lyrics, fp);
	for(j = 0; j < sp->vocal_track->lyrics; j++)
	{
		pack_putc(sp->vocal_track->lyric[j]->note, fp);
		pack_iputl(sp->vocal_track->lyric[j]->pos, fp);
		pack_iputl(sp->vocal_track->lyric[j]->length, fp);
		pack_iputw(ustrlen(sp->vocal_track->lyric[j]->text), fp);
		pack_fwrite(sp->vocal_track->lyric[j]->text, ustrlen(sp->vocal_track->lyric[j]->text), fp);
	}
	pack_iputl(sp->vocal_track->lines, fp);
	for(j = 0; j < sp->vocal_track->lines; j++)
	{
		pack_iputl(sp->vocal_track->line[j].start_pos, fp);
		pack_iputl(sp->vocal_track->line[j].end_pos, fp);
		pack_iputl(sp->vocal_track->line[j].flags, fp);
	}

	/* write bookmarks */
	for(i = 0; i < 10; i++)
	{
		pack_iputl(sp->bookmark_pos[i], fp);
	}

	/* write fret catalog */
	pack_iputl(sp->catalog->entries, fp);
	for(i = 0; i < sp->catalog->entries; i++)
	{
		pack_putc(sp->catalog->entry[i].track, fp);
		pack_putc(sp->catalog->entry[i].type, fp);
		pack_iputl(sp->catalog->entry[i].start_pos, fp);
		pack_iputl(sp->catalog->entry[i].end_pos, fp);
	}
	pack_fclose(fp);
	return 1;
}

int eof_load_song_pf(EOF_SONG * sp, PACKFILE * fp)
{
	int i, j, b, c, tl;

	/* read file revision number */
	sp->tags->revision = pack_igetl(fp);

	/* read song info */
	tl = pack_igetw(fp);
	pack_fread(sp->tags->artist, tl, fp);
	sp->tags->artist[tl] = 0;
	tl = pack_igetw(fp);
	pack_fread(sp->tags->title, tl, fp);
	sp->tags->title[tl] = 0;
	tl = pack_igetw(fp);
	pack_fread(sp->tags->frettist, tl, fp);
	sp->tags->frettist[tl] = 0;
	tl = pack_igetw(fp);
	pack_fread(sp->tags->year, tl, fp);
	sp->tags->year[tl] = 0;
	tl = pack_igetw(fp);
	pack_fread(sp->tags->loading_text, tl, fp);
	sp->tags->loading_text[tl] = 0;
	sp->tags->lyrics = pack_getc(fp);
	sp->tags->eighth_note_hopo = pack_getc(fp);

	/* read OGG data */
	sp->tags->oggs = pack_igetw(fp);
	for(i = 0; i < sp->tags->oggs; i++)
	{
		pack_fread(sp->tags->ogg[i].filename, 256, fp);
		sp->tags->ogg[i].midi_offset = pack_igetl(fp);
	}

	/* read INI settings */
	sp->tags->ini_settings = pack_igetw(fp);
	for(i = 0; i < sp->tags->ini_settings; i++)
	{
		pack_fread(sp->tags->ini_setting[i], 512, fp);
	}

	/* read beat info */
	b = pack_igetl(fp);
	if(!eof_song_resize_beats(sp, b))
	{
		return 0;
	}
	for(i = 0; i < b; i++)
	{
		sp->beat[i]->ppqn = pack_igetl(fp);
		sp->beat[i]->pos = pack_igetl(fp);
		sp->beat[i]->flags = pack_igetl(fp);
	}

	/* read events info */
	b = pack_igetl(fp);
	eof_song_resize_text_events(sp, b);
	for(i = 0; i < b; i++)
	{
		pack_fread(sp->text_event[i]->text, 256, fp);
		sp->text_event[i]->beat = pack_igetl(fp);
	}

	/* read tracks */
	for(i = 0; i < EOF_MAX_TRACKS; i++)
	{

		/* read solo sections */
		sp->track[i]->solos = pack_igetw(fp);
		for(j = 0; j < sp->track[i]->solos; j++)
		{
			sp->track[i]->solo[j].start_pos = pack_igetl(fp);
			sp->track[i]->solo[j].end_pos = pack_igetl(fp);
		}
		if(sp->track[i]->solos < 0)
		{
			sp->track[i]->solos = 0;
		}

		/* read star power sections */
		sp->track[i]->star_power_paths = pack_igetw(fp);
		for(j = 0; j < sp->track[i]->star_power_paths; j++)
		{
			sp->track[i]->star_power_path[j].start_pos = pack_igetl(fp);
			sp->track[i]->star_power_path[j].end_pos = pack_igetl(fp);
		}
		if(sp->track[i]->star_power_paths < 0)
		{
			sp->track[i]->star_power_paths = 0;
		}

		/* read notes */
		b = pack_igetl(fp);
		eof_track_resize(sp->track[i], b);
		for(j = 0; j < b; j++)
		{
			sp->track[i]->note[j]->type = pack_getc(fp);
			sp->track[i]->note[j]->note = pack_getc(fp);
			sp->track[i]->note[j]->pos = pack_igetl(fp);
			sp->track[i]->note[j]->length = pack_igetl(fp);
			sp->track[i]->note[j]->flags = pack_getc(fp);
		}
	}

	/* read lyric track */
	b = pack_igetl(fp);
	eof_vocal_track_resize(sp->vocal_track, b);
	for(j = 0; j < b; j++)
	{
		sp->vocal_track->lyric[j]->note = pack_getc(fp);
		sp->vocal_track->lyric[j]->pos = pack_igetl(fp);
		sp->vocal_track->lyric[j]->length = pack_igetl(fp);
		c = pack_igetw(fp);
		pack_fread(sp->vocal_track->lyric[j]->text, c, fp);
		sp->vocal_track->lyric[j]->text[c] = '\0';
	}
	sp->vocal_track->lines = pack_igetl(fp);
	for(j = 0; j < sp->vocal_track->lines; j++)
	{
		sp->vocal_track->line[j].start_pos = pack_igetl(fp);
		sp->vocal_track->line[j].end_pos = pack_igetl(fp);
		sp->vocal_track->line[j].flags = pack_igetl(fp);
	}

	/* read bookmarks */
	for(i = 0; i < 10; i++)
	{
		sp->bookmark_pos[i] = pack_igetl(fp);
	}

	/* read fret catalog */
	sp->catalog->entries = pack_igetl(fp);
	for(i = 0; i < sp->catalog->entries; i++)
	{
		sp->catalog->entry[i].track = pack_getc(fp);
		sp->catalog->entry[i].type = pack_getc(fp);
		sp->catalog->entry[i].start_pos = pack_igetl(fp);
		sp->catalog->entry[i].end_pos = pack_igetl(fp);
	}

	return 1;
}

EOF_SONG * eof_load_song(const char * fn)
{
	PACKFILE * fp;
	EOF_SONG * sp;
	char header[16] = {'E', 'O', 'F', 'S', 'O', 'N', 'F', 0};
	char rheader[16];
	int i;

	fp = pack_fopen(fn, "r");
	if(!fp)
	{
		return 0;
	}
	pack_fread(rheader, 16, fp);
	if(!ustricmp(rheader, header))
	{
		sp = eof_create_song();
		if(!sp)
		{
			return NULL;
		}
		if(!eof_load_song_pf(sp, fp))
		{
			return NULL;
		}
	}
	else
	{
		sp = eof_load_notes_legacy(fp, rheader[6]);
	}
	pack_fclose(fp);

	/* select correct OGG */
	if(sp)
	{
		for(i = 0; i < sp->tags->oggs; i++)
		{
			if(sp->tags->ogg[i].midi_offset == sp->beat[0]->pos)
			{
				eof_selected_ogg = i;
				break;
			}
		}
	}

	return sp;
}

EOF_NOTE * eof_track_add_note(EOF_TRACK * tp)
{
	tp->note[tp->notes] = malloc(sizeof(EOF_NOTE));
	if(tp->note[tp->notes])
	{
		memset(tp->note[tp->notes], 0, sizeof(EOF_NOTE));
		tp->notes++;
	}
	return tp->note[tp->notes - 1];
}

void eof_track_delete_note(EOF_TRACK * tp, int note)
{
	int i;

	free(tp->note[note]);
	for(i = note; i < tp->notes - 1; i++)
	{
		tp->note[i] = tp->note[i + 1];
	}
	tp->notes--;
}

void eof_track_sort_notes(EOF_TRACK * tp)
{
	qsort(tp->note, tp->notes, sizeof(EOF_NOTE *), eof_song_qsort_notes);
}

int eof_fixup_next_note(EOF_TRACK * tp, int note)
{
	int i;

	for(i = note + 1; i < tp->notes; i++)
	{
		if(tp->note[i]->type == tp->note[note]->type)
		{
			return i;
		}
	}
	return -1;
}

/* find and mark crazy notes, use during MIDI import */
void eof_track_find_crazy_notes(EOF_TRACK * tp)
{
	int i;
	int next;

	for(i = 0; i < tp->notes; i++)
	{
		next = eof_fixup_next_note(tp, i);
		if(next >= 0)
		{
			if(tp->note[i]->pos + tp->note[i]->length > tp->note[next]->pos)
			{
				tp->note[i]->flags |= EOF_NOTE_FLAG_CRAZY;
			}
		}
	}
}

void eof_track_fixup_notes(EOF_TRACK * tp, int sel)
{
	int i;
	int next;

	if(!sel)
	{
		if(eof_selection.current < tp->notes)
		{
			eof_selection.multi[eof_selection.current] = 0;
		}
		eof_selection.current = EOF_MAX_NOTES - 1;
	}
	for(i = tp->notes - 1; i >= 0; i--)
	{

		/* fix selections */
		if(tp->note[i]->type == eof_note_type && tp->note[i]->pos == eof_selection.current_pos)
		{
			eof_selection.current = i;
		}
		if(tp->note[i]->type == eof_note_type && tp->note[i]->pos == eof_selection.last_pos)
		{
			eof_selection.last = i;
		}

		/* delete certain notes */
		if(tp->note[i]->note == 0 || (tp->note[i]->type < 0 || tp->note[i]->type > 4) || tp->note[i]->pos < eof_song->tags->ogg[eof_selected_ogg].midi_offset || tp->note[i]->pos >= eof_music_length)
		{
			eof_track_delete_note(tp, i);
		}

		else
		{
			/* make sure there are no 0-length notes */
			if(tp->note[i]->length <= 0)
			{
				tp->note[i]->length = 1;
			}

			/* make sure note doesn't extend past end of song */
			if(tp->note[i]->pos + tp->note[i]->length >= eof_music_length)
			{
				tp->note[i]->length = eof_music_length - tp->note[i]->pos;
			}

			/* compare this note to the next one of the same type
			   to make sure they don't overlap */
			next = eof_fixup_next_note(tp, i);
			if(next >= 0)
			{
				if(tp->note[i]->pos == tp->note[next]->pos)
				{
					tp->note[i]->note |= tp->note[next]->note;
					eof_track_delete_note(tp, next);
				}
				else if(!(tp->note[i]->flags & EOF_NOTE_FLAG_CRAZY) && tp->note[i]->pos + tp->note[i]->length > tp->note[next]->pos - 1)
				{
					tp->note[i]->length = tp->note[next]->pos - tp->note[i]->pos - 1;
				}
			}
		}
	}
	if(!sel)
	{
		if(eof_selection.current < tp->notes)
		{
			eof_selection.multi[eof_selection.current] = 1;
		}
	}
}

void eof_track_resize(EOF_TRACK * tp, int notes)
{
	int i;
	int oldnotes = tp->notes;
	if(notes > oldnotes)
	{
		for(i = oldnotes; i < notes; i++)
		{
			eof_track_add_note(tp);
		}
	}
	else if(notes < oldnotes)
	{
		for(i = notes; i < oldnotes; i++)
		{
			free(tp->note[i]);
			tp->notes--;
		}
	}
}

void eof_track_add_star_power(EOF_TRACK * tp, unsigned long start_pos, unsigned long end_pos)
{
	tp->star_power_path[tp->star_power_paths].start_pos = start_pos;
	tp->star_power_path[tp->star_power_paths].end_pos = end_pos;
	tp->star_power_paths++;
}

void eof_track_delete_star_power(EOF_TRACK * tp, int index)
{
	int i;

	for(i = index; i < tp->star_power_paths - 1; i++)
	{
		memcpy(&tp->star_power_path[i], &tp->star_power_path[i + 1], sizeof(EOF_STAR_POWER_ENTRY));
	}
	tp->star_power_paths--;
}

void eof_track_add_solo(EOF_TRACK * tp, unsigned long start_pos, unsigned long end_pos)
{
	tp->solo[tp->solos].start_pos = start_pos;
	tp->solo[tp->solos].end_pos = end_pos;
	tp->solos++;
}

void eof_track_delete_solo(EOF_TRACK * tp, int index)
{
	int i;

	for(i = index; i < tp->solos - 1; i++)
	{
		memcpy(&tp->solo[i], &tp->solo[i + 1], sizeof(EOF_SOLO_ENTRY));
	}
	tp->solos--;
}

EOF_LYRIC * eof_vocal_track_add_lyric(EOF_VOCAL_TRACK * tp)
{
	tp->lyric[tp->lyrics] = malloc(sizeof(EOF_LYRIC));
	if(tp->lyric[tp->lyrics])
	{
		memset(tp->lyric[tp->lyrics], 0, sizeof(EOF_LYRIC));
		tp->lyrics++;
	}
	return tp->lyric[tp->lyrics - 1];
}

void eof_vocal_track_delete_lyric(EOF_VOCAL_TRACK * tp, int lyric)
{
	int i;

	free(tp->lyric[lyric]);
	for(i = lyric; i < tp->lyrics - 1; i++)
	{
		tp->lyric[i] = tp->lyric[i + 1];
	}
	tp->lyrics--;
}

void eof_vocal_track_sort_lyrics(EOF_VOCAL_TRACK * tp)
{
	qsort(tp->lyric, tp->lyrics, sizeof(EOF_LYRIC *), eof_song_qsort_lyrics);
}

int eof_fixup_next_lyric(EOF_VOCAL_TRACK * tp, int lyric)
{
	int i;

	for(i = lyric + 1; i < tp->lyrics; i++)
	{
		return i;
	}
	return -1;
}

void eof_vocal_track_fixup_lyrics(EOF_VOCAL_TRACK * tp, int sel)
{
	int i, j;
	int lc;
	int next;

	if(!sel)
	{
		if(eof_selection.current < tp->lyrics)
		{
			eof_selection.multi[eof_selection.current] = 0;
		}
		eof_selection.current = EOF_MAX_NOTES - 1;
	}
	for(i = tp->lyrics - 1; i >= 0; i--)
	{

		/* fix selections */
		if(tp->lyric[i]->pos == eof_selection.current_pos)
		{
			eof_selection.current = i;
		}
		if(tp->lyric[i]->pos == eof_selection.last_pos)
		{
			eof_selection.last = i;
		}

		/* delete certain notes */
		if(tp->lyric[i]->pos < eof_song->tags->ogg[eof_selected_ogg].midi_offset || tp->lyric[i]->pos >= eof_music_length)
		{
			eof_vocal_track_delete_lyric(tp, i);
		}

		else
		{

			/* make sure there are no 0-length notes */
			if(tp->lyric[i]->length <= 0)
			{
				tp->lyric[i]->length = 1;
			}

			/* make sure note doesn't extend past end of song */
			if(tp->lyric[i]->pos + tp->lyric[i]->length >= eof_music_length)
			{
				tp->lyric[i]->length = eof_music_length - tp->lyric[i]->pos;
			}

			/* compare this note to the next one of the same type
			   to make sure they don't overlap */
			next = eof_fixup_next_lyric(tp, i);
			if(next >= 0)
			{
				if(tp->lyric[i]->pos == tp->lyric[next]->pos)
				{
					eof_vocal_track_delete_lyric(tp, next);
				}
				else if(tp->lyric[i]->pos + tp->lyric[i]->length > tp->lyric[next]->pos - 1)
				{
					tp->lyric[i]->length = tp->lyric[next]->pos - tp->lyric[i]->pos - 1;
				}
			}
		}
	}
	/* delete empty lines */
	for(i = tp->lines - 1; i >= 0; i--)
	{
		lc = 0;
		for(j = 0; j < tp->lyrics; j++)
		{
			if(tp->lyric[j]->pos >= tp->line[i].start_pos && tp->lyric[j]->pos <= tp->line[i].end_pos)
			{
				lc++;
			}
		}
		if(lc <= 0)
		{
			eof_vocal_track_delete_line(tp, i);
		}
	}
	if(!sel)
	{
		if(eof_selection.current < tp->lyrics)
		{
			eof_selection.multi[eof_selection.current] = 1;
		}
	}
}

void eof_vocal_track_resize(EOF_VOCAL_TRACK * tp, int lyrics)
{
	int i;
	int oldlyrics = tp->lyrics;
	if(lyrics > oldlyrics)
	{
		for(i = oldlyrics; i < lyrics; i++)
		{
			eof_vocal_track_add_lyric(tp);
		}
	}
	else if(lyrics < oldlyrics)
	{
		for(i = lyrics; i < oldlyrics; i++)
		{
			free(tp->lyric[i]);
			tp->lyrics--;
		}
	}
}

void eof_vocal_track_add_line(EOF_VOCAL_TRACK * tp, unsigned long start_pos, unsigned long end_pos)
{
	tp->line[tp->lines].start_pos = start_pos;
	tp->line[tp->lines].end_pos = end_pos;
	tp->line[tp->lines].flags = 0;	//Ensure that a blank flag status is initialized
	tp->lines++;
}

void eof_vocal_track_delete_line(EOF_VOCAL_TRACK * tp, int index)
{
	int i;

	for(i = index; i < tp->lines - 1; i++)
	{
		memcpy(&tp->line[i], &tp->line[i + 1], sizeof(EOF_LYRIC_LINE));
	}
	tp->lines--;
}

EOF_BEAT_MARKER * eof_song_add_beat(EOF_SONG * sp)
{
	sp->beat[sp->beats] = malloc(sizeof(EOF_BEAT_MARKER));
	if(sp)
	{
		sp->beat[sp->beats]->pos = 0;
		sp->beat[sp->beats]->ppqn = 500000;
		sp->beat[sp->beats]->flags = 0;
		sp->beats++;
	}
	return sp->beat[sp->beats - 1];
}

void eof_song_delete_beat(EOF_SONG * sp, int beat)
{
	int i;

	free(sp->beat[beat]);
	for(i = beat; i < sp->beats - 1; i++)
	{
		sp->beat[i] = sp->beat[i + 1];
	}
	sp->beats--;
}

int eof_song_resize_beats(EOF_SONG * sp, int beats)
{
	int i;
	int oldbeats = sp->beats;
	if(beats > oldbeats)
	{
		for(i = oldbeats; i < beats; i++)
		{
			if(!eof_song_add_beat(sp))
			{
				return 0;
			}
		}
	}
	else if(beats < oldbeats)
	{
		for(i = beats; i < oldbeats; i++)
		{
			free(sp->beat[i]);
			sp->beats--;
		}
	}
	return 1;
}

void eof_song_add_text_event(EOF_SONG * sp, int beat, char * text)
{
	sp->text_event[sp->text_events] = malloc(sizeof(EOF_TEXT_EVENT));
	if(sp->text_event[sp->text_events])
	{
		ustrcpy(sp->text_event[sp->text_events]->text, text);
		sp->text_event[sp->text_events]->beat = beat;
		sp->text_events++;
	}
}

void eof_song_delete_text_event(EOF_SONG * sp, int event)
{
	int i;
	free(sp->text_event[event]);
	for(i = event; i < sp->text_events - 1; i++)
	{
		sp->text_event[i] = sp->text_event[i + 1];
	}
	sp->text_events--;
}

void eof_song_move_text_events(EOF_SONG * sp, int beat, int offset)
{
	int i;

	for(i = 0; i < sp->text_events; i++)
	{
		if(sp->text_event[i]->beat >= beat)
		{
			sp->beat[sp->text_event[i]->beat]->flags = sp->beat[sp->text_event[i]->beat]->flags & EOF_BEAT_FLAG_ANCHOR;
			sp->text_event[i]->beat += offset;
			sp->beat[sp->text_event[i]->beat]->flags |= EOF_BEAT_FLAG_EVENTS;
		}
	}
}

void eof_song_resize_text_events(EOF_SONG * sp, int events)
{
	int i;
	int oldevents = sp->text_events;
	if(events > oldevents)
	{
		for(i = oldevents; i < events; i++)
		{
			eof_song_add_text_event(sp, 0, "");
		}
	}
	else if(events < oldevents)
	{
		for(i = events; i < oldevents; i++)
		{
			free(sp->text_event[i]);
			sp->text_events--;
		}
	}
}

void eof_sort_events(void)
{
	qsort(eof_song->text_event, eof_song->text_events, sizeof(EOF_TEXT_EVENT *), eof_song_qsort_events);
}

/* make sure notes don't overlap */
void eof_fixup_notes(void)
{
	int i, j;

	if(eof_selection.current < eof_song->track[eof_selected_track]->notes)
	{
		eof_selection.multi[eof_selection.current] = 0;
	}
	eof_selection.current = EOF_MAX_NOTES - 1;

	/* fix beats */
	if(eof_song->beat[0]->pos != eof_song->tags->ogg[eof_selected_ogg].midi_offset)
	{
		for(i = 0; i < eof_song->beats; i++)
		{
			eof_song->beat[i]->pos += eof_song->tags->ogg[eof_selected_ogg].midi_offset - eof_song->beat[0]->pos;
		}
	}

	for(j = 0; j < EOF_MAX_TRACKS; j++)
	{
		eof_track_fixup_notes(eof_song->track[j], j == eof_selected_track);
	}
}

void eof_sort_notes(void)
{
	int j;

	for(j = 0; j < EOF_MAX_TRACKS; j++)
	{
		eof_track_sort_notes(eof_song->track[j]);
	}
}

void eof_detect_difficulties(EOF_SONG * sp)
{
	int i;

	memset(eof_note_difficulties, 0, sizeof(int) * 4);
	eof_note_type_name[0][0] = ' ';
	eof_note_type_name[1][0] = ' ';
	eof_note_type_name[2][0] = ' ';
	eof_note_type_name[3][0] = ' ';
	eof_note_type_name[4][0] = ' ';
	eof_vocal_tab_name[0][0] = ' ';
	for(i = 0; i < sp->track[eof_selected_track]->notes; i++)
	{
		if(sp->track[eof_selected_track]->note[i]->type >= 0 && sp->track[eof_selected_track]->note[i]->type < 5)
		{
			eof_note_difficulties[(int)sp->track[eof_selected_track]->note[i]->type] = 1;
			eof_note_type_name[(int)sp->track[eof_selected_track]->note[i]->type][0] = '*';
		}
	}
	if(sp->vocal_track->lyrics)
	{
		eof_vocal_tab_name[0][0] = '*';
	}
}
