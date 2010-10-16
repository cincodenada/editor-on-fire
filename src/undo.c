#include "main.h"
#include "menu/edit.h"
#include "dialog.h"
#include "undo.h"
#include "editor.h"
#include "utility.h"

//EOF_UNDO_STATE eof_undo[EOF_MAX_UNDO];
//EOF_UNDO_STATE eof_redo;
char * eof_undo_filename[EOF_MAX_UNDO] = {"eof.undo0", "eof.undo1", "eof.undo2", "eof.undo3", "eof.undo4", "eof.undo5", "eof.undo6", "eof.undo7"};
int eof_undo_index[EOF_MAX_UNDO] = {0};
int eof_undo_type[EOF_MAX_UNDO] = {0};
int eof_undo_last_type = 0;
int eof_undo_current_index = 0;
int eof_undo_count = 0;
int eof_redo_count = 0;
int eof_redo_type = 0;

int eof_undo_load_state(const char * fn)
{
	PACKFILE * fp;
	char rheader[16];

	fp = pack_fopen(fn, "r");
	if(!fp)
	{
		return 0;
	}
	if(pack_fread(rheader, 16, fp) != 16)
		return 0;	//Return error if 16 bytes cannot be read
	eof_load_song_pf(eof_song, fp);
	pack_fclose(fp);
	return 1;
}

void eof_undo_reset(void)
{
	int i;

	for(i = 0; i < EOF_MAX_UNDO; i++)
	{
		eof_undo_index[i] = 0;
	}
	eof_undo_current_index = 0;
	eof_undo_count = 0;
	eof_redo_count = 0;
}

int eof_undo_add(int type)
{
	char fn[1024] = {0};
	
	if((type == EOF_UNDO_TYPE_NOTE_LENGTH) && (eof_undo_last_type == EOF_UNDO_TYPE_NOTE_LENGTH))
	{
		return 0;
	}
	if((type == EOF_UNDO_TYPE_LYRIC_NOTE) && (eof_undo_last_type == EOF_UNDO_TYPE_LYRIC_NOTE))
	{
		return 0;
	}
	if((type == EOF_UNDO_TYPE_RECORD) && (eof_undo_last_type == EOF_UNDO_TYPE_RECORD))
	{
		return 0;
	}
	if(type == EOF_UNDO_TYPE_SILENCE)
	{
		sprintf(fn, "%s.ogg", eof_undo_filename[eof_undo_current_index]);
		eof_copy_file(eof_loaded_ogg_name, fn);
	}
	eof_undo_last_type = type;
	eof_save_song(eof_song, eof_undo_filename[eof_undo_current_index]);
	eof_undo_type[eof_undo_current_index] = type;
	eof_undo_current_index++;
	if(eof_undo_current_index >= EOF_MAX_UNDO)
	{
		eof_undo_current_index = 0;
	}
	eof_undo_count++;
	if(eof_undo_count >= EOF_MAX_UNDO)
	{
		eof_undo_count = EOF_MAX_UNDO;
	}
	return 1;
}

int eof_undo_apply(void)
{
	char fn[1024] = {0};
	
	if(eof_undo_count > 0)
	{
		eof_save_song(eof_song, "eof.redo");
		eof_redo_type = 0;
		eof_undo_current_index--;
		if(eof_undo_current_index < 0)
		{
			eof_undo_current_index = EOF_MAX_UNDO - 1;
		}
		eof_undo_load_state(eof_undo_filename[eof_undo_current_index]);
		if(eof_undo_type[eof_undo_current_index] == EOF_UNDO_TYPE_NOTE_SEL)
		{
			eof_menu_edit_deselect_all();
		}
		if(eof_undo_type[eof_undo_current_index] == EOF_UNDO_TYPE_SILENCE)
		{
			sprintf(fn, "%s.ogg", eof_undo_filename[eof_undo_current_index]);
			eof_copy_file(fn, eof_loaded_ogg_name);
			eof_load_ogg(eof_loaded_ogg_name);
			eof_fix_waveform_graph();
			eof_redo_type = EOF_UNDO_TYPE_SILENCE;
		}
		eof_undo_count--;
		eof_redo_count = 1;
		eof_change_count--;
		if(eof_change_count == 0)
		{
			eof_changes = 0;
		}
		else
		{
			eof_changes = 1;
		}
		eof_undo_last_type = 0;

		eof_detect_difficulties(eof_song);
		eof_select_beat(eof_selected_beat);
		eof_fix_catalog_selection();
		eof_fix_window_title();
		return 1;
	}
	return 0;
}

void eof_redo_apply(void)
{
	char fn[1024] = {0};
	
	if(eof_redo_count > 0)
	{
		eof_save_song(eof_song, eof_undo_filename[eof_undo_current_index]);
		eof_undo_current_index++;
		if(eof_undo_current_index >= EOF_MAX_UNDO)
		{
			eof_undo_current_index = 0;
		}
		eof_undo_load_state("eof.redo");
		if(eof_redo_type == EOF_UNDO_TYPE_SILENCE)
		{
			sprintf(fn, "%s.ogg", eof_undo_filename[eof_undo_current_index]);
			eof_copy_file(fn, eof_loaded_ogg_name);
			eof_load_ogg(eof_loaded_ogg_name);
			eof_fix_waveform_graph();
		}
		eof_undo_count++;
		if(eof_undo_count >= EOF_MAX_UNDO)
		{
			eof_undo_count = EOF_MAX_UNDO;
		}
		eof_redo_count = 0;
		eof_change_count++;
		if(eof_change_count == 0)
		{
			eof_changes = 0;
		}
		else
		{
			eof_changes = 1;
		}
		eof_detect_difficulties(eof_song);
		eof_select_beat(eof_selected_beat);
		eof_fix_catalog_selection();
		eof_fix_window_title();
	}
}
