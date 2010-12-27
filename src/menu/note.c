#include <allegro.h>
#include "../agup/agup.h"
#include "../undo.h"
#include "../dialog.h"
#include "../dialog/proc.h"
#include "../player.h"
#include "../utility.h"
#include "../foflc/Lyric_storage.h"
#include "../main.h"
#include "note.h"
#include "ctype.h"

char eof_solo_menu_mark_text[32] = "&Mark";
char eof_star_power_menu_mark_text[32] = "&Mark";
char eof_lyric_line_menu_mark_text[32] = "&Mark";

MENU eof_solo_menu[] =
{
    {eof_solo_menu_mark_text, eof_menu_solo_mark, NULL, 0, NULL},
    {"&Remove", eof_menu_solo_unmark, NULL, 0, NULL},
    {"&Erase All", eof_menu_solo_erase_all, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_star_power_menu[] =
{
    {eof_star_power_menu_mark_text, eof_menu_star_power_mark, NULL, 0, NULL},
    {"&Remove", eof_menu_star_power_unmark, NULL, 0, NULL},
    {"&Erase All", eof_menu_star_power_erase_all, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_lyric_line_menu[] =
{
    {eof_lyric_line_menu_mark_text, eof_menu_lyric_line_mark, NULL, 0, NULL},
    {"&Remove", eof_menu_lyric_line_unmark, NULL, 0, NULL},
    {"&Erase All", eof_menu_lyric_line_erase_all, NULL, 0, NULL},
    {"", NULL, NULL, 0, NULL},
    {"&Toggle Overdrive", eof_menu_lyric_line_toggle_overdrive, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_hopo_menu[] =
{
    {"&Auto", eof_menu_hopo_auto, NULL, 0, NULL},
    {"&Force On", eof_menu_hopo_force_on, NULL, 0, NULL},
    {"Force &Off", eof_menu_hopo_force_off, NULL, 0, NULL},
    {"&Cycle On/Off/Auto\tH", eof_menu_hopo_cycle, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_note_toggle_menu[] =
{
    {"&Green\tCtrl+1", eof_menu_note_toggle_green, NULL, 0, NULL},
    {"&Red\tCtrl+2", eof_menu_note_toggle_red, NULL, 0, NULL},
    {"&Yellow\tCtrl+3", eof_menu_note_toggle_yellow, NULL, 0, NULL},
    {"&Blue\tCtrl+4", eof_menu_note_toggle_blue, NULL, 0, NULL},
    {"&Purple\tCtrl+5", eof_menu_note_toggle_purple, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_note_freestyle_menu[] =
{
    {"&On", eof_menu_set_freestyle_on, NULL, 0, NULL},
    {"O&ff", eof_menu_set_freestyle_off, NULL, 0, NULL},
    {"&Toggle\tF", eof_menu_toggle_freestyle, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_note_prodrum_menu[] =
{
    {"Toggle &Yellow cymbal\tCtrl+Y", eof_menu_note_toggle_rb3_cymbal_yellow, NULL, 0, NULL},
    {"Toggle &Blue cymbal\tCtrl+B", eof_menu_note_toggle_rb3_cymbal_blue, NULL, 0, NULL},
    {"Toggle &Green cymbal\tCtrl+G", eof_menu_note_toggle_rb3_cymbal_green, NULL, 0, NULL},
    {"Mark as &Non cymbal", eof_menu_note_remove_cymbal, NULL, 0, NULL},
    {"&Mark new notes as cymbals", eof_menu_note_default_cymbal, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_note_proguitar_menu[] =
{
    {"Edit pro guitar &note\tN", eof_menu_note_edit_pro_guitar_note, NULL, 0, NULL},
    {"Toggle tapping\tCtrl+T", eof_menu_note_toggle_tapping, NULL, 0, NULL},
    {"Mark as non &Tapping", eof_menu_note_remove_tapping, NULL, 0, NULL},
    {"Toggle Slide &Up\tCtrl+Up", eof_menu_note_toggle_slide_up, NULL, 0, NULL},
    {"Toggle Slide &Down\tCtrl+Down", eof_menu_note_toggle_slide_down, NULL, 0, NULL},
    {"Mark as non &Slide", eof_menu_note_remove_slide, NULL, 0, NULL},
    {"Toggle &Palm muting", eof_menu_note_toggle_palm_muting, NULL, 0, NULL},
    {"Mark as non palm &Muting", eof_menu_note_remove_palm_muting, NULL, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

MENU eof_note_menu[] =
{
    {"&Toggle", NULL, eof_note_toggle_menu, 0, NULL},
    {"Transpose Up\tUp", eof_menu_note_transpose_up, NULL, 0, NULL},
    {"Transpose Down\tDown", eof_menu_note_transpose_down, NULL, 0, NULL},
    {"", NULL, NULL, 0, NULL},
    {"&Resnap", eof_menu_note_resnap, NULL, 0, NULL},
    {"", NULL, NULL, 0, NULL},
    {"Ed&it Lyric\tL", eof_edit_lyric_dialog, NULL, 0, NULL},
    {"Split Lyric\tShift+L", eof_menu_split_lyric, NULL, 0, NULL},
    {"", NULL, NULL, 0, NULL},
    {"Toggle &Crazy\tT", eof_menu_note_toggle_crazy, NULL, 0, NULL},
    {"", NULL, NULL, 0, NULL},
    {"&Solos", NULL, eof_solo_menu, 0, NULL},
    {"Star &Power", NULL, eof_star_power_menu, 0, NULL},
    {"&Lyric Lines", NULL, eof_lyric_line_menu, 0, NULL},
    {"", NULL, NULL, 0, NULL},
    {"&HOPO", NULL, eof_hopo_menu, 0, NULL},
    {"", NULL, NULL, 0, NULL},
    {"Delete\tDel", eof_menu_note_delete, NULL, 0, NULL},
    {"Display semitones as flat", eof_display_flats_menu, NULL, 0, NULL},
    {"&Freestyle", NULL, eof_note_freestyle_menu, 0, NULL},
    {"Toggle &Expert+ bass drum\tCtrl+E", eof_menu_note_toggle_double_bass, NULL, 0, NULL},
    {"Pro &Drum mode notation", NULL, eof_note_prodrum_menu, 0, NULL},
    {"Pro &Guitar mode notation", NULL, eof_note_proguitar_menu, 0, NULL},
    {NULL, NULL, NULL, 0, NULL}
};

DIALOG eof_lyric_dialog[] =
{
   /* (proc)         (x)  (y)  (w)  (h)  (fg) (bg) (key) (flags) (d1) (d2) (dp)           (dp2) (dp3) */
   { d_agup_window_proc,    0,  48,  204 + 110, 106, 2,   23,  0,    0,      0,   0,   "Edit Lyric",               NULL, NULL },
   { d_agup_text_proc,   12,  84,  64,  8,  2,   23,  0,    0,      0,   0,   "Text:",         NULL, NULL },
   { d_agup_edit_proc,   48, 80,  144 + 110,  20,  2,   23,  0,    0,      255,   0,   eof_etext,           NULL, NULL },
   { d_agup_button_proc, 12 + 55,  112, 84,  28, 2,   23,  '\r',    D_EXIT, 0,   0,   "OK",               NULL, NULL },
   { d_agup_button_proc, 108 + 55, 112, 78,  28, 2,   23,  0,    D_EXIT, 0,   0,   "Cancel",           NULL, NULL },
   { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL }
};

DIALOG eof_split_lyric_dialog[] =
{
   /* (proc)         (x)  (y)  (w)  (h)  (fg) (bg) (key) (flags) (d1) (d2) (dp)           (dp2) (dp3) */
   { d_agup_window_proc,    0,  48,  204 + 110, 106, 2,   23,  0,    0,      0,   0,   "Split Lyric",               NULL, NULL },
   { d_agup_text_proc,   12,  84,  64,  8,  2,   23,  0,    0,      0,   0,   "Text:",         NULL, NULL },
   { d_agup_edit_proc,   48, 80,  144 + 110,  20,  2,   23,  0,    0,      255,   0,   eof_etext,           NULL, NULL },
   { d_agup_button_proc, 12 + 55,  112, 84,  28, 2,   23,  '\r',    D_EXIT, 0,   0,   "OK",               NULL, NULL },
   { d_agup_button_proc, 108 + 55, 112, 78,  28, 2,   23,  0,    D_EXIT, 0,   0,   "Cancel",           NULL, NULL },
   { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL }
};

void eof_prepare_note_menu(void)
{
	int selected = 0;
	int vselected = 0;
	int insp = 0, insolo = 0, inll = 0;
	int spstart = -1;
	int spend = -1;
	int spp = 0;
	int ssstart = -1;
	int ssend = -1;
	int ssp = 0;
	int llstart = -1;
	int llend = -1;
	int llp = 0;
	unsigned long i, j;
	unsigned long tracknum;
	int sel_start = eof_music_length, sel_end = 0;
	int firstnote = 0, lastnote;
	EOF_STAR_POWER_ENTRY *starpowerptr = NULL;
	EOF_SOLO_ENTRY *soloptr = NULL;

	if(eof_song && eof_song_loaded)
	{
		tracknum = eof_song->track[eof_selected_track]->tracknum;
		if(eof_vocals_selected)
		{	//PART VOCALS SELECTED
			for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
			{
				if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i])
				{
					if(eof_song->vocal_track[tracknum]->lyric[i]->pos < sel_start)
					{
						sel_start = eof_song->vocal_track[tracknum]->lyric[i]->pos;
					}
					if(eof_song->vocal_track[tracknum]->lyric[i]->pos > sel_end)
					{
						sel_end = eof_song->vocal_track[tracknum]->lyric[i]->pos + eof_song->vocal_track[tracknum]->lyric[i]->length;
					}
				}
				if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i])
				{
					selected++;
					vselected++;
				}
				if(firstnote < 0)
				{
					firstnote = i;
				}
				lastnote = i;
			}
			for(j = 0; j < eof_song->vocal_track[tracknum]->lines; j++)
			{
				if((sel_end >= eof_song->vocal_track[tracknum]->line[j].start_pos) && (sel_start <= eof_song->vocal_track[tracknum]->line[j].end_pos))
				{
					inll = 1;
					llstart = sel_start;
					llend = sel_end;
					llp = j;
				}
			}
		}
		else
		{	//PART VOCALS NOT SELECTED
			for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
			{	//For each note in the active track
				if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
				{
					if(eof_get_note_pos(eof_song, eof_selected_track, i) < sel_start)
					{
						sel_start = eof_get_note_pos(eof_song, eof_selected_track, i);
					}
					if(eof_get_note_pos(eof_song, eof_selected_track, i) > sel_end)
					{
						sel_end = eof_get_note_pos(eof_song, eof_selected_track, i) + eof_get_note_length(eof_song, eof_selected_track, i);
					}
				}
				if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
				{
					selected++;
					if(eof_get_note_note(eof_song, eof_selected_track, i))
					{
						vselected++;
					}
				}
				if(eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type)
				{
					if(firstnote < 0)
					{
						firstnote = i;
					}
					lastnote = i;
				}
			}
			for(j = 0; j < eof_get_num_star_power_paths(eof_song, eof_selected_track); j++)
			{	//For each star power path in the active track
				starpowerptr = eof_get_star_power_path(eof_song, eof_selected_track, j);
				if((sel_end >= starpowerptr->start_pos) && (sel_start <= starpowerptr->end_pos))
				{
					insp = 1;
					spstart = sel_start;
					spend = sel_end;
					spp = j;
				}
			}
			for(j = 0; j < eof_get_num_solos(eof_song, eof_selected_track); j++)
			{	//For each solo section in the active track
				soloptr = eof_get_solo(eof_song, eof_selected_track, j);
				if((sel_end >= soloptr->start_pos) && (sel_start <= soloptr->end_pos))
				{
					insolo = 1;
					ssstart = sel_start;
					ssend = sel_end;
					ssp = j;
				}
			}
		}
		vselected = eof_count_selected_notes(NULL, 1);
		if(vselected)
		{	//ONE OR MORE NOTES/LYRICS SELECTED
			/* star power mark */
			starpowerptr = eof_get_star_power_path(eof_song, eof_selected_track, spp);
			if((starpowerptr != NULL) && (spstart == starpowerptr->start_pos) && (spend == starpowerptr->end_pos))
			{
				eof_star_power_menu[0].flags = D_DISABLED;
			}
			else
			{
				eof_star_power_menu[0].flags = 0;
			}

			/* solo mark */
			soloptr = eof_get_solo(eof_song, eof_selected_track, ssp);
			if((soloptr != NULL) && (ssstart == soloptr->start_pos) && (ssend == soloptr->end_pos))
			{
				eof_solo_menu[0].flags = D_DISABLED;
			}
			else
			{
				eof_solo_menu[0].flags = 0;
			}

			/* lyric line mark */
			if((llstart == eof_song->vocal_track[0]->line[llp].start_pos) && (llend == eof_song->vocal_track[0]->line[llp].end_pos))
			{
				eof_lyric_line_menu[0].flags = D_DISABLED;
			}
			else
			{
				eof_lyric_line_menu[0].flags = 0;
			}

			eof_note_menu[11].flags = 0; // solos
			eof_note_menu[12].flags = 0; // star power
		}
		else
		{	//NO NOTES/LYRICS SELECTED
			eof_star_power_menu[0].flags = D_DISABLED; // star power mark
			eof_solo_menu[0].flags = D_DISABLED; // solo mark
			eof_lyric_line_menu[0].flags = D_DISABLED; // lyric line mark
			eof_note_menu[6].flags = D_DISABLED; // edit lyric
			eof_note_menu[11].flags = D_DISABLED; // solos
			eof_note_menu[12].flags = D_DISABLED; // star power
		}

		/* star power remove */
		if(insp)
		{
			eof_star_power_menu[1].flags = 0;
			ustrcpy(eof_star_power_menu_mark_text, "Re-&Mark");
		}
		else
		{
			eof_star_power_menu[1].flags = D_DISABLED;
			ustrcpy(eof_star_power_menu_mark_text, "&Mark");
		}

		/* solo remove */
		if(insolo)
		{
			eof_solo_menu[1].flags = 0;
			ustrcpy(eof_solo_menu_mark_text, "Re-&Mark");
		}
		else
		{
			eof_solo_menu[1].flags = D_DISABLED;
			ustrcpy(eof_solo_menu_mark_text, "&Mark");
		}

		/* lyric line */
		if(inll)
		{
			eof_lyric_line_menu[1].flags = 0; // remove
			eof_lyric_line_menu[4].flags = 0; // toggle overdrive
			ustrcpy(eof_lyric_line_menu_mark_text, "Re-&Mark");
		}
		else
		{
			eof_lyric_line_menu[1].flags = D_DISABLED; // remove
			eof_lyric_line_menu[4].flags = D_DISABLED; // toggle overdrive
			ustrcpy(eof_lyric_line_menu_mark_text, "&Mark");
		}

		/* star power erase all */
		if(eof_get_num_star_power_paths(eof_song, eof_selected_track) > 0)
		{	//If there are one or more star power paths in the active track
			eof_star_power_menu[2].flags = 0;
		}
		else
		{
			eof_star_power_menu[2].flags = D_DISABLED;
		}

		/* solo erase all */
		if(eof_get_num_solos(eof_song, eof_selected_track) > 0)
		{	//If there are one or more solo paths in the active track
			eof_solo_menu[2].flags = 0;
		}
		else
		{
			eof_solo_menu[2].flags = D_DISABLED;
		}

		/* lyric lines erase all */
		if(eof_song->vocal_track[0]->lines > 0)
		{
			eof_lyric_line_menu[2].flags = 0;
		}
		else
		{
			eof_lyric_line_menu[2].flags = D_DISABLED;
		}

		/* resnap */
		if(eof_snap_mode == EOF_SNAP_OFF)
		{
			eof_note_menu[4].flags = D_DISABLED;
		}
		else
		{
			eof_note_menu[4].flags = 0;
		}

		if(eof_vocals_selected)
		{	//PART VOCALS SELECTED
			eof_note_menu[0].flags = D_DISABLED; // toggle
			eof_note_menu[1].flags = D_DISABLED; // transpose up
			eof_note_menu[2].flags = D_DISABLED; // transpose down

			if((eof_selection.current < eof_song->vocal_track[tracknum]->lyrics) && (vselected == 1))
			{	//Only enable edit and split lyric if only one lyric is selected
				eof_note_menu[6].flags = 0; // edit lyric
				eof_note_menu[7].flags = 0; // split lyric
			}
			else
			{
				eof_note_menu[6].flags = D_DISABLED; // edit lyric
				eof_note_menu[7].flags = D_DISABLED; // split lyric
			}
			eof_note_menu[9].flags = D_DISABLED; // toggle crazy
			eof_note_menu[11].flags = D_DISABLED; // solos
			eof_note_menu[12].flags = D_DISABLED; // star power
			eof_note_menu[15].flags = D_DISABLED; // HOPO

			/* lyric lines */
			if((eof_song->vocal_track[tracknum]->lines > 0) || vselected)
			{
				eof_note_menu[13].flags = 0;	// lyric lines
			}

			if(vselected)
			{
				eof_note_menu[19].flags = 0; // freestyle submenu
			}

			eof_note_menu[20].flags = D_DISABLED;	//Disable toggle Expert+ bass drum
			eof_note_menu[21].flags = D_DISABLED;	//Disable pro drum mode menu
		}
		else
		{	//PART VOCALS NOT SELECTED
			eof_note_menu[0].flags = 0; // toggle

			/* transpose up */
			if(eof_transpose_possible(-1))
			{
				eof_note_menu[1].flags = 0;
			}
			else
			{
				eof_note_menu[1].flags = D_DISABLED;
			}

			/* transpose down */
			if(eof_transpose_possible(1))
			{
				eof_note_menu[2].flags = 0;
			}
			else
			{
				eof_note_menu[2].flags = D_DISABLED;
			}

			eof_note_menu[6].flags = D_DISABLED; // edit lyric
			eof_note_menu[7].flags = D_DISABLED; // split lyric

			/* toggle crazy , toggle Expert+ bass drum*/
			if(eof_selected_track != EOF_TRACK_DRUM)
			{	//When PART DRUMS is not active
				eof_note_menu[9].flags = 0;				//Enable toggle crazy
				eof_note_menu[21].flags = D_DISABLED;	//Disable pro drum mode menu
			}
			else
			{	//When PART DRUMS is active
				eof_note_menu[9].flags = D_DISABLED;	//Disable toggle crazy
				eof_note_menu[21].flags = 0;			//Enable pro drum mode menu
			}

			if((eof_selected_track == EOF_TRACK_DRUM) && (eof_note_type == EOF_NOTE_AMAZING))
				eof_note_menu[20].flags = 0;			//Enable toggle Expert+ bass drum only on Expert Drums
			else
				eof_note_menu[20].flags = D_DISABLED;	//Otherwise disable the menu item

			/* solos */
			if(selected)
			{
				eof_note_menu[11].flags = 0;
			}
			else
			{
				eof_note_menu[11].flags = D_DISABLED;
			}

			/* star power */
			if(selected)
			{
				eof_note_menu[12].flags = 0;
			}
			else
			{
				eof_note_menu[12].flags = D_DISABLED;
			}

			eof_note_menu[13].flags = D_DISABLED; // lyric lines

			/* HOPO */
			if(eof_selected_track != EOF_TRACK_DRUM)
			{
				eof_note_menu[15].flags = 0;
			}
			else
			{
				eof_note_menu[15].flags = D_DISABLED;
			}

			eof_note_menu[19].flags = D_DISABLED; // freestyle submenu

			/* Pro Guitar mode notation> */
			if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
			{	//If the active track is a pro guitar track
				eof_note_menu[22].flags = 0;			//Enable the Pro Guitar mode notation submenu
			}
			else
			{
				eof_note_menu[22].flags = D_DISABLED;	//Otherwise disable the submenu
			}
		}
	}
}

int eof_menu_note_transpose_up(void)
{
	unsigned long i, j;
	unsigned long max = 31;	//This represents the highest valid note bitmask, based on the current track options (including open bass strumming)
	unsigned long flags, note, tracknum;

	if(!eof_transpose_possible(-1))
	{
		return 1;
	}
	if(eof_vocals_selected)
	{
		eof_prepare_undo(EOF_UNDO_TYPE_LYRIC_NOTE);	//Perform a cumulative undo for lyric pitch transpose operations
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each lyric in the active track
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i] && (note != EOF_LYRIC_PERCUSSION))
			{
				note++;
				eof_set_note_note(eof_song, eof_selected_track, i, note);
			}
		}
	}
	else
	{
		if(eof_open_bass_enabled() || (eof_count_track_lanes(eof_song, eof_selected_track) > 5))
		{	//If open bass is enabled, or the track has more than 5 lanes, lane 6 is valid for use
			max = 63;
		}
		eof_prepare_undo(EOF_UNDO_TYPE_NONE);
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each note in the active track
			if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
			{	//If the note is in the same track as the selected notes, is selected and is the same difficulty as the selected notes
				note = eof_get_note_note(eof_song, eof_selected_track, i);
				note = (note << 1) & max;
				if((eof_selected_track == EOF_TRACK_BASS) && eof_open_bass_enabled() && (note & 32))
				{	//If open bass is enabled, and this transpose operation resulted in a bass guitar gem in lane 6
					flags = eof_get_note_flags(eof_song, eof_selected_track, i);
					eof_set_note_note(eof_song, eof_selected_track, i, 32);		//Clear all lanes except lane 6
					flags &= ~(EOF_NOTE_FLAG_CRAZY);	//Clear the crazy flag, which is invalid for open strum notes
					flags &= ~(EOF_NOTE_FLAG_F_HOPO);	//Clear the HOPO flags, which are invalid for open strum notes
					flags &= ~(EOF_NOTE_FLAG_NO_HOPO);
					eof_set_note_flags(eof_song, eof_selected_track, i, flags);
				}
				eof_set_note_note(eof_song, eof_selected_track, i, note);
				if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
				{	//If a pro guitar note was tranposed, move the fret values accordingly
					tracknum = eof_song->track[eof_selected_track]->tracknum;
					for(j = 15; j > 0; j--)
					{	//For the upper 15 frets
					eof_song->pro_guitar_track[tracknum]->note[i]->frets[j] = eof_song->pro_guitar_track[tracknum]->note[i]->frets[j-1];	//Cycle fret values up from lower lane
					}
					eof_song->pro_guitar_track[tracknum]->note[i]->frets[0] = 0xFF;	//Re-initialize lane 1 to the default of (muted)
				}
			}
		}
	}
	return 1;
}

int eof_menu_note_transpose_down(void)
{
	unsigned long i, j;
	unsigned long flags, note, tracknum;

	if(!eof_transpose_possible(1))
	{
		return 1;
	}
	if(eof_vocals_selected)
	{
		eof_prepare_undo(EOF_UNDO_TYPE_LYRIC_NOTE);	//Perform a cumulative undo for lyric pitch transpose operations
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each lyric in the active track
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i] && (note != EOF_LYRIC_PERCUSSION))
			{
				note--;
				eof_set_note_note(eof_song, eof_selected_track, i, note);
			}
		}
	}
	else
	{
		eof_prepare_undo(EOF_UNDO_TYPE_NONE);
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each note in the active track
			if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
			{	//If the note is in the same track as the selected notes, is selected and is the same difficulty as the selected notes
				note = eof_get_note_note(eof_song, eof_selected_track, i);
				note = (note >> 1) & 63;
				eof_set_note_note(eof_song, eof_selected_track, i, note);
				if((eof_selected_track == EOF_TRACK_BASS) && eof_open_bass_enabled() && (note & 1))
				{	//If open bass is enabled, and this tranpose operation resulted in a bass guitar gem in lane 1
					flags = eof_get_note_flags(eof_song, eof_selected_track, i);
					flags &= ~(EOF_NOTE_FLAG_F_HOPO);	//Clear the forced HOPO on flag, which conflicts with open bass strum notation
					eof_set_note_flags(eof_song, eof_selected_track, i, flags);
				}
				if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
				{	//If a pro guitar note was tranposed, move the fret values accordingly
					tracknum = eof_song->track[eof_selected_track]->tracknum;
					for(j = 0; j < 15; j++)
					{	//For the lower 15 frets
						eof_song->pro_guitar_track[tracknum]->note[i]->frets[j] = eof_song->pro_guitar_track[tracknum]->note[i]->frets[j+1];	//Cycle fret values down from upper lane
					}
					eof_song->pro_guitar_track[tracknum]->note[i]->frets[15] = 0xFF;	//Re-initialize lane 15 to the default of (muted)
				}
			}
		}
	}
	return 1;
}

int eof_menu_note_transpose_up_octave(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_transpose_possible(-12))	//If lyric cannot move up one octave
		return 1;
	if(!eof_vocals_selected)			//If PART VOCALS is not active
		return 1;

	eof_prepare_undo(EOF_UNDO_TYPE_LYRIC_NOTE);	//Perform a cumulative undo for lyric pitch transpose operations

	for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
		if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i] && (eof_song->vocal_track[tracknum]->lyric[i]->note != EOF_LYRIC_PERCUSSION))
			eof_song->vocal_track[tracknum]->lyric[i]->note+=12;

	return 1;
}

int eof_menu_note_transpose_down_octave(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_transpose_possible(12))		//If lyric cannot move down one octave
		return 1;
	if(!eof_vocals_selected)		//If PART VOCALS is not active
		return 1;


	eof_prepare_undo(EOF_UNDO_TYPE_LYRIC_NOTE);	//Perform a cumulative undo for lyric pitch transpose operations

	for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
		if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i] && (eof_song->vocal_track[tracknum]->lyric[i]->note != EOF_LYRIC_PERCUSSION))
			eof_song->vocal_track[tracknum]->lyric[i]->note-=12;

	return 1;
}

