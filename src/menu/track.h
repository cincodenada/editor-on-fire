#ifndef EOF_MENU_TRACK_H
#define EOF_MENU_TRACK_H

#include "../song.h"

extern MENU eof_track_menu[];
extern MENU eof_track_rocksmith_menu[];
extern MENU eof_track_proguitar_menu[];

extern DIALOG eof_pro_guitar_tuning_dialog[];
extern DIALOG eof_note_set_num_frets_strings_dialog[];
extern DIALOG eof_fret_hand_position_list_dialog[];

extern char eof_fret_hand_position_list_dialog_undo_made;	//Used to track an undo state having been made in the fret hand positions dialog
extern char eof_fret_hand_position_list_dialog_title_string[30];	//This will be rebuilt by the list box count function to display the number of positions present

int eof_track_difficulty_dialog(void);	//Allow the active track's difficulty to be set to a value from 0 through 6
int eof_track_rocksmith_toggle_difficulty_limit(void);
	//Toggles the 5 difficulty limit on/off.  If difficulties >= 4 are populated, appropriate warnings are given to the user regarding Rock Band vs. Rocksmith standards compatibility
int eof_track_rocksmith_insert_difficulty(void);
	//If the active pro guitar/bass track has fewer than 255 difficulties, prompts user to insert a new difficulty above or below the active difficulty
	//If the new difficulty has a populated difficulty above or below it, the user is prompted whether to import either (calling paste from logic)
int eof_track_rocksmith_delete_difficulty(void);
	//Deletes the active difficulty's content, decrementing all higher difficulty's content accordingly

int eof_track_rename(void);				//Allows the user to define an alternate name for the active track

int eof_track_tuning(void);				//Allows the active track's tuning to be defined
int eof_edit_tuning_proc(int msg, DIALOG *d, int c);
	//This is a modification of eof_verified_edit_proc() allowing the note names and tuning name to be redrawn when a tuning field is altered
int eof_track_set_num_frets_strings(void);
	//Allows the track's number of frets (pro guitar/bass) and strings (pro bass only) to be defined
int eof_track_proguitar_toggle_ignore_tuning(void);
	//Toggles the option to have the chord name detection disregard the track's defined tuning and base it on standard tuning instead
int eof_track_transpose_tuning(EOF_PRO_GUITAR_TRACK* tp, char *tuningdiff);
	//Receives a pointer to an array of 6 signed chars indicating a subtractive tuning offset (in half steps) for each string
	//If any existing notes in the specified track will be affected by the tuning change, the user is prompted whether or not to transpose the notes
	//If so, the affected fret values will be raised or lowered accordingly
	//If a note cannot be altered (ie. values of 0 or the track's max fret number would be surpassed), the user is warned and the note is highlighted

int eof_track_proguitar_set_fret_hand_position(void);
	//Allows the user to define a fret hand position for the active difficulty
int eof_track_fret_hand_positions(void);
	//Displays the fret hand positions defined for the active track difficulty, allowing them to be deleted
int eof_track_fret_hand_positions_copy_from(void);
	//Allows the user to copy fret hand positions defined in another difficulty into the current track difficulty
char * eof_track_fret_hand_positions_copy_from_list(int index, int * size);
	//The list dialog function for eof_track_fret_hand_positions_copy_from()

int eof_track_rs_popup_add(void);
	//Allows the user to add a popup message displayed in-game in Rocksmith
int eof_track_rs_popup_messages(void);
	//Displays the popup messages defined for the current pro guitar/bass track
char * eof_track_rs_popup_messages_list(int index, int * size);
	//List dialog function for eof_track_rs_popup_messages()
int eof_track_rs_popup_messages_edit(DIALOG * d);
	//Allows the user to edit a popup message from the list
void eof_track_pro_guitar_sort_popup_messages(EOF_PRO_GUITAR_TRACK* tp);
	//Sorts the popup messages for the specified track
int eof_track_rs_popup_messages_delete(DIALOG * d);
	//Deletes the selected popup message from eof_track_rs_popup_messages()
int eof_track_rs_popup_messages_delete_all(DIALOG * d);
	//Deletes all popup messages from eof_track_rs_popup_messages()
int eof_track_rs_popup_messages_seek(DIALOG * d);
	//Seeks to the selected popup message from eof_track_rs_popup_messages()
void eof_track_pro_guitar_delete_popup_message(EOF_PRO_GUITAR_TRACK *tp, unsigned long index);
	//Deletes the specified popup message
int eof_find_effective_rs_popup_message(unsigned long pos, unsigned long *popupnum);
	//Returns nonzero if there is a popup message in effect for the current pro guitar/bass track at the specified position
	//The corresponding popup message number for the track is returned through popupnum
	//Zero is returned if no popup message is in effect or upon error

int eof_track_rs_tone_change_add(void);
	//Allows the user to add a tone change for use in Rocksmith
int eof_track_rs_tone_changes(void);
	//Displays the tone changes defined for the current pro guitar/bass track
char * eof_track_rs_tone_changes_list(int index, int * size);
	//List dialog function for eof_track_rs_tone_changes()
int eof_track_rs_tone_changes_edit(DIALOG * d);
	//Allows the user to edit a tone change from the list
void eof_track_pro_guitar_sort_tone_changes(EOF_PRO_GUITAR_TRACK* tp);
	//Sorts the tone changes for the specified track
