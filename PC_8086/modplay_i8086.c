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
// File : modpay_i8086.c
// Contains: HxCMod PC 8086 player (Real-mode) test program
//
// Written by: Jean François DEL NERO
///////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <bios.h>
#include <dos.h>
#include <conio.h>
#include <i86.h>
#include <stddef.h>

#include "sb_io.h"

#include "../hxcmod.h"


#define DMA_PAGESIZE 4096
#define SB_SAMPLE_RATE 22050
#ifdef HXCMOD_MONO_OUTPUT
	#define NB_CHANNELS 1
#else
	#define NB_CHANNELS 2
#endif

//
// DMA Programming table
//
unsigned char DMAMask[]     = {0x0A,0x0A,0x0A,0x0A,0xD4,0xD4,0xD4,0xD4};
unsigned char DMAMode[]     = {0x0B,0x0B,0x0B,0x0B,0xD6,0xD6,0xD6,0xD6};
unsigned char DMAFlipFlop[] = {0x0C,0x0C,0x0C,0x0C,0xD8,0xD8,0xD8,0xD8};
unsigned char DMACount[]    = {0x01,0x03,0x05,0x07,0xC2,0xC6,0xCA,0xCE};
unsigned char DMAAddress[]  = {0x00,0x02,0x04,0x06,0xC0,0xC4,0xC8,0xCC};
unsigned char DMAPage[]     = {0x87,0x83,0x81,0x82,0x8F,0x8B,0x89,0x8A};

volatile unsigned char far * dma_buffer;
volatile unsigned char far * fixed_dma_buffer;


int reset_sb(const int port)
{
	int cnt; 

	outp(port + SB_DSP_RESET_REG,0x01); 	/* shall wait for 3µs before writing 0 */
	inp(port + SB_DSP_RESET_REG);
	inp(port + SB_DSP_RESET_REG);
	inp(port + SB_DSP_RESET_REG);
	inp(port + SB_DSP_RESET_REG);
	outp(port + SB_DSP_RESET_REG,0x00);
	
	cnt = 256;
	while( !(inp(port + SB_DSP_READ_BUF_IT_STATUS) & 0x80) && cnt != 0)	{
		cnt--;
	}

	return cnt;
}


int get_sb_config(int* port,int* irq, int* dma)
{
	char * blaster;
	int i;

	// parsing the BLASTER configuration
	blaster = getenv("BLASTER");
	i = 0;
	while(blaster[i] != 'A' && blaster[i] != 0) {++i;}
	*port = strtol(&blaster[i+1], NULL, 16);
	i = 0;
	while(blaster[i] != 'I' && blaster[i] != 0) {++i;}
	*irq = strtol(&blaster[i+1], NULL, 10);
	i = 0;
	while(blaster[i] != 'D' && blaster[i] != 0) {++i;}
	*dma = strtol(&blaster[i+1], NULL, 10);
	// sanitize the BLASTER configuration parsed
	if(*port < 0x210 ||  *port > 0x280) {
		return 1;
	}
	if(*irq != 2 && *irq != 5 && *irq != 7 && *irq != 10) {
		return 1;
	}
	if(	*dma != 0 && *dma != 1 && *dma != 3 &&
		*dma != 5 && *dma != 6 && *dma != 7) {
		return 1;
	}

	return 0;
}