int eof_menu_note_resnap_vocal(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	int oldnotes = eof_song->vocal_track[tracknum]->lyrics;

	if(!eof_vocals_selected)
		return 1;

	if(eof_snap_mode == EOF_SNAP_OFF)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
	{
		if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i])
		{

			/* snap the note itself */
			eof_snap_logic(&eof_tail_snap, eof_song->vocal_track[tracknum]->lyric[i]->pos);
			eof_song->vocal_track[tracknum]->lyric[i]->pos = eof_tail_snap.pos;
		}
	}
	eof_track_fixup_notes(eof_song, eof_selected_track, 1);
	if(oldnotes != eof_song->vocal_track[tracknum]->lyrics)
	{
		allegro_message("Warning! Some lyrics snapped to the same position and were automatically combined.");
	}
	return 1;
}

int eof_menu_note_resnap(void)
{
	if(eof_vocals_selected)
	{
		return eof_menu_note_resnap_vocal();
	}
	unsigned long i;
	unsigned long oldnotes = eof_track_get_size(eof_song, eof_selected_track);

	if(eof_snap_mode == EOF_SNAP_OFF)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			/* snap the note itself */
			eof_snap_logic(&eof_tail_snap, eof_get_note_pos(eof_song, eof_selected_track, i));
			eof_set_note_pos(eof_song, eof_selected_track, i, eof_tail_snap.pos);

			/* snap the tail */
			eof_snap_logic(&eof_tail_snap, eof_get_note_pos(eof_song, eof_selected_track, i) + eof_get_note_length(eof_song, eof_selected_track, i));
			eof_snap_length_logic(&eof_tail_snap);
			if(eof_get_note_length(eof_song, eof_selected_track, i) > 1)
			{
				eof_snap_logic(&eof_tail_snap, eof_get_note_pos(eof_song, eof_selected_track, i) + eof_get_note_length(eof_song, eof_selected_track, i));
				eof_note_set_tail_pos(eof_song, eof_selected_track, i, eof_tail_snap.pos);
			}
		}
	}
	eof_track_fixup_notes(eof_song, eof_selected_track, 1);
	if(oldnotes != eof_track_get_size(eof_song, eof_selected_track))
	{
		allegro_message("Warning! Some notes snapped to the same position and were automatically combined.");
	}
	eof_determine_hopos();
	return 1;
}

