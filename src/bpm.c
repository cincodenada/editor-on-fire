#include <alogg.h>
#include "minibpm-1.0/src/minibpm-c.h"

static MiniBPMState eof_bpm_estimator_state;
static float * eof_bpm_estimator_buffer = NULL;

static void eof_bpm_estimator_callback(void * buffer, int nsamples, int stereo)
{
	int i;
	unsigned short * buffer_sp = (unsigned short *)buffer;
	int iter = stereo ? 2 : 1;
	for(i = 0; i < nsamples / iter; i++)
	{
		eof_bpm_estimator_buffer[i] = (float)((long)buffer_sp[i * iter] - 0x8000) / (float)0x8000;
		if(stereo)
		{
			eof_bpm_estimator_buffer[i] += (float)((long)buffer_sp[i * iter + 1] - 0x8000) / (float)0x8000;
			eof_bpm_estimator_buffer[i] /= 2.0;
		}
	}
	minibpm_process(eof_bpm_estimator_state, eof_bpm_estimator_buffer, nsamples / iter);
}


double eof_estimate_bpm(ALOGG_OGG * ogg)
{
	double bpm = 0.0;

	eof_bpm_estimator_state = minibpm_new((float)alogg_get_wave_freq_ogg(ogg));
	if(eof_bpm_estimator_state)
	{
		eof_bpm_estimator_buffer = malloc(sizeof(float) * 1024); // 1024 mono floating point samples
		if(eof_bpm_estimator_buffer)
		{
			(void) alogg_process_ogg(ogg, eof_bpm_estimator_callback, 1024, 0.0, 0.0);
			bpm = minibpm_estimate_tempo(eof_bpm_estimator_state);
			free(eof_bpm_estimator_buffer);
		}
		minibpm_delete(eof_bpm_estimator_state);
	}
	return bpm;
}