int init_sb(int port,int irq,int dma)
{
	uint16_t temp, segment, offset;
	uint32_t foo;
	uint8_t dma_page;
	uint16_t dma_offset;
	uint16_t dsp_ver_major;
	uint16_t dsp_ver_minor;

	outp(0x21 + (irq & 8), inp(0x21 + (irq & 8) ) |  (0x01 << (irq&7)) ); // Mask the IRQ
	install_irq();
	outp(0x21 + (irq & 8), inp(0x21 + (irq & 8) ) & ~(0x01 << (irq&7)) ); // Enable the IRQ	

	if( reset_sb(port) != 0 && inp(port + SB_DSP_READ_REG) == 0xAA )
	{
		/* SB reset success ! */
		SB_DSP_wr(port,DSP_CMD_VERSION);			
		dsp_ver_major = SB_DSP_rd(port);
		dsp_ver_minor = SB_DSP_rd(port);
		printf("SB DSP Version %d.%.2d\n", dsp_ver_major, dsp_ver_minor);

		//////////////////////////////////////////////////////////////////////
		// Init the 8237A DMA

		outp(DMAMask[dma], (dma & 0x03) | 0x04);  // Disable channel
		outp(DMAFlipFlop[dma], 0x00);             // Clear the dma flip flop
		outp(DMAMode[dma], (dma & 0x03) | 0x58 ); // Select the transfert mode (Auto-initialized playback)
		outp(DMACount[dma], (DMA_PAGESIZE - 1) & 0xFF );
		outp(DMACount[dma],((DMA_PAGESIZE - 1)>> 8) & 0xFF );

		// Segment/Offset to DMA 20 bits Page/offset physical address
		segment = (uint16_t)((uint32_t)(dma_buffer) >> 16);
		offset  = (uint16_t)dma_buffer;
		fixed_dma_buffer = dma_buffer;

		dma_page = ((segment & 0xF000) >> 12);
		temp = (segment & 0x0FFF) << 4;
		foo = (unsigned long)offset + (unsigned long)temp;
		if (foo > 0xFFFF)
		{
			dma_page++;
		}

		dma_offset = (unsigned int)(foo & 0xFFFF);

		printf("Buffer : 0x%.4X:0x%.4X\nDMA Page : 0x%.2x\nDMA offset : 0x%.4X\nDMA size : 0x%.4X\n", segment,offset,dma_page,dma_offset,DMA_PAGESIZE);

		if(dma_offset > (0xFFFF - DMA_PAGESIZE))
		{
			printf("Crossing dma page !!!\nFixing buffer position to the next dma page.\n");
			dma_page++;
			fixed_dma_buffer += ((0xFFFF - dma_offset) + 1);
			dma_offset = 0;
		}

		// Set the dma page/offset.
		outp(DMAAddress[dma],  dma_offset & 0xFF );
		outp(DMAAddress[dma], (dma_offset >> 8) & 0xFF );
		outp(DMAPage[dma], dma_page );

		outp(DMAMask[dma], (dma & 0x03)); // Enable the channel

		//////////////////////////////////////////////////////////////////////

		#ifdef SB16
			SB_DSP_wr(port,	DSP_CMD_OUTPUT_RATE);
			SB_DSP_wr(port,	(unsigned char)(SB_SAMPLE_RATE >> 8));
			SB_DSP_wr(port,	(unsigned char)(SB_SAMPLE_RATE & 0xFF));
			SB_DSP_wr(port,	DSP_CMD_OUT_8BIT);
		#ifdef HXCMOD_MONO_OUTPUT
				SB_DSP_wr(port,	DSP_CMD_OUT_MONO);
		#else	
				SB_DSP_wr(port,	DSP_CMD_OUT_STEREO);
		#endif
			SB_DSP_wr(port, (( (DMA_PAGESIZE / 2) - 1 ) ) & 0xFF ); // 2 ITs for the whole DMA buffer.
			SB_DSP_wr(port, (( (DMA_PAGESIZE / 2) - 1 ) ) >> 8 );
		#else
			#define SAMPLE_PERIOD (unsigned char)((65536 - (256000000/(SB_SAMPLE_RATE)))>>8)
			SB_DSP_wr(port, DSP_CMD_TIME_CSTE);     // Set period
			SB_DSP_wr(port, SAMPLE_PERIOD);
			SB_DSP_wr(port,DSP_CMD_BLOCK_TRANSFER_SIZE); // Set block transfer size			
			SB_DSP_wr(port, (( (DMA_PAGESIZE / 2) - 1 ) ) & 0xFF ); // 2 ITs for the whole DMA buffer.
			SB_DSP_wr(port, (( (DMA_PAGESIZE / 2) - 1 ) ) >> 8 );
			SB_DSP_wr(port, DSP_CMD_DMA8_STARTAUTOMODE);  // Start ! (Mono 8 bits unsigned mode)
		#endif

		SB_DSP_wr(port,DSP_CMD_ENABLE_SPEAKER);  // Enable "speaker"


		printf("SB Init done !\n");

		return 1;
	}
	else {
		printf("Could not reset the Sound Blaster.\n");
	}

	return 0;
}

void stop_sb(int port, int dma)
{
	outp(DMAMask[dma], (dma & 0x03) | 0x04); // Stop the 8237A DMA		
	outp(port + SB_DSP_WRITE_DATCMD_REG, DSP_CMD_DMA8_STOPAUTOMODE); // stops 8-bit "auto-initialize DMA"
	uninstall_irq();
}