int eof_menu_note_delete_vocal(void)
{
	unsigned long i, d = 0;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_vocals_selected)
		return 1;

	for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
	{
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{	//Add logic to match the selected lyric type (set) in preparation for supporting vocal harmonies
			d++;
		}
	}
	if(d)
	{
		eof_prepare_undo(EOF_UNDO_TYPE_NOTE_SEL);
		for(i = eof_track_get_size(eof_song, eof_selected_track); i > 0; i--)
		{
			if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i-1] && (eof_get_note_type(eof_song, eof_selected_track, i-1) == eof_note_type))
			{
				eof_track_delete_note(eof_song, eof_selected_track, i-1);
				eof_selection.multi[i-1] = 0;
			}
		}
		eof_selection.current = EOF_MAX_NOTES - 1;
		eof_track_fixup_notes(eof_song, eof_selected_track, 0);
		eof_detect_difficulties(eof_song);
		eof_reset_lyric_preview_lines();
	}
	return 1;
}

int eof_menu_note_delete(void)
{
	if(eof_vocals_selected)
	{
		return eof_menu_note_delete_vocal();
	}
	unsigned long i, d = 0;

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			d++;
		}
	}
	if(d)
	{
		eof_prepare_undo(EOF_UNDO_TYPE_NOTE_SEL);
		for(i = eof_track_get_size(eof_song, eof_selected_track); i > 0; i--)
		{
			if((eof_selection.track == eof_selected_track) && eof_selection.multi[i-1] && (eof_selection.track == eof_selected_track) && (eof_get_note_type(eof_song, eof_selected_track, i-1) == eof_note_type))
			{
				eof_track_delete_note(eof_song, eof_selected_track, i-1);
				eof_selection.multi[i-1] = 0;
			}
		}
		eof_selection.current = EOF_MAX_NOTES - 1;
		eof_detect_difficulties(eof_song);
		eof_determine_hopos();
	}
	return 1;
}

int eof_menu_note_toggle_green(void)
{
	unsigned long i;
	unsigned long flags, note;

	if(eof_count_selected_notes(NULL, 0) <= 0)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			note ^= 1;
			eof_set_note_note(eof_song, eof_selected_track, i, note);
			if(eof_selected_track == EOF_TRACK_DRUM)
			{	//If green drum is being toggled on/off
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				flags &= (~EOF_NOTE_FLAG_DBASS);		//Clear the Expert+ status if it is set
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
			}
			else if(eof_selected_track == EOF_TRACK_BASS)
			{	//When a lane 1 bass note is added, open bass must be forced clear, because they use conflicting MIDI notation
				note &= ~(32);	//Clear the bit for lane 6 (open bass)
				eof_set_note_note(eof_song, eof_selected_track, i, note);
			}
		}
	}
	return 1;
}

int eof_menu_note_toggle_red(void)
{
	unsigned long i;
	unsigned long note;

	if(eof_count_selected_notes(NULL, 0) <= 0)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			note ^= 2;
			eof_set_note_note(eof_song, eof_selected_track, i, note);
		}
	}
	return 1;
}

int eof_menu_note_toggle_yellow(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	unsigned long flags, note;

	if(eof_count_selected_notes(NULL, 0) <= 0)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			note ^= 4;
			if(eof_selected_track == EOF_TRACK_DRUM)
			{	//If yellow drum is being toggled on/off
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				flags &= (~EOF_NOTE_FLAG_Y_CYMBAL);	//Clear the Pro yellow cymbal status if it is set
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
				if(eof_mark_drums_as_cymbal && (eof_song->legacy_track[tracknum]->note[i]->note & 4))
				{	//If user specified to mark new notes as cymbals, and this note was toggled on
					eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_Y_CYMBAL,1);	//Set the yellow cymbal flag on all drum notes at this position
				}
			}
			eof_set_note_note(eof_song, eof_selected_track, i, note);
		}
	}
	return 1;
}

int eof_menu_note_toggle_blue(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	unsigned long flags, note;

	if(eof_count_selected_notes(NULL, 0) <= 0)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			note ^= 8;
			if(eof_selected_track == EOF_TRACK_DRUM)
			{	//If blue drum is being toggled on/off
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				flags &= (~EOF_NOTE_FLAG_B_CYMBAL);	//Clear the Pro blue cymbal status if it is set
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
				if(eof_mark_drums_as_cymbal && (note & 8))
				{	//If user specified to mark new notes as cymbals, and this note was toggled on
					eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_B_CYMBAL,1);	//Set the blue cymbal flag on all drum notes at this position
				}
			}
			eof_set_note_note(eof_song, eof_selected_track, i, note);
		}
	}
	return 1;
}

int eof_menu_note_toggle_purple(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	unsigned long flags, note;

	if(eof_count_selected_notes(NULL, 0) <= 0)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			note ^= 16;	//Toggle lane 5
			if(eof_selected_track == EOF_TRACK_DRUM)
			{	//If green drum is being toggled on/off
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				flags &= (~EOF_NOTE_FLAG_G_CYMBAL);	//Clear the Pro green cymbal status if it is set
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
				if(eof_mark_drums_as_cymbal && (note & 16))
				{	//If user specified to mark new notes as cymbals, and this note was toggled on
					eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_G_CYMBAL,1);	//Set the green cymbal flag on all drum notes at this position
				}
			}
			eof_set_note_note(eof_song, eof_selected_track, i, note);
		}
	}
	return 1;
}

int eof_menu_note_toggle_orange(void)
{
	unsigned long i;
	unsigned long flags, note;

	if(eof_count_track_lanes(eof_song, eof_selected_track) < 6)
	{
		return 1;	//Don't do anything if there is less than 6 tracks available
	}
	if(eof_count_selected_notes(NULL, 0) <= 0)
	{
		return 1;
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			if(eof_selected_track == EOF_TRACK_BASS)
			{	//When an open bass note is added, all other lanes must be forced clear, because they use conflicting MIDI notation
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				eof_set_note_note(eof_song, eof_selected_track, i, 32);	//Clear all lanes except lane 6
				flags &= ~(EOF_NOTE_FLAG_CRAZY);		//Clear the crazy flag, which is invalid for open strum notes
				flags &= ~(EOF_NOTE_FLAG_F_HOPO);	//Clear the HOPO flags, which are invalid for open strum notes
				flags &= ~(EOF_NOTE_FLAG_NO_HOPO);
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
			}
			else
			{
				note = eof_get_note_note(eof_song, eof_selected_track, i);
				note ^= 32;	//Toggle lane 6
				eof_set_note_note(eof_song, eof_selected_track, i, note);
			}
		}
	}
	return 1;
}

