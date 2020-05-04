///////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------//
//-------------------------------------------------------------------------------//
//-----------H----H--X----X-----CCCCC----22222----0000-----0000------11----------//
//----------H----H----X-X-----C--------------2---0----0---0----0--1--1-----------//
//---------HHHHHH-----X------C----------22222---0----0---0----0-----1------------//
//--------H----H----X--X----C----------2-------0----0---0----0-----1-------------//
//-------H----H---X-----X---CCCCC-----222222----0000-----0000----1111------------//
//-------------------------------------------------------------------------------//
//----------------------------------------------------- http://hxc2001.free.fr --//
///////////////////////////////////////////////////////////////////////////////////
// File : hxcmod.h
// Contains: a tiny mod player
//
// Written by: Jean Fran�ois DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////
#ifndef MODPLAY_DEF
#define MODPLAY_DEF

#include <stdint.h>

#ifndef HXCMOD_SLOW_TARGET
	#define HXCMOD_STATE_REPORT_SUPPORT 1
	#define HXCMOD_OUTPUT_FILTER 1
	#define HXCMOD_OUTPUT_STEREO_MIX 1
	#define HXCMOD_CLIPPING_CHECK 1
#endif

// Basic type
typedef uint8_t    muchar;
typedef int8_t     mchar;
typedef uint16_t   muint;
typedef int16_t    mint;
typedef uint32_t   mulong;

#ifdef HXCMOD_16BITS_TARGET
	typedef uint16_t  mssize;
#else
	typedef uint32_t   mssize;
#endif

#ifdef HXCMOD_8BITS_OUTPUT
	#ifdef HXCMOD_UNSIGNED_OUTPUT
	typedef uint8_t  msample;
	#else
	typedef signed char    msample;
	#endif
#else
	#ifdef HXCMOD_UNSIGNED_OUTPUT
	typedef uint16_t msample;
	#else
	typedef int16_t msample;
	#endif
#endif

#ifdef HXCMOD_MAXCHANNELS
	#define NUMMAXCHANNELS HXCMOD_MAXCHANNELS
#else
	#define NUMMAXCHANNELS 32
#endif

#define MAXNOTES 12*12

//
// MOD file structures
//

#pragma pack(1)

typedef struct {
	muchar  name[22];
	muint   length;
	muchar  finetune;
	muchar  volume;
	muint   reppnt;
	muint   replen;
} sample;

typedef struct {
	muchar  sampperiod;
	muchar  period;
	muchar  sampeffect;
	muchar  effect;
} note;

typedef struct {
	muchar  title[20];
	sample  samples[31];
	muchar  length;
	muchar  protracker;
	muchar  patterntable[128];
	muchar  signature[4];
	muchar  speed;
} module;

#pragma pack()

//
// HxCMod Internal structures
//
typedef struct {
	mchar * sampdata;
	muint   length;
	muint   reppnt;
	muint   replen;
	muchar  sampnum;

	mchar * nxt_sampdata;
	muint   nxt_length;
	muint   nxt_reppnt;
	muint   nxt_replen;
	muchar  update_nxt_repeat;

	mchar * dly_sampdata;
	muint   dly_length;
	muint   dly_reppnt;
	muint   dly_replen;
	muchar  note_delay;

	mchar * lst_sampdata;
	muint   lst_length;
	muint   lst_reppnt;
	muint   lst_replen;
	muchar  retrig_cnt;
	muchar  retrig_param;

	muint   funkoffset;
	mint    funkspeed;

	mint    glissando;

	mulong  samppos;
	mulong  sampinc;
	muint   period;
	muchar  volume;
	muchar  effect;
	muchar  parameffect;
	muint   effect_code;

	mint    decalperiod;
	mint    portaspeed;
	mint    portaperiod;
	mint    vibraperiod;
	mint    Arpperiods[3];
	muchar  ArpIndex;

	muchar  volumeslide;

	muchar  vibraparam;
	muchar  vibrapointeur;

	muchar  finetune;

	muchar  cut_param;

	muint   patternloopcnt;
	muint   patternloopstartpoint;
} channel;