int eof_track_rs_tone_changes_delete(DIALOG * d);
	//Deletes the selected tone change from eof_track_rs_tone_changes()
int eof_track_rs_tone_changes_delete_all(DIALOG * d);
	//Deletes all tone changes from eof_track_rs_tone_changes()
int eof_track_rs_tone_changes_seek(DIALOG * d);
	//Seeks to the selected tone change from eof_track_rs_tone_changes()
void eof_track_pro_guitar_delete_tone_change(EOF_PRO_GUITAR_TRACK *tp, unsigned long index);
	//Deletes the specified tone change
int eof_track_find_effective_rs_tone_change(unsigned long pos, unsigned long *changenum);
	//Returns nonzero if there is a tone change for the current pro guitar/bass track at or before the specified position
	//The corresponding tone change number for the track is returned through changenum
	//Zero is returned if no tone change is in effect or upon error

int eof_track_rocksmith_arrangement_set(unsigned char num);
	//Validates that the active track is a pro guitar track and sets the track's arrangement type to the specified number
	//If num is > 4, a value of 0 is set instead
int eof_track_rocksmith_arrangement_undefined(void);	//Sets an undefined arrangement type
int eof_track_rocksmith_arrangement_combo(void);		//Sets a combo arrangement type
int eof_track_rocksmith_arrangement_rhythm(void);	//Sets a rhythm arrangement type
int eof_track_rocksmith_arrangement_lead(void);		//Sets a lead arrangement type
int eof_track_rocksmith_arrangement_bass(void);		//Sets a bass arrangement type

int eof_track_manage_rs_phrases(void);
	//Displays the phrases defined for the current pro guitar/bass track, along with the maxdifficulty of each
int eof_track_manage_rs_phrases_seek(DIALOG * d);
	//Seeks to the beat containing the selected phrase
int eof_track_manage_rs_phrases_add_or_remove_level(int function);
	//Used by eof_track_manage_rs_phrases_add_level() and eof_track_manage_rs_phrases_delete_level() to manipulate the difficulties for the selected RS phrase
	//If function is negative, the notes, arpeggios, hand positions and tremolos in the active difficulty are deleted, and those of higher difficulties are decremented
	//If function is >= 0, the notes, arpeggios, hand positions and tremolos in the active difficulty are duplicated into the next difficulty, and those of higher difficulties are incremented
	//	Also, the presence of a hand position at the first note of the leveled up phrase is enforced, as this is done by official Rocksmith charts to ensure hand positions stay correct despite leveling up/down
int eof_track_manage_rs_phrases_add_level(DIALOG * d);
	//Increments the difficulty level of the notes, arpeggios and hand positions in the selected phrase (either the selected instance or within all instances, depending on user selection)
	//and copies the now higher-difficulty's content back into the active difficulty, making it convenient to author a new difficulty level for the notes in question.
int eof_track_manage_rs_phrases_remove_level(DIALOG * d);
	//Decrements the notes within the selected phrase (either the selected instance or within all instances, depending on user selection)
	//and deletes the affected notes in the current difficulty, making it convenient to remove a difficulty level for the notes in question.

int eof_track_flatten_difficulties(void);	//Launches a dialog to specify a threshold distance and calls eof_flatten_difficulties on the active track difficulty
int eof_track_unflatten_difficulties(void);	//Runs eof_unflatten_difficulties() on the active track difficulty

void eof_prepare_track_menu(void);

int eof_menu_track_open_bass(void);			//Toggle the ability to use a sixth lane in PART BASS on/off
int eof_menu_track_five_lane_drums(void);	//Toggle the ability to use a fifth lane in PART DRUM on/off
int eof_menu_track_disable_double_bass_drums(void);	//Toggle the setting to disable expert+ bass drum gem export on/off

int eof_track_erase_track(void);			//Allows the user to erase the active track's contents
int eof_track_erase_track_difficulty(void);	//Allows the user to erase the active track difficulty's contents

int eof_menu_track_remove_highlighting(void);	//Removes highlighting from the notes in the active track
int eof_menu_track_highlight_arpeggios(void);	//Enables highlighting for all notes in the active track that are within arpeggio phrases

extern MENU eof_menu_track_rocksmith_popup_copy_menu[EOF_TRACKS_MAX];
extern char eof_menu_track_rocksmith_popup_copy_menu_text[EOF_TRACKS_MAX][EOF_TRACK_NAME_SIZE];
int eof_menu_track_copy_popups_track_1(void);
int eof_menu_track_copy_popups_track_2(void);
int eof_menu_track_copy_popups_track_3(void);
int eof_menu_track_copy_popups_track_4(void);
int eof_menu_track_copy_popups_track_5(void);
int eof_menu_track_copy_popups_track_6(void);
int eof_menu_track_copy_popups_track_7(void);
int eof_menu_track_copy_popups_track_8(void);
int eof_menu_track_copy_popups_track_9(void);
int eof_menu_track_copy_popups_track_10(void);
int eof_menu_track_copy_popups_track_11(void);
int eof_menu_track_copy_popups_track_12(void);
int eof_menu_track_copy_popups_track_13(void);
int eof_menu_track_copy_popups_track_number(EOF_SONG *sp, int sourcetrack, int desttrack);
	//Copies the popup messages from the specified source track to the destination track

#endif