int eof_menu_note_toggle_crazy(void)
{
	unsigned long i;
	int u = 0;	//Is set to nonzero when an undo state has been made
	unsigned long track_behavior = eof_song->track[eof_selected_track]->track_behavior;
	unsigned long flags;

	if((track_behavior == EOF_DRUM_TRACK_BEHAVIOR) || (track_behavior == EOF_VOCAL_TRACK_BEHAVIOR) || (track_behavior == EOF_KEYS_TRACK_BEHAVIOR))
		return 1;	//Do not allow this function to run on any drum, vocal or keys track

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{	//If the note is in the active instrument difficulty and is selected
			if(!((eof_selected_track == EOF_TRACK_BASS) && (eof_get_note_note(eof_song, eof_selected_track, i) & 32)))
			{	//If the note is not an open bass strum note (lane 6)
				if(!u)
				{
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);
					u = 1;
				}
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				flags ^= EOF_NOTE_FLAG_CRAZY;
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
			}
		}
	}
	return 1;
}

int eof_menu_note_toggle_double_bass(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags;

	if(eof_selected_track != EOF_TRACK_DRUM)
		return 1;	//Do not allow this function to run when PART DRUMS is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == EOF_NOTE_AMAZING) && (eof_get_note_note(eof_song, eof_selected_track, i) & 1))
		{	//If this note is in the currently active track, is selected, is in the Expert difficulty and has a green gem
			if(!u)
			{
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			flags ^= EOF_NOTE_FLAG_DBASS;
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}

int eof_menu_note_toggle_rb3_cymbal_green(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	long u = 0;
	unsigned long flags;

	if(eof_selected_track != EOF_TRACK_DRUM)
		return 1;	//Do not allow this function to run when PART DRUMS is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			if(eof_get_note_note(eof_song, eof_selected_track, i) & 16)
			{	//If this drum note is purple (represents a green drum in Rock Band)
				if(!u)
				{	//Make a back up before changing the first note
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);
					u = 1;
				}
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				flags &= (~EOF_NOTE_FLAG_DBASS);	//Clear the Expert+ status if it is set
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
				eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_G_CYMBAL,2);	//Toggle the green cymbal flag on all drum notes at this position
			}
		}
	}
	return 1;
}

int eof_menu_note_toggle_rb3_cymbal_yellow(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	long u = 0;

	if(eof_selected_track != EOF_TRACK_DRUM)
		return 1;	//Do not allow this function to run when PART DRUMS is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			if(eof_get_note_note(eof_song, eof_selected_track, i) & 4)
			{	//If this drum note is yellow
				if(!u)
				{	//Make a back up before changing the first note
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);
					u = 1;
				}
				eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_Y_CYMBAL,2);	//Toggle the yellow cymbal flag on all drum notes at this position
			}
		}
	}
	return 1;
}

int eof_menu_note_toggle_rb3_cymbal_blue(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	long u = 0;

	if(eof_selected_track != EOF_TRACK_DRUM)
		return 1;	//Do not allow this function to run when PART DRUMS is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			if(eof_get_note_note(eof_song, eof_selected_track, i) & 8)
			{	//If this drum note is blue
				if(!u)
				{	//Make a back up before changing the first note
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);
					u = 1;
				}
//				eof_song->legacy_track[tracknum]->note[i]->flags ^= EOF_NOTE_FLAG_B_CYMBAL;
				eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_B_CYMBAL,2);	//Toggle the blue cymbal flag on all drum notes at this position
			}
		}
	}
	return 1;
}

int eof_menu_note_remove_cymbal(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	long u = 0;
	unsigned long flags, oldflags, note;

	if(eof_selected_track != EOF_TRACK_DRUM)
		return 1;	//Do not allow this function to run when PART DRUMS is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			note = eof_get_note_note(eof_song, eof_selected_track, i);
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			oldflags = flags;	//Save an extra copy of the original flags
			if(	((note & 4) && (flags & EOF_NOTE_FLAG_Y_CYMBAL)) ||
				((note & 8) && (flags & EOF_NOTE_FLAG_B_CYMBAL)) ||
				((note & 16) && (flags & EOF_NOTE_FLAG_G_CYMBAL)))
			{	//If this note has a cymbal notation
				if(!u && (oldflags != flags))
				{	//Make a back up before changing the first note
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);
					u = 1;
				}
				eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_Y_CYMBAL,0);	//Clear the yellow cymbal flag on all drum notes at this position
				eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_B_CYMBAL,0);	//Clear the blue cymbal flag on all drum notes at this position
				eof_set_flags_at_legacy_note_pos(eof_song->legacy_track[tracknum],i,EOF_NOTE_FLAG_G_CYMBAL,0);	//Clear the green cymbal flag on all drum notes at this position
			}
		}
	}
	return 1;
}

int eof_menu_note_default_cymbal(void)
{
	if(eof_mark_drums_as_cymbal)
	{
		eof_mark_drums_as_cymbal = 0;
		eof_note_prodrum_menu[4].flags = 0;
	}
	else
	{
		eof_mark_drums_as_cymbal = 1;
		eof_note_prodrum_menu[4].flags = D_SELECTED;
	}
	return 1;
}

float eof_menu_note_push_get_offset(void)
{
	switch(eof_snap_mode)
	{
		case EOF_SNAP_QUARTER:
		{
			return 100.0;
		}
		case EOF_SNAP_EIGHTH:
		{
			return 50.0;
		}
		case EOF_SNAP_TWELFTH:
		{
			return 100.0 / 3.0;
		}
		case EOF_SNAP_SIXTEENTH:
		{
			return 25.0;
		}
		case EOF_SNAP_TWENTY_FOURTH:
		{
			return 100.0 / 6.0;
		}
		case EOF_SNAP_THIRTY_SECOND:
		{
			return 12.5;
		}
		case EOF_SNAP_FORTY_EIGHTH:
		{
			return 100.0 / 12.0;
		}
		case EOF_SNAP_CUSTOM:
		{
			return 100.0 / (float)eof_snap_interval;
		}
	}
	return 0.0;
}

int eof_menu_note_push_back(void)
{
	unsigned long i;
	float porpos;
	long beat;

	if(eof_count_selected_notes(NULL, 0) > 0)
	{	//If notes are selected
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each note in the active track
			if(eof_selection.multi[i])
			{
				beat = eof_get_beat(eof_song, eof_get_note_pos(eof_song, eof_selected_track, i));
				porpos = eof_get_porpos(eof_get_note_pos(eof_song, eof_selected_track, i));
				eof_set_note_pos(eof_song, eof_selected_track, i, eof_put_porpos(beat, porpos, eof_menu_note_push_get_offset()));
			}
		}
	}
	else
	{
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{
			if(eof_get_note_pos(eof_song, eof_selected_track, i) >= eof_music_pos - eof_av_delay)
			{
				beat = eof_get_beat(eof_song, eof_get_note_pos(eof_song, eof_selected_track, i));
				porpos = eof_get_porpos(eof_get_note_pos(eof_song, eof_selected_track, i));
				eof_set_note_pos(eof_song, eof_selected_track, i, eof_put_porpos(beat, porpos, eof_menu_note_push_get_offset()));
			}
		}
	}
	return 1;
}

int eof_menu_note_push_up(void)
{
	unsigned long i;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	float porpos;
	long beat;

	if(eof_count_selected_notes(NULL, 0) > 0)
	{	//If notes are selected
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each note in the active track
			if(eof_selection.multi[i])
			{
				beat = eof_get_beat(eof_song, eof_get_note_pos(eof_song, eof_selected_track, i));
				porpos = eof_get_porpos(eof_song->legacy_track[tracknum]->note[i]->pos);
				eof_set_note_pos(eof_song, eof_selected_track, i, eof_put_porpos(beat, porpos, -eof_menu_note_push_get_offset()));
			}
		}
	}
	else
	{
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each note in the active track
			if(eof_get_note_pos(eof_song, eof_selected_track, i) >= eof_music_pos - eof_av_delay)
			{
				beat = eof_get_beat(eof_song, eof_get_note_pos(eof_song, eof_selected_track, i));
				porpos = eof_get_porpos(eof_get_note_pos(eof_song, eof_selected_track, i));
				eof_set_note_pos(eof_song, eof_selected_track, i, eof_put_porpos(beat, porpos, -eof_menu_note_push_get_offset()));
			}
		}
	}
	return 1;
}

int eof_menu_note_create_bre(void)
{
	unsigned long i;
	long first_pos = 0;
	long last_pos = eof_music_length;
	EOF_NOTE * new_note = NULL;

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			if(eof_get_note_pos(eof_song, eof_selected_track, i) > first_pos)
			{
				first_pos = eof_get_note_pos(eof_song, eof_selected_track, i);
			}
			if(eof_get_note_pos(eof_song, eof_selected_track, i) < last_pos)
			{
				last_pos = eof_get_note_pos(eof_song, eof_selected_track, i);
			}
		}
	}

	/* create the BRE marking note */
	if((first_pos != 0) && (last_pos != eof_music_length))
	{
		new_note = eof_track_add_create_note(eof_song, eof_selected_track, 31, first_pos, last_pos - first_pos, EOF_NOTE_SPECIAL, NULL);
//		new_note->type = EOF_NOTE_SPECIAL;
//		new_note->flags = EOF_NOTE_FLAG_BRE;
	}
	return 1;
}

/* split a lyric into multiple pieces (look for ' ' characters) */
static void eof_split_lyric(int lyric)
{
	unsigned long i, l, c = 0, lastc;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	int first = 1;
	int piece = 1;
	int pieces = 1;
	char * token = NULL;
	EOF_LYRIC * new_lyric = NULL;

	if(!eof_vocals_selected)
		return;

	/* see how many pieces there are */
	for(i = 0; i < strlen(eof_song->vocal_track[tracknum]->lyric[lyric]->text); i++)
	{
		lastc = c;
		c = eof_song->vocal_track[tracknum]->lyric[lyric]->text[i];
		if((c == ' ') && (lastc != ' '))
		{
			pieces++;
		}
	}

	/* shorten the original note */
	if((eof_song->vocal_track[tracknum]->lyric[lyric]->note >= MINPITCH) && (eof_song->vocal_track[tracknum]->lyric[lyric]->note <= MAXPITCH))
	{
		l = eof_song->vocal_track[tracknum]->lyric[lyric]->length > 100 ? eof_song->vocal_track[tracknum]->lyric[lyric]->length : 250 * pieces;
	}
	else
	{
		l = 250 * pieces;
	}
	eof_song->vocal_track[tracknum]->lyric[lyric]->length = l / pieces - 20;
	if(eof_song->vocal_track[tracknum]->lyric[lyric]->length < 1)
	{
		eof_song->vocal_track[tracknum]->lyric[lyric]->length = 1;
	}

	/* split at spaces */
	strtok(eof_song->vocal_track[tracknum]->lyric[lyric]->text, " ");
	do
	{
		token = strtok(NULL, " ");
		if(token)
		{
//			if(!first)
			{
				new_lyric = eof_track_add_create_note(eof_song, eof_selected_track, eof_song->vocal_track[tracknum]->lyric[lyric]->note, eof_song->vocal_track[tracknum]->lyric[lyric]->pos + (l / pieces) * piece, l / pieces - 20, 0, token);
				piece++;
			}
			first = 0;
		}
	} while(token != NULL);
	eof_track_sort_notes(eof_song, eof_selected_track);
	eof_track_fixup_notes(eof_song, eof_selected_track, 1);
}

