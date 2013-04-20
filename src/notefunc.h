#define BASE_FREQ       16.3515978312874
extern char* notenames;
extern char returnval[10];

double eof_note_to_freq(char note[3]);
char* eof_freq_to_note(double freq);