typedef struct {
	module  song;
	mchar * sampledata[31];
	note *  patterndata[128];

#ifdef HXCMOD_16BITS_TARGET
	muint   playrate;
#else
	mulong  playrate;
#endif

	muint   tablepos;
	muint   patternpos;
	muint   patterndelay;
	muchar  jump_loop_effect;
	muchar  bpm;

#ifdef HXCMOD_16BITS_TARGET
	muint   patternticks;
	muint   patterntickse;
	muint   patternticksaim;
	muint   patternticksem;
	muint   tick_cnt;
#else
	mulong  patternticks;
	mulong  patterntickse;
	mulong  patternticksaim;
	mulong  patternticksem;
	mulong  tick_cnt;
#endif

	mulong  sampleticksconst;

	channel channels[NUMMAXCHANNELS];

	muint   number_of_channels;

	muint   fullperiod[MAXNOTES * 8];

	muint   mod_loaded;

	mint    last_r_sample;
	mint    last_l_sample;

	mint    stereo;
	mint    stereo_separation;
	mint    bits;
	mint    filter;

	int 	end_of_song;
#ifdef EFFECTS_USAGE_STATE
	int effects_event_counts[32];
#endif

} modcontext;


//
// Player states structures
//
typedef struct track_state_
{
	uint8_t instrument_number;
	uint16_t cur_period;
	uint8_t  cur_volume;
	uint16_t cur_effect;
	uint16_t cur_parameffect;
}track_state;

typedef struct tracker_state_
{
	int16_t number_of_tracks;
	int16_t bpm;
	int16_t speed;
	int16_t cur_pattern;
	int16_t cur_pattern_pos;
	int16_t cur_pattern_table_pos;
	uint32_t buf_index;
	track_state tracks[NUMMAXCHANNELS];
} tracker_state;

typedef struct tracker_state_instrument_
{
	char name[22];
	int16_t active;
}tracker_state_instrument;

typedef struct tracker_buffer_state_
{
	int16_t nb_max_of_state;
	int16_t nb_of_state;
	int16_t cur_rd_index;
	int16_t sample_step;
	char name[64];
	tracker_state_instrument instruments[31];
	tracker_state * track_state_buf;
}tracker_buffer_state;

///////////////////////////////////////////////////////////////////////////////////
// HxCMOD Core API:
// -------------------------------------------
// int  hxcmod_init(modcontext * modctx)
//
// - Initialize the modcontext buffer. Must be called before doing anything else.
//   Return 1 if success. 0 in case of error.
// -------------------------------------------
// int  hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size )
//
// - "Load" a MOD from memory (from "mod_data" with size "mod_data_size").
//   Return 1 if success. 0 in case of error.
// -------------------------------------------
// void hxcmod_fillbuffer( modcontext * modctx, uint16_t * outbuffer, mssize nbsample, tracker_buffer_state * trkbuf )
//
// - Generate and return the next samples chunk to outbuffer.
//   nbsample specify the number of stereo 16bits samples you want.
//   The output format is signed 44100Hz 16-bit Stereo PCM samples.
//   The output buffer size in byte must be equal to ( nbsample * 2 * 2 ).
//   The optional trkbuf parameter can be used to get detailed status of the player. Put NULL/0 is unused.
// -------------------------------------------
// void hxcmod_unload( modcontext * modctx )
//
// - "Unload" / clear the player status.
// -------------------------------------------
///////////////////////////////////////////////////////////////////////////////////

int  hxcmod_init( modcontext * modctx );
int  hxcmod_setcfg( modcontext * modctx, int samplerate, int stereo_separation, int filter );
int  hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size );
void hxcmod_fillbuffer( modcontext * modctx, msample * outbuffer, mssize nbsample, tracker_buffer_state * trkbuf );
void hxcmod_unload( modcontext * modctx );

#endif
