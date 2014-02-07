#include <allegro.h>
#include "agup/agup.h"
#include "modules/wfsel.h"
#include "modules/gametime.h"
#include "modules/g-idle.h"
#include "foflc/Lyric_storage.h"
#include "foflc/Midi_parse.h"
#include "menu/main.h"
#include "menu/file.h"
#include "menu/edit.h"
#include "menu/song.h"
#include "menu/track.h"
#include "menu/note.h"
#include "menu/beat.h"
#include "menu/help.h"
#include "menu/context.h"
#include "lc_import.h"
#include "main.h"
#include "player.h"
#include "ini.h"
#include "editor.h"
#include "window.h"
#include "dialog.h"
#include "beat.h"
#include "event.h"
#include "midi.h"
#include "undo.h"
#include "mix.h"
#include "tuning.h"
#include "menu/file.h"
#include "menu/edit.h"
#include "menu/song.h"
#include "menu/note.h"
#include "menu/beat.h"
#include "menu/help.h"

#ifdef USEMEMWATCH
#include "memwatch.h"
#endif

#ifdef ALLEGRO_WINDOWS
	char * eof_system_slash = "\\";
#else
	char * eof_system_slash = "/";
#endif

/* editable/dynamic text fields */
char eof_etext[1024] = {0};
char eof_etext2[1024] = {0};
char eof_etext3[1024] = {0};
char eof_etext4[1024] = {0};
char eof_etext5[1024] = {0};
char eof_etext6[1024] = {0};
char eof_etext7[1024] = {0};
char eof_etext8[1024] = {0};
char *eof_help_text = NULL;
char eof_ctext[13][1024] = {{0}};

static int eof_keyboard_shortcut = 0;

void eof_prepare_menus(void)
{
	eof_log("eof_prepare_menus() entered", 2);

	eof_prepare_main_menu();
	eof_prepare_beat_menu();
	eof_prepare_file_menu();
	eof_prepare_edit_menu();
	eof_prepare_song_menu();
	eof_prepare_track_menu();
	eof_prepare_note_menu();
	eof_prepare_context_menu();
}

void eof_color_dialog(DIALOG * dp, int fg, int bg)
{
	int i;

	eof_log("eof_color_dialog() entered", 2);

	if(dp == NULL)
		return;	//Invalid DIALOG pointer

	for(i = 0; dp[i].proc != NULL; i++)
	{
		dp[i].fg = fg;
		dp[i].bg = bg;
	}
}

int eof_popup_dialog(DIALOG * dp, int n)
{
	int ret;
	int oldlen = 0;
	int dflag = 0;
	DIALOG_PLAYER * player;

	eof_log("eof_popup_dialog() entered", 2);

	eof_prepare_menus();
	player = init_dialog(dp, n);
	eof_show_mouse(screen);
	clear_keybuf();
	while(1)
	{
		/* Read the keyboard input and simulate the keypresses so the dialog
		 * player can pick up keyboard input after we intercepted it. This lets
		 * us be aware of any keyboard input so we can react accordingly. */
		eof_read_keyboard_input();
		if(eof_key_pressed)
		{
			if(eof_key_char)
			{
				simulate_ukeypress(eof_key_uchar, eof_key_code);
			}
			else if(eof_key_code)
			{
				simulate_ukeypress(0, eof_key_code);
			}
		}
		if(!update_dialog(player))
		{
			break;
		}
		/* special handling of the main menu */
		if(dp[0].proc == d_agup_menu_proc)
		{
			/* use has opened the menu with a shortcut key */
			if(eof_key_char == 'f' || eof_key_char == 'e' || eof_key_char == 's' || eof_key_char == 't' || eof_key_char == 'n' || eof_key_char == 'b' || eof_key_char == 'h')
			{
				eof_keyboard_shortcut = 1;
				player->mouse_obj = 0;
			}

			/* detect if menu was activated with a click */
			if(mouse_b & 1)
			{
				eof_keyboard_shortcut = 2;
			}
			
			/* allow menu to be closed if mouse button released after it was
			 * opened with a click */
			if((eof_keyboard_shortcut == 2) && !(mouse_b & 1))
			{
				eof_keyboard_shortcut = 0;
			}

			/* if mouse isn't hovering over the menu, try and deactivate it */
			if(player->mouse_obj < 0)
			{
				/* if the user isn't about to press a menu shortcut */
				if(!KEY_EITHER_ALT && !eof_keyboard_shortcut)
				{
					break;
				}
			}
		}

		/* special handling of the song properties box */
		if(dp == eof_song_properties_dialog)
		{
			if(ustrlen(dp[14].dp) != oldlen)
			{
				(void) object_message(&dp[14], MSG_DRAW, 0);
				oldlen = ustrlen(dp[14].dp);
			}
		}

		/* special handling of new project dialog */
		if(dp == eof_file_new_windows_dialog)
		{
			if((dp[3].flags & D_SELECTED) && (ustrlen(eof_etext4) <= 0))
			{
				dp[5].flags = D_DISABLED;
				(void) object_message(&dp[5], MSG_DRAW, 0);
				dflag = 1;
			}
			else
			{
				dp[5].flags = D_EXIT;
				if(dflag)
				{
					(void) object_message(&dp[5], MSG_DRAW, 0);
					dflag = 0;
				}
			}
		}
		/* clear keyboard data so it doesn't show up in the next run of the
		 * loop */
		if(eof_key_pressed)
		{
			eof_use_key();
		}
		
		Idle(10);
	}
	ret = shutdown_dialog(player);

//	ret = popup_dialog(dp, n);
	eof_clear_input();
	gametime_reset();
	eof_show_mouse(NULL);
	eof_keyboard_shortcut = 0;

	return ret;
}