int eof_menu_split_lyric(void)
{
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	if(!eof_vocals_selected)
		return 1;
	if(eof_count_selected_notes(NULL, 0) != 1)
	{
		return 1;
	}
	eof_cursor_visible = 0;
	eof_render();
	eof_color_dialog(eof_split_lyric_dialog, gui_fg_color, gui_bg_color);
	centre_dialog(eof_split_lyric_dialog);
	ustrcpy(eof_etext, eof_song->vocal_track[tracknum]->lyric[eof_selection.current]->text);
	if(eof_popup_dialog(eof_split_lyric_dialog, 2) == 3)
	{
		if(ustricmp(eof_song->vocal_track[tracknum]->lyric[eof_selection.current]->text, eof_etext))
		{
			eof_prepare_undo(EOF_UNDO_TYPE_NONE);
			ustrcpy(eof_song->vocal_track[tracknum]->lyric[eof_selection.current]->text, eof_etext);
			eof_split_lyric(eof_selection.current);
		}
	}
	eof_cursor_visible = 1;
	eof_pen_visible = 1;
	eof_show_mouse(screen);
	return D_O_K;
}

int eof_menu_solo_mark(void)
{
	unsigned long i, j;
	long insp = -1;
	long sel_start = -1;
	long sel_end = 0;
	EOF_SOLO_ENTRY *soloptr = NULL;

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			if(eof_get_note_pos(eof_song, eof_selected_track, i) < sel_start)
			{
				sel_start = eof_get_note_pos(eof_song, eof_selected_track, i);
			}
			if(eof_get_note_pos(eof_song, eof_selected_track, i) + eof_get_note_length(eof_song, eof_selected_track, i) > sel_end)
			{
				sel_end = eof_get_note_pos(eof_song, eof_selected_track, i) + eof_get_note_length(eof_song, eof_selected_track, i);
			}
		}
	}
	for(j = 0; j < eof_get_num_solos(eof_song, eof_selected_track); j++)
	{	//For each solo in the track
		soloptr = eof_get_solo(eof_song, eof_selected_track, j);
		if((sel_end >= soloptr->start_pos) && (sel_start <= soloptr->end_pos))
		{
			insp = j;
		}
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	if(insp < 0)
	{
		eof_track_add_solo(eof_song, eof_selected_track, sel_start, sel_end);
	}
	else
	{
		soloptr = eof_get_solo(eof_song, eof_selected_track, insp);
		soloptr->start_pos = sel_start;
		soloptr->end_pos = sel_end;
	}
	return 1;
}

int eof_menu_solo_unmark(void)
{
	unsigned long i, j;
	EOF_SOLO_ENTRY *soloptr = NULL;

	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			for(j = 0; j < eof_get_num_solos(eof_song, eof_selected_track); j++)
			{	//For each solo section in the track
				soloptr = eof_get_solo(eof_song, eof_selected_track, j);
				if((eof_get_note_pos(eof_song, eof_selected_track, i) >= soloptr->start_pos) && (eof_get_note_pos(eof_song, eof_selected_track, i) + eof_get_note_length(eof_song, eof_selected_track, i) <= soloptr->end_pos))
				{
					eof_track_delete_solo(eof_song, eof_selected_track, j);
					break;
				}
			}
		}
	}
	return 1;
}

int eof_menu_solo_erase_all(void)
{
	if(alert(NULL, "Erase all solos from this track?", NULL, "&Yes", "&No", 'y', 'n') == 1)
	{
		eof_prepare_undo(EOF_UNDO_TYPE_NONE);
		eof_set_num_solos(eof_song, eof_selected_track, 0);
	}
	return 1;
}

int eof_menu_star_power_mark(void)
{
	unsigned long i, j;
	long insp = -1;
	long sel_start = -1;
	long sel_end = 0;
	EOF_STAR_POWER_ENTRY *starpowerptr = NULL;

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			if(eof_get_note_pos(eof_song, eof_selected_track, i) < sel_start)
			{
				sel_start = eof_get_note_pos(eof_song, eof_selected_track, i);
			}
			if(eof_get_note_pos(eof_song, eof_selected_track, i) > sel_end)
			{
				sel_end = eof_get_note_pos(eof_song, eof_selected_track, i) + (eof_get_note_length(eof_song, eof_selected_track, i) > 20 ? eof_get_note_length(eof_song, eof_selected_track, i) : 20);
			}
		}
	}
	for(j = 0; j < eof_get_num_star_power_paths(eof_song, eof_selected_track); j++)
	{	//For each star power path in the active track
		starpowerptr = eof_get_star_power_path(eof_song, eof_selected_track, j);
		if((sel_end >= starpowerptr->start_pos) && (sel_start <= starpowerptr->end_pos))
		{
			insp = j;
		}
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	if(insp < 0)
	{
		eof_track_add_star_power_path(eof_song, eof_selected_track, sel_start, sel_end);
	}
	else
	{
		starpowerptr = eof_get_star_power_path(eof_song, eof_selected_track, insp);
		if(starpowerptr != NULL)
		{
			starpowerptr->start_pos = sel_start;
			starpowerptr->end_pos = sel_end;
		}
	}
	eof_determine_hopos();
	return 1;
}

int eof_menu_star_power_unmark(void)
{
	unsigned long i, j;
	EOF_STAR_POWER_ENTRY *starpowerptr = NULL;

	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
		{
			for(j = 0; j < eof_get_num_star_power_paths(eof_song, eof_selected_track); j++)
			{	//For each star power path in the track
				starpowerptr = eof_get_star_power_path(eof_song, eof_selected_track, j);
				if((eof_get_note_pos(eof_song, eof_selected_track, i) >= starpowerptr->start_pos) && (eof_get_note_pos(eof_song, eof_selected_track, i) <= starpowerptr->end_pos))
				{
					eof_track_delete_star_power_path(eof_song, eof_selected_track, j);
					break;
				}
			}
		}
	}
	eof_determine_hopos();
	return 1;
}

int eof_menu_star_power_erase_all(void)
{
	if(alert(NULL, "Erase all star power from this track?", NULL, "&Yes", "&No", 'y', 'n') == 1)
	{
		eof_prepare_undo(EOF_UNDO_TYPE_NONE);
		eof_set_num_star_power_paths(eof_song, eof_selected_track, 0);
	}
	eof_determine_hopos();
	return 1;
}

int eof_menu_lyric_line_mark(void)
{
	unsigned long i, j;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	long sel_start = -1;
	long sel_end = 0;
	int originalflags = 0; //Used to apply the line's original flags after the line is recreated

	if(!eof_vocals_selected)
		return 1;

	for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
	{
		if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i])
		{
			if(eof_song->vocal_track[tracknum]->lyric[i]->pos < sel_start)
			{
				sel_start = eof_song->vocal_track[tracknum]->lyric[i]->pos;
				if(sel_start < eof_song->tags->ogg[eof_selected_ogg].midi_offset)
				{
					sel_start = eof_song->tags->ogg[eof_selected_ogg].midi_offset;
				}
			}
			if(eof_song->vocal_track[tracknum]->lyric[i]->pos > sel_end)
			{
				sel_end = eof_song->vocal_track[tracknum]->lyric[i]->pos + eof_song->vocal_track[tracknum]->lyric[i]->length;
				if(sel_end >= eof_music_length)
				{
					sel_end = eof_music_length - 1;
				}
			}
		}
	}
	eof_prepare_undo(EOF_UNDO_TYPE_NONE);	//Create the undo state before removing/adding phrase(s)
	for(j = eof_song->vocal_track[tracknum]->lines; j > 0; j--)
	{
		if((sel_end >= eof_song->vocal_track[tracknum]->line[j-1].start_pos) && (sel_start <= eof_song->vocal_track[tracknum]->line[j-1].end_pos))
		{
			originalflags=eof_song->vocal_track[tracknum]->line[j-1].flags;	//Save this line's flags before deleting it
			eof_vocal_track_delete_line(eof_song->vocal_track[tracknum], j-1);
		}
	}
	eof_vocal_track_add_line(eof_song->vocal_track[tracknum], sel_start, sel_end);

	if(eof_song->vocal_track[tracknum]->lines >0)
		eof_song->vocal_track[tracknum]->line[eof_song->vocal_track[tracknum]->lines-1].flags = originalflags;	//Restore the line's flags

	/* check for overlapping lines */
	for(i = 0; i < eof_song->vocal_track[tracknum]->lines; i++)
	{
		for(j = i; j < eof_song->vocal_track[tracknum]->lines; j++)
		{
			if((i != j) && (eof_song->vocal_track[tracknum]->line[i].start_pos <= eof_song->vocal_track[tracknum]->line[j].end_pos) && (eof_song->vocal_track[tracknum]->line[j].start_pos <= eof_song->vocal_track[tracknum]->line[i].end_pos))
			{
				eof_song->vocal_track[tracknum]->line[i].start_pos = eof_song->vocal_track[tracknum]->line[j].end_pos + 1;
			}
		}
	}
	eof_reset_lyric_preview_lines();
	return 1;
}

int eof_menu_lyric_line_unmark(void)
{
	unsigned long i, j;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_vocals_selected)
		return 1;

	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
	{
		if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i])
		{
			for(j = 0; j < eof_song->vocal_track[tracknum]->lines; j++)
			{
				if((eof_song->vocal_track[tracknum]->lyric[i]->pos >= eof_song->vocal_track[tracknum]->line[j].start_pos) && (eof_song->vocal_track[tracknum]->lyric[i]->pos <= eof_song->vocal_track[tracknum]->line[j].end_pos))
				{
					eof_vocal_track_delete_line(eof_song->vocal_track[tracknum], j);
					break;
				}
			}
		}
	}
	eof_reset_lyric_preview_lines();
	return 1;
}

int eof_menu_lyric_line_erase_all(void)
{
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_vocals_selected)
		return 1;

	if(alert(NULL, "Erase all lyric lines?", NULL, "&Yes", "&No", 'y', 'n') == 1)
	{
		eof_prepare_undo(EOF_UNDO_TYPE_NONE);
		eof_song->vocal_track[tracknum]->lines = 0;
		eof_reset_lyric_preview_lines();
	}
	return 1;
}

int eof_menu_lyric_line_toggle_overdrive(void)
{
	char used[1024] = {0};
	unsigned long i, j;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_vocals_selected)
		return 1;

	eof_prepare_undo(EOF_UNDO_TYPE_NONE);
	for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
	{
		if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i])
		{
			for(j = 0; j < eof_song->vocal_track[tracknum]->lines; j++)
			{
				if((eof_song->vocal_track[tracknum]->lyric[i]->pos >= eof_song->vocal_track[tracknum]->line[j].start_pos) && (eof_song->vocal_track[tracknum]->lyric[i]->pos <= eof_song->vocal_track[tracknum]->line[j].end_pos && !used[j]))
				{
					eof_song->vocal_track[tracknum]->line[j].flags ^= EOF_LYRIC_LINE_FLAG_OVERDRIVE;
					used[j] = 1;
				}
			}
		}
	}
	return 1;
}