int main(int argc, char* argv[])
{
	FILE* mod;
	int size_mod, size_read;
	unsigned char * rawModData;
	int sb_port,sb_irq_int,sb_dma;
	int i;
	modcontext * modctx;
	tracker_buffer_state* read_state;
	unsigned char last_toggle;
	clock_t time_start, time_stop, time_process_beg, time_process_acc;
	int cpu_usage, nb_lost_frames;
	int last_pattern;

	if(argc != 2) {
		printf("Error: missing argument.\nUsage: hxcmod [FILE]\n");
		exit(-1);
	} 

	// open the file
	mod = fopen(argv[1], "rb");
	if(mod == NULL) {
		printf("Error: %s is not a file", argv[1]);
		exit(-1);
	}
	if(fseek(mod, 0, SEEK_END) != 0) {
		printf("Error: %s is bad.", argv[1]);
		exit(-1);
	} 
	size_mod = ftell(mod);
	rawModData = malloc( size_mod );
	if(rawModData == NULL) {
		printf("Error: could not allocate enough memory for the module.");
		exit(-1);
	}
	fseek(mod, 0, SEEK_SET);
	size_read = fread(rawModData, 1, size_mod, mod);
	if( size_read != size_mod) {
		printf("Error: cannot properly read %s %d\n", argv[1], size_read);
		exit(-1);
	}

	printf("PC-8086 Real mode HxCMod Test program\n");

	if( get_sb_config(&sb_port, &sb_irq_int, &sb_dma) != 0) {
		printf("Error: Could not read the Sound Blaster configuration.\nCheck the BLASTER environment variable.\n");
		exit(-1);
	}
	it_sbport = sb_port;
	it_irq = sb_irq_int;

	printf("Init Sound Blaster : Port 0x%x, IRQ %d, DMA: %d\n",sb_port,sb_irq_int,sb_dma);

	dma_buffer = malloc(DMA_PAGESIZE*2);
	printf("dma_buffer : %Fp\n", dma_buffer);
	if(!dma_buffer)
	{
		printf("Error: DMA memory allocation failed !\n");
		exit(-1);
	}

	for(i=0;i<DMA_PAGESIZE*2;i++)
	{
		dma_buffer[i] = 0x00;
	}

	if(!init_sb(sb_port,sb_irq_int,sb_dma))
	{
		printf("Sound Blaster init failed !\n");
		exit(-1);
	}

	modctx = malloc(sizeof(modcontext));
	read_state = malloc(sizeof(tracker_buffer_state));

	if(modctx && read_state)
	{
		if( hxcmod_init( modctx ) )
		{
			printf("HxCMOD init done !\n");

			hxcmod_setcfg( modctx, SB_SAMPLE_RATE, 0, 0);

			printf("Sound configuration done !\n");

			last_toggle = 0;
			time_stop = 0;

			if(hxcmod_load( modctx, rawModData, size_mod))
			{
				printf("Playing \"%s\"\nLength: %d patterns\n", modctx->song.title, modctx->song.length);
				
				nb_lost_frames = 0;
				time_process_acc = 0;
				time_start = clock();

				// ==== MAIN LOOP ====
				last_pattern = -1;
				while(kbhit() == 0 && modctx->end_of_song == 0)
				{
					if(it_flag)
					{
						it_flag = 0x00;

						if( modctx->tablepos != last_pattern) { // on pattern change
							printf("Position: %d\r", modctx->tablepos);
							fflush(stdout);
							last_pattern = modctx->tablepos;
						}

						if( last_toggle == it_toggle )
						{
							printf("lost frame ?\n");
							++nb_lost_frames;
						}

						last_toggle = it_toggle;

						time_process_beg = clock();
						if(it_toggle) {							
							hxcmod_fillbuffer( modctx, (msample *)&fixed_dma_buffer[0], DMA_PAGESIZE/(2*NB_CHANNELS), read_state );
						}
						else {
							hxcmod_fillbuffer( modctx, (msample *)&fixed_dma_buffer[DMA_PAGESIZE/2], DMA_PAGESIZE/(2*NB_CHANNELS), read_state );
						}
						time_process_acc += clock() - time_process_beg;					
					}
					
				}
				// ==== MAIN LOOP ====
				
				time_stop = clock();
				if(kbhit() != 0) {
					getch(); // clear key
				}
			}
			else {
				printf("Error: could not load or decode module's data.\n");
			}
		}				

		// Stop SB
		stop_sb(sb_port, sb_dma);

		// Free resources
		hxcmod_unload( modctx );
		free(modctx);

		// Display bench
		if(time_stop != 0) {
			cpu_usage = 1000 * time_process_acc / (time_stop - time_start);
			printf("Nb frames lost: %d\nCPU usage: %d.%d%%\n", nb_lost_frames, cpu_usage / 10, cpu_usage % 10);
		}

	}
	else
	{
		printf("Malloc failed !\n");
	}


	return 0;
}