int eof_menu_hopo_auto(void)
{
	unsigned long i;
	char undo_made = 0;	//Set to nonzero if an undo state was saved
	unsigned long flags, oldflags;

	if((eof_selected_track == EOF_TRACK_DRUM) || eof_vocals_selected)
		return 1;	//Do not allow this function to run when PART DRUMS or PART VOCALS is active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if(eof_selection.multi[i])
		{	//If the note is selected
			if(!((eof_selected_track == EOF_TRACK_BASS) && (eof_get_note_note(eof_song, eof_selected_track, i) & 32)))
			{	//If the note is not an open bass strum note
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				oldflags = flags;					//Save another copy of the original flags
				flags &= (~EOF_NOTE_FLAG_F_HOPO);	//Clear the HOPO on flag
				flags &= (~EOF_NOTE_FLAG_NO_HOPO);	//Clear the HOPO off flag
				if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
				{	//If this is a pro guitar note, ensure that Hammer on, Pull of and Tap statuses are cleared
					flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_HO);	//Clear the hammer on flag
					flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_PO);	//Clear the pull off flag
					flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_TAP);	//Clear the tap flag
				}
				if(!undo_made && (flags != oldflags))
				{	//If an undo state hasn't been made yet
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);	//Make one
					undo_made = 1;
				}
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
			}
		}
	}
	eof_determine_hopos();
	return 1;
}

int eof_menu_hopo_cycle(void)
{
	unsigned long i;
	char undo_made = 0;	//Set to nonzero if an undo state was saved
	unsigned long flags;

	if((eof_selected_track == EOF_TRACK_DRUM) || eof_vocals_selected)
		return 1;	//Do not allow this function to run when PART DRUMS or PART VOCALS is active

	if((eof_count_selected_notes(NULL, 0) > 0))
	{
		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//For each note in the active track
			if(eof_selection.multi[i])
			{	//If the note is selected
				if(!((eof_selected_track == EOF_TRACK_BASS) && (eof_get_note_note(eof_song, eof_selected_track, i) & 32)))
				{	//If the note is not an open bass strum note
					if((eof_selected_track == EOF_TRACK_BASS) && eof_open_bass_enabled() && (eof_get_note_note(eof_song, eof_selected_track, i) & 1))
					{	//If open bass strumming is enabled and this is a bass guitar note that uses lane 1
						continue;	//Skip this note, as open bass and forced HOPO on lane 1 conflict
					}
					if(!undo_made)
					{	//If an undo state hasn't been made yet
						eof_prepare_undo(EOF_UNDO_TYPE_NONE);	//Make one
						undo_made = 1;
					}
					flags = eof_get_note_flags(eof_song, eof_selected_track, i);
					if(flags & EOF_NOTE_FLAG_F_HOPO)
					{	//If the note was a forced on HOPO, make it a forced off HOPO
						flags &= ~EOF_NOTE_FLAG_F_HOPO;	//Turn off forced on hopo
						flags |= EOF_NOTE_FLAG_NO_HOPO;	//Turn on forced off hopo
						if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
						{	//If this is a pro guitar note, ensure that Hammer on, Pull of and Tap statuses are cleared
							flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_HO);	//Clear the hammer on flag
							flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_PO);	//Clear the pull off flag
							flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_TAP);	//Clear the tap flag
						}
					}
					else if(flags & EOF_NOTE_FLAG_NO_HOPO)
					{	//If the note was a forced off HOPO, make it an auto HOPO
						flags &= ~EOF_NOTE_FLAG_F_HOPO;		//Clear the forced on hopo flag
						flags &= ~EOF_NOTE_FLAG_NO_HOPO;	//Clear the forced off hopo flag
						if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
						{	//If this is a pro guitar note, ensure that Hammer on, Pull of and Tap statuses are cleared
							flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_HO);	//Clear the hammer on flag
							flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_PO);	//Clear the pull off flag
							flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_TAP);	//Clear the tap flag
						}
					}
					else
					{	//If the note was an auto HOPO, make it a forced on HOPO
						flags |= EOF_NOTE_FLAG_F_HOPO;	//Turn on forced on hopo
						if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
						{	//If this is a pro guitar note
							flags |= EOF_PRO_GUITAR_NOTE_FLAG_HO;		//Set the hammer on flag (default HOPO type)
							flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_PO);	//Clear the pull off flag
							flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_TAP);	//Clear the tap flag
						}
					}
					eof_set_note_flags(eof_song, eof_selected_track, i, flags);
				}
			}
		}
		eof_determine_hopos();
	}
	return 1;
}

int eof_menu_hopo_force_on(void)
{
	unsigned long i;
	char undo_made = 0;	//Set to nonzero if an undo state was saved
	unsigned long flags, oldflags;

	if((eof_selected_track == EOF_TRACK_DRUM) || eof_vocals_selected)
		return 1;	//Do not allow this function to run when PART DRUMS or PART VOCALS is active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if(eof_selection.multi[i])
		{
			if(!((eof_selected_track == EOF_TRACK_BASS) && (eof_get_note_note(eof_song, eof_selected_track, i) & 32)))
			{	//If the note is not an open bass strum note
				if((eof_selected_track == EOF_TRACK_BASS) && eof_open_bass_enabled() && (eof_get_note_note(eof_song, eof_selected_track, i) & 1))
				{	//If open bass strumming is enabled and this is a bass guitar note that uses lane 1
					continue;	//Skip this note, as open bass and forced HOPO on lane 1 conflict
				}
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				oldflags = flags;					//Save another copy of the original flags
				flags |= EOF_NOTE_FLAG_F_HOPO;		//Set the HOPO on flag
				flags &= (~EOF_NOTE_FLAG_NO_HOPO);	//Clear the HOPO off flag
				if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
				{	//If this is a pro guitar note
					flags |= EOF_PRO_GUITAR_NOTE_FLAG_HO;		//Set the hammer on flag (default HOPO type)
					flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_PO);	//Clear the pull off flag
					flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_TAP);	//Clear the tap flag
				}
				if(!undo_made && (flags != oldflags))
				{	//If an undo state hasn't been made yet
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);	//Make one
					undo_made = 1;
				}
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
			}
		}
	}
	eof_determine_hopos();
	return 1;
}

int eof_menu_hopo_force_off(void)
{
	unsigned long i;
	char undo_made = 0;	//Set to nonzero if an undo state was saved
	unsigned long flags, oldflags;

	if((eof_selected_track == EOF_TRACK_DRUM) || eof_vocals_selected)
		return 1;	//Do not allow this function to run when PART DRUMS or PART VOCALS is active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if(eof_selection.multi[i])
		{
			if(!((eof_selected_track == EOF_TRACK_BASS) && (eof_get_note_note(eof_song, eof_selected_track, i) & 32)))
			{	//If the note is not an open bass strum note
				flags = eof_get_note_flags(eof_song, eof_selected_track, i);
				oldflags = flags;					//Save another copy of the original flags
				flags |= EOF_NOTE_FLAG_NO_HOPO;		//Set the HOPO off flag
				flags &= (~EOF_NOTE_FLAG_F_HOPO);	//Clear the HOPO on flag
				if(eof_song->track[eof_selected_track]->track_format == EOF_PRO_GUITAR_TRACK_FORMAT)
				{	//If this is a pro guitar note, ensure that Hammer on, Pull of and Tap statuses are cleared
					flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_HO);	//Clear the hammer on flag
					flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_PO);	//Clear the pull off flag
					flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_TAP);	//Clear the tap flag
				}
				if(!undo_made && (flags != oldflags))
				{	//If an undo state hasn't been made yet
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);	//Make one
					undo_made = 1;
				}
				eof_set_note_flags(eof_song, eof_selected_track, i, flags);
			}
		}
	}
	eof_determine_hopos();
	return 1;
}

int eof_transpose_possible(int dir)
{
	unsigned long i;
	unsigned long max = 16;	//This represents the highest note bitmask value that will be allowed to transpose up, based on the current track options (including open bass strumming)

	/* no notes, no transpose */
	if(eof_vocals_selected)
	{
		if(eof_track_get_size(eof_song, eof_selected_track) <= 0)
		{
			return 0;
		}

		if(eof_count_selected_notes(NULL, 1) <= 0)
		{
			return 0;
		}

		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{	//Test if the lyric can transpose the given amount in the given direction
			if((eof_selection.track == EOF_TRACK_VOCALS) && eof_selection.multi[i])
			{
				if((eof_get_note_note(eof_song, eof_selected_track, i) == 0) || (eof_get_note_note(eof_song, eof_selected_track, i) == EOF_LYRIC_PERCUSSION))
				{	//Cannot transpose a pitchless lyric or a vocal percussion note
					return 0;
				}
				if(eof_get_note_note(eof_song, eof_selected_track, i) - dir < MINPITCH)
				{
					return 0;
				}
				else if(eof_get_note_note(eof_song, eof_selected_track, i) - dir > MAXPITCH)
				{
					return 0;
				}
			}
		}
		return 1;
	}
	else
	{
		if(eof_open_bass_enabled() || (eof_count_track_lanes(eof_song, eof_selected_track) > 5))
		{	//If open bass is enabled, or the track has more than 5 lanes, lane 5 can transpose up to lane 6
			max = 32;
		}
		if(eof_track_get_size(eof_song, eof_selected_track) <= 0)
		{
			return 0;
		}

		if(eof_count_selected_notes(NULL, 1) <= 0)
		{
			return 0;
		}

		for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
		{
			if((eof_selection.track == eof_selected_track) && eof_selection.multi[i] && (eof_get_note_type(eof_song, eof_selected_track, i) == eof_note_type))
			{
				if((eof_get_note_note(eof_song, eof_selected_track, i) & 1) && (dir > 0))
				{
					return 0;
				}
				else if((eof_get_note_note(eof_song, eof_selected_track, i) & max) && (dir < 0))
				{
					return 0;
				}
			}
		}
		return 1;
	}
}

int eof_new_lyric_dialog(void)
{
	EOF_LYRIC * new_lyric = NULL;
	int ret=0;

	if(!eof_vocals_selected)
		return D_O_K;

	eof_cursor_visible = 0;
	eof_render();
	eof_color_dialog(eof_lyric_dialog, gui_fg_color, gui_bg_color);
	centre_dialog(eof_lyric_dialog);
	ustrcpy(eof_etext, "");

	if(eof_pen_lyric.note != EOF_LYRIC_PERCUSSION)		//If not entering a percussion note
	{
		ret = eof_popup_dialog(eof_lyric_dialog, 2);	//prompt for lyric text
		if(!eof_check_string(eof_etext) && !eof_pen_lyric.note)	//If the placed lyric is both pitchless AND textless
			return D_O_K;	//Return without adding
	}

	if((ret == 3) || (eof_pen_lyric.note == EOF_LYRIC_PERCUSSION))
	{
		eof_prepare_undo(EOF_UNDO_TYPE_NONE);
		new_lyric = eof_track_add_create_note(eof_song, eof_selected_track, eof_pen_lyric.note, eof_pen_lyric.pos, eof_pen_lyric.length, 0, NULL);
		ustrcpy(new_lyric->text, eof_etext);
		eof_selection.track = EOF_TRACK_VOCALS;
		eof_selection.current_pos = new_lyric->pos;
		eof_selection.range_pos_1 = eof_selection.current_pos;
		eof_selection.range_pos_2 = eof_selection.current_pos;
		memset(eof_selection.multi, 0, sizeof(eof_selection.multi));
		eof_track_sort_notes(eof_song, eof_selected_track);
		eof_track_fixup_notes(eof_song, eof_selected_track, 0);
		eof_detect_difficulties(eof_song);
		eof_reset_lyric_preview_lines();
	}
	eof_cursor_visible = 1;
	eof_pen_visible = 1;
	eof_show_mouse(screen);
	return D_O_K;
}

int eof_edit_lyric_dialog(void)
{
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_vocals_selected)
		return 1;

	if(eof_count_selected_notes(NULL, 0) != 1)
	{
		return 1;
	}
	eof_cursor_visible = 0;
	eof_render();
	eof_color_dialog(eof_lyric_dialog, gui_fg_color, gui_bg_color);
	centre_dialog(eof_lyric_dialog);
	ustrcpy(eof_etext, eof_song->vocal_track[tracknum]->lyric[eof_selection.current]->text);
	if(eof_popup_dialog(eof_lyric_dialog, 2) == 3)	//User hit OK on "Edit Lyric" dialog instead of canceling
	{
		if(eof_is_freestyle(eof_etext))		//If the text entered had one or more freestyle characters
			eof_set_freestyle(eof_etext,1);	//Perform any necessary corrections

		if(ustricmp(eof_song->vocal_track[tracknum]->lyric[eof_selection.current]->text, eof_etext))	//If the updated string (eof_etext) is different
		{
			eof_prepare_undo(EOF_UNDO_TYPE_NONE);
			if(!eof_check_string(eof_etext))
			{	//If the updated string is empty or just whitespace
				eof_track_delete_note(eof_song, eof_selected_track, eof_selection.current);
			}
			else
			{
				ustrcpy(eof_song->vocal_track[tracknum]->lyric[eof_selection.current]->text, eof_etext);
				eof_fix_lyric(eof_song->vocal_track[tracknum],eof_selection.current);	//Correct the freestyle character if necessary
			}
		}
	}
	eof_cursor_visible = 1;
	eof_pen_visible = 1;
	eof_show_mouse(screen);
	return D_O_K;
}

int eof_menu_set_freestyle(char status)
{
	unsigned long i=0,ctr=0;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(!eof_vocals_selected)
		return 1;

//Determine if any lyrics will actually be affected by this action
	if(eof_vocals_selected && (eof_selection.track == EOF_TRACK_VOCALS))
	{	//If lyrics are selected
		for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
		{	//For each lyric...
			if(eof_selection.multi[i])
			{	//...that is selected, count the number of lyrics that would be altered
				if(eof_lyric_is_freestyle(eof_song->vocal_track[tracknum],i) && (status == 0))
					ctr++;	//Increment if a lyric would change from freestyle to non freestyle
				else if(!eof_lyric_is_freestyle(eof_song->vocal_track[tracknum],i) && (status != 0))
					ctr++;	//Increment if a lyric would change from non freestyle to freestyle
			}
		}

//If so, make an undo state and perform the action on the lyrics
		if(ctr)
		{	//If at least one lyric is going to be modified
			eof_prepare_undo(EOF_UNDO_TYPE_NONE);	//Make an undo state

			for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
			{	//For each lyric...
				if(eof_selection.multi[i])
				{	//...that is selected, apply the specified freestyle status
					eof_set_freestyle(eof_song->vocal_track[tracknum]->lyric[i]->text,status);
				}
			}
		}
	}

	return 1;
}

int eof_menu_set_freestyle_on(void)
{
	return eof_menu_set_freestyle(1);
}

int eof_menu_set_freestyle_off(void)
{
	return eof_menu_set_freestyle(0);
}

int eof_menu_toggle_freestyle(void)
{
	unsigned long i=0;
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;

	if(eof_vocals_selected && (eof_selection.track == EOF_TRACK_VOCALS) && eof_count_selected_notes(NULL, 0))
	{	//If lyrics are selected
		eof_prepare_undo(EOF_UNDO_TYPE_NONE);	//Make an undo state

		for(i = 0; i < eof_song->vocal_track[tracknum]->lyrics; i++)
		{	//For each lyric...
			if(eof_selection.multi[i])
			{	//...that is selected, toggle its freestyle status
				eof_toggle_freestyle(eof_song->vocal_track[tracknum],i);
			}
		}
	}

	return 1;
}

char eof_fret1[4] = {0};	//Use a fourth byte to guarantee proper truncation
char eof_fret2[4] = {0};
char eof_fret3[4] = {0};
char eof_fret4[4] = {0};
char eof_fret5[4] = {0};
char eof_fret6[4] = {0};
char *eof_fret_strings[6] = {eof_fret1, eof_fret2, eof_fret3, eof_fret4, eof_fret5, eof_fret6};

DIALOG eof_pro_guitar_note_dialog[] =
{
/*	(proc)					(x)  (y)  (w)  (h) (fg) (bg) (key) (flags) (d1) (d2) (dp)          (dp2)          (dp3) */
	{d_agup_window_proc,    0,   48,  224, 308,2,   23,  0,    0,      0,   0,   "Edit pro guitar note",NULL, NULL },
	{d_agup_text_proc,      16,  104, 64,  8,  2,   23,  0,    0,      0,   0,   "String 1:",  NULL,          NULL },
	{eof_verified_edit_proc,74,  100, 22,  20, 2,   23,  0,    0,      2,   0,   eof_fret1,    "0123456789Xx",NULL },
	{d_agup_text_proc,      16,  128, 64,  8,  2,   23,  0,    0,      0,   0,   "String 2:",  NULL,          NULL },
	{eof_verified_edit_proc,74,  124, 22,  20, 2,   23,  0,    0,      2,   0,   eof_fret2,    "0123456789Xx",NULL },
	{d_agup_text_proc,      16,  152, 64,  8,  2,   23,  0,    0,      0,   0,   "String 3:",  NULL,          NULL },
	{eof_verified_edit_proc,74,  148, 22,  20, 2,   23,  0,    0,      2,   0,   eof_fret3,    "0123456789Xx",NULL },
	{d_agup_text_proc,      16,  176, 64,  8,  2,   23,  0,    0,      0,   0,   "String 4:",  NULL,          NULL },
	{eof_verified_edit_proc,74,  172, 22,  20, 2,   23,  0,    0,      2,   0,   eof_fret4,    "0123456789Xx",NULL },
	{d_agup_text_proc,      16,  200, 64,  8,  2,   23,  0,    0,      0,   0,   "String 5:",  NULL,          NULL },
	{eof_verified_edit_proc,74,  196, 22,  20, 2,   23,  0,    0,      2,   0,   eof_fret5,    "0123456789Xx",NULL },
	{d_agup_text_proc,      16,  224, 64,  8,  2,   23,  0,    0,      0,   0,   "String 6:",  NULL,          NULL },
	{eof_verified_edit_proc,74,  220, 22,  20, 2,   23,  0,    0,      2,   0,   eof_fret6,    "0123456789Xx",NULL },

	{d_agup_text_proc,      16,  84,  64,  8,  2,   23,  0,    0,      0,   0,   "Pro",        NULL,          NULL },
	{d_agup_text_proc,      124, 84,  64,  8,  2,   23,  0,    0,      0,   0,   "Legacy",     NULL,          NULL },
	{d_agup_check_proc,		122, 103, 64,  16, 2,   23,  0,    0,      0,   0,   "Lane 1",     NULL,          NULL },
	{d_agup_check_proc,		122, 127, 64,  16, 2,   23,  0,    0,      0,   0,   "Lane 2",     NULL,          NULL },
	{d_agup_check_proc,		122, 151, 64,  16, 2,   23,  0,    0,      0,   0,   "Lane 3",     NULL,          NULL },
	{d_agup_check_proc,		122, 175, 64,  16, 2,   23,  0,    0,      0,   0,   "Lane 4",     NULL,          NULL },
	{d_agup_check_proc,		122, 199, 64,  16, 2,   23,  0,    0,      0,   0,   "Lane 5",     NULL,          NULL },

	{d_agup_text_proc,      10,  268, 64,  8,  2,   23,  0,    0,      0,   0,   "Slide:",     NULL,          NULL },
	{d_agup_text_proc,      10,  288, 64,  8,  2,   23,  0,    0,      0,   0,   "Mute:",      NULL,          NULL },
	{d_agup_radio_proc,		10,  248, 38,  16, 2,   23,  0,    0,      1,   0,   "HO",         NULL,          NULL },
	{d_agup_radio_proc,		58,  248, 38,  16, 2,   23,  0,    0,      1,   0,   "PO",         NULL,          NULL },
	{d_agup_radio_proc,		102, 248, 45,  16, 2,   23,  0,    0,      1,   0,   "Tap",        NULL,          NULL },
	{d_agup_radio_proc,		154, 248, 50,  16, 2,   23,  0,    0,      1,   0,   "None",       NULL,          NULL },
	{d_agup_radio_proc,		58,  268, 38,  16, 2,   23,  0,    0,      2,   0,   "Up",         NULL,          NULL },
	{d_agup_radio_proc,		102, 268, 54,  16, 2,   23,  0,    0,      2,   0,   "Down",       NULL,          NULL },
	{d_agup_radio_proc,		154, 268, 64,  16, 2,   23,  0,    0,      2,   0,   "Neither",    NULL,          NULL },
	{d_agup_radio_proc,		46,  288, 58,  16, 2,   23,  0,    0,      3,   0,   "String",     NULL,          NULL },
	{d_agup_radio_proc,		102, 288, 52,  16, 2,   23,  0,    0,      3,   0,   "Palm",       NULL,          NULL },
	{d_agup_radio_proc,		154, 288, 64,  16, 2,   23,  0,    0,      3,   0,   "Neither",    NULL,          NULL },

	{d_agup_button_proc,    20,  316, 68,  28, 2,   23,  '\r', D_EXIT, 0,   0,   "OK",         NULL,          NULL },
	{d_agup_button_proc,    140, 316, 68,  28, 2,   23,  0,    D_EXIT, 0,   0,   "Cancel",     NULL,          NULL },
	{NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL }
};

int eof_menu_note_edit_pro_guitar_note(void)
{
	unsigned long tracknum = eof_song->track[eof_selected_track]->tracknum;
	unsigned long ctr, fretcount;
	char undo_made = 0;	//Set to nonzero when an undo state is created
	long fretvalue;
	char allmuted;					//Used to track whether all used strings are string muted
	unsigned long bitmask = 0;		//Used to build the updated pro guitar note bitmask
	unsigned long legacymask;		//Used to build the updated legacy note bitmask
	unsigned long flags;			//Used to build the updated flag bitmask

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run unless the pro guitar track is active

	if(eof_selection.current >= eof_track_get_size(eof_song, eof_selected_track))
		return 1;	//Do not allow this function to run if a valid note isn't selected

	if(!eof_music_paused)
	{
		eof_music_play();
	}

	eof_cursor_visible = 0;
	eof_pen_visible = 0;
	eof_render();
	eof_color_dialog(eof_pro_guitar_note_dialog, gui_fg_color, gui_bg_color);
	centre_dialog(eof_pro_guitar_note_dialog);

//Update the fret text boxes
	fretcount = eof_count_track_lanes(eof_song, eof_selected_track);
	for(ctr = 0, bitmask = 1; ctr < 6; ctr++, bitmask<<=1)
	{	//For each of the 6 supported strings
		if(ctr < fretcount)
		{	//If this track uses this string, copy the fret value to the appropriate string
			eof_pro_guitar_note_dialog[12 - (2 * ctr)].flags = 0;	//Ensure this text box is enabled
			if(eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->note & bitmask)
			{	//If this string is already defined as being in use, copy its fret value to the string
				if(eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->frets[ctr] == 0xFF)
				{	//If this string is muted
					snprintf(eof_fret_strings[ctr], 3, "X");
				}
				else
				{
					snprintf(eof_fret_strings[ctr], 3, "%d", eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->frets[ctr]);
				}
			}
			else
			{	//Otherwise empty the string
				eof_fret_strings[ctr][0] = '\0';
			}
		}
		else
		{	//Otherwise disable the text box for this fret and empty the string
			eof_pro_guitar_note_dialog[12 - (2 * ctr)].flags = D_DISABLED;	//Ensure this text box is disabled
			eof_fret_strings[ctr][0] = '\0';
		}
	}

//Update the legacy bitmask checkboxes
	legacymask = eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->legacymask;
	eof_pro_guitar_note_dialog[15].flags = (legacymask & 1) ? D_SELECTED : 0;
	eof_pro_guitar_note_dialog[16].flags = (legacymask & 2) ? D_SELECTED : 0;
	eof_pro_guitar_note_dialog[17].flags = (legacymask & 4) ? D_SELECTED : 0;
	eof_pro_guitar_note_dialog[18].flags = (legacymask & 8) ? D_SELECTED : 0;
	eof_pro_guitar_note_dialog[19].flags = (legacymask & 16) ? D_SELECTED : 0;

//Update the note flag radio buttons
	for(ctr = 0; ctr < 10; ctr++)
	{	//Clear each of the 10 status radio buttons
		eof_pro_guitar_note_dialog[22 + ctr].flags = 0;
	}
	flags = eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->flags;
	if(flags & EOF_PRO_GUITAR_NOTE_FLAG_HO)
	{	//Select "HO"
		eof_pro_guitar_note_dialog[22].flags = D_SELECTED;
	}
	else if(flags & EOF_PRO_GUITAR_NOTE_FLAG_PO)
	{	//Select "PO"
		eof_pro_guitar_note_dialog[23].flags = D_SELECTED;
	}
	else if(flags & EOF_PRO_GUITAR_NOTE_FLAG_TAP)
	{	//Select "Tap"
		eof_pro_guitar_note_dialog[24].flags = D_SELECTED;
	}
	else
	{	//Select "None"
		eof_pro_guitar_note_dialog[25].flags = D_SELECTED;
	}
	if(flags & EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP)
	{	//Select "Up"
		eof_pro_guitar_note_dialog[26].flags = D_SELECTED;
	}
	else if(flags & EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN)
	{	//Select "Down"
		eof_pro_guitar_note_dialog[27].flags = D_SELECTED;
	}
	else
	{	//Select "Neither"
		eof_pro_guitar_note_dialog[28].flags = D_SELECTED;
	}
	if(flags & EOF_PRO_GUITAR_NOTE_FLAG_STRING_MUTE)
	{	//Select "String"
		eof_pro_guitar_note_dialog[29].flags = D_SELECTED;
	}
	else if(flags &EOF_PRO_GUITAR_NOTE_FLAG_PALM_MUTE)
	{	//Select "Palm"
		eof_pro_guitar_note_dialog[30].flags = D_SELECTED;
	}
	else
	{	//Select "Neither"
		eof_pro_guitar_note_dialog[31].flags = D_SELECTED;
	}

	bitmask = 0;
	if(eof_popup_dialog(eof_pro_guitar_note_dialog, 0) == 32)
	{	//If user clicked OK
		//Validate and store the input
		for(ctr = 0, allmuted = 1; ctr < 6; ctr++)
		{	//For each of the 6 supported strings
			if(eof_fret_strings[ctr][0] != '\0')
			{	//If this string isn't empty, set the fret value
				bitmask |= (1 << ctr);	//Set the appropriate bit for this lane
				if(toupper(eof_fret_strings[ctr][0]) == 'X')
				{	//If the user defined this string as muted
					fretvalue = 0xFF;
				}
				else
				{	//Get the appropriate fret value
					fretvalue = atol(eof_fret_strings[ctr]);
					if((fretvalue < 0) || (fretvalue > eof_song->pro_guitar_track[tracknum]->numfrets))
					{	//If the conversion to number failed, or an invalid fret number was entered, enter a value of (muted) for the string
						fretvalue = 0xFF;
					}
				}
				if(!undo_made && (fretvalue != eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->frets[ctr]))
				{	//If an undo state hasn't been made yet, and this fret value changed
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);
					undo_made = 1;
				}
				eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->frets[ctr] = fretvalue;
				if(fretvalue != 0xFF)
				{	//Track whether the all used strings in this note/chord are muted
					allmuted = 0;
				}
			}
			else
			{	//Clear the fret value and return the fret back to its default of 0 (open)
				bitmask &= ~(1 << ctr);	//Clear the appropriate bit for this lane
				if(!undo_made && (eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->frets[ctr] != 0))
				{	//If an undo state hasn't been made yet, and this fret value changed
					eof_prepare_undo(EOF_UNDO_TYPE_NONE);
					undo_made = 1;
				}
				eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->frets[ctr] = 0;
			}
		}
		if(bitmask == 0)
		{	//If edits results in this note having no played strings
			eof_track_delete_note(eof_song, eof_selected_track, eof_selection.current);	//Delete this note because it no longer exists
			return 1;
		}

//Save the updated note bitmask
		if(!undo_made && (eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->note != bitmask))
		{	//If an undo state hasn't been made yet, and the note bitmask changed
			eof_prepare_undo(EOF_UNDO_TYPE_NONE);
			undo_made = 1;
		}
		eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->note = bitmask;

//Save the updated legacy note bitmask
		legacymask = 0;
		if(eof_pro_guitar_note_dialog[15].flags == D_SELECTED)
			legacymask |= 1;
		if(eof_pro_guitar_note_dialog[16].flags == D_SELECTED)
			legacymask |= 2;
		if(eof_pro_guitar_note_dialog[17].flags == D_SELECTED)
			legacymask |= 4;
		if(eof_pro_guitar_note_dialog[18].flags == D_SELECTED)
			legacymask |= 8;
		if(eof_pro_guitar_note_dialog[19].flags == D_SELECTED)
			legacymask |= 16;
		if(!undo_made && (legacymask != eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->legacymask))
		{
			eof_prepare_undo(EOF_UNDO_TYPE_NONE);
			undo_made = 1;
		}
		eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->legacymask = legacymask;

//Save the updated note flag bitmask
		flags = 0;
		if(eof_pro_guitar_note_dialog[22].flags == D_SELECTED)
		{
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_HO;	//Set the hammer on flag
			flags &= (~EOF_NOTE_FLAG_NO_HOPO);		//Clear the forced HOPO off note
			flags |= EOF_NOTE_FLAG_F_HOPO;			//Set the legacy HOPO flag
		}
		else if(eof_pro_guitar_note_dialog[23].flags == D_SELECTED)
		{
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_PO;	//Set the pull off flag
			flags &= (~EOF_NOTE_FLAG_NO_HOPO);		//Clear the forced HOPO off flag
			flags |= EOF_NOTE_FLAG_F_HOPO;			//Set the legacy HOPO flag
		}
		else if(eof_pro_guitar_note_dialog[24].flags == D_SELECTED)
		{
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_TAP;	//Set the tap flag
			flags &= (~EOF_NOTE_FLAG_NO_HOPO);		//Clear the forced HOPO off flag
			flags |= EOF_NOTE_FLAG_F_HOPO;			//Set the legacy HOPO flag
		}
		if(eof_pro_guitar_note_dialog[26].flags == D_SELECTED)
		{
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP;
		}
		else if(eof_pro_guitar_note_dialog[27].flags == D_SELECTED)
		{
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN;
		}
		if(eof_pro_guitar_note_dialog[29].flags == D_SELECTED)
		{
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_STRING_MUTE;
		}
		else if(eof_pro_guitar_note_dialog[30].flags == D_SELECTED)
		{
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_PALM_MUTE;
		}
		if(!allmuted)
		{	//If any used strings in this note/chord weren't string muted
			flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_STRING_MUTE);	//Clear the string mute flag
		}
		else if(!(flags & EOF_PRO_GUITAR_NOTE_FLAG_PALM_MUTE))
		{	//If all strings are muted and the user didn't specify a palm mute
			flags |= EOF_PRO_GUITAR_NOTE_FLAG_STRING_MUTE;		//Set the string mute flag
		}
		if(!undo_made && (flags != eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->flags))
		{
			eof_prepare_undo(EOF_UNDO_TYPE_NONE);
			undo_made = 1;
		}
		eof_song->pro_guitar_track[tracknum]->note[eof_selection.current]->flags = flags;
	}
	eof_show_mouse(NULL);
	eof_cursor_visible = 1;
	eof_pen_visible = 1;
	return 1;
}

int eof_menu_note_toggle_tapping(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags;

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run when a pro guitar format track is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			flags ^= EOF_PRO_GUITAR_NOTE_FLAG_TAP;	//Toggle the tap flag
			if(flags & EOF_PRO_GUITAR_NOTE_FLAG_TAP)
			{	//If tapping was just enabled
				flags |= EOF_NOTE_FLAG_F_HOPO;			//Set the legacy HOPO on flag (no strum required for this note)
				flags &= ~(EOF_NOTE_FLAG_NO_HOPO);		//Clear the HOPO off flag
				flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_HO);	//Clear the hammer on flag
				flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_PO);	//Clear the pull off flag
			}
			else
			{	//If tapping was just disabled
				flags &= ~(EOF_NOTE_FLAG_F_HOPO);		//Clear the legacy HOPO on flag
			}
			if(!u)
			{	//Make a back up before changing the first note
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}

int eof_menu_note_remove_tapping(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags, oldflags;

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run when a pro guitar format track is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			oldflags = flags;							//Save an extra copy of the original flags
			flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_TAP);	//Clear the tap flag
			flags &= ~(EOF_NOTE_FLAG_F_HOPO);			//Clear the legacy HOPO on flag
			if(!u && (oldflags != flags))
			{	//Make a back up before changing the first note
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}

int eof_menu_note_toggle_slide_up(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags;

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run when a pro guitar format track is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			flags ^= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP;			//Toggle the slide up flag
			flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN);	//Clear the slide down flag
			if(!u)
			{	//Make a back up before changing the first note
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}

int eof_menu_note_toggle_slide_down(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags;

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run when a pro guitar format track is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			flags ^= EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN;		//Toggle the slide down flag
			flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP);		//Clear the slide down flag
			if(!u)
			{	//Make a back up before changing the first note
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}

int eof_menu_note_remove_slide(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags, oldflags;

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run when a pro guitar format track is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			oldflags = flags;							//Save an extra copy of the original flags
			flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_UP);		//Clear the tap flag
			flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_SLIDE_DOWN);	//Clear the tap flag
			if(!u && (oldflags != flags))
			{	//Make a back up before changing the first note
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}

int eof_menu_note_toggle_palm_muting(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags;

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run when a pro guitar format track is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			flags ^= EOF_PRO_GUITAR_NOTE_FLAG_PALM_MUTE;		//Toggle the palm mute flag
			flags &= ~(EOF_PRO_GUITAR_NOTE_FLAG_STRING_MUTE);	//Clear the string mute flag
			if(!u)
			{	//Make a back up before changing the first note
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}

int eof_menu_note_remove_palm_muting(void)
{
	unsigned long i;
	long u = 0;
	unsigned long flags, oldflags;

	if(eof_song->track[eof_selected_track]->track_format != EOF_PRO_GUITAR_TRACK_FORMAT)
		return 1;	//Do not allow this function to run when a pro guitar format track is not active

	for(i = 0; i < eof_track_get_size(eof_song, eof_selected_track); i++)
	{	//For each note in the active track
		if((eof_selection.track == eof_selected_track) && eof_selection.multi[i])
		{	//If this note is in the currently active track and is selected
			flags = eof_get_note_flags(eof_song, eof_selected_track, i);
			oldflags = flags;								//Save an extra copy of the original flags
			flags &= (~EOF_PRO_GUITAR_NOTE_FLAG_PALM_MUTE);	//Clear the palm mute flag
			if(!u && (oldflags != flags))
			{	//Make a back up before changing the first note
				eof_prepare_undo(EOF_UNDO_TYPE_NONE);
				u = 1;
			}
			eof_set_note_flags(eof_song, eof_selected_track, i, flags);
		}
	}
	return 1;
}
