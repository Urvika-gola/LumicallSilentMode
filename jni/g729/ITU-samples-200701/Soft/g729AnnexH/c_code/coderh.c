/* ITU-T G.729 Software Package Release 2 (November 2006) */
/*
   ITU-T G.729 Annex H  - Reference C code for fixed point
                         implementation of G.729 Annex H
                         (integration of Annexes D and E)
   Version 1.2    Last modified: October 2006 
*/
/*
----------------------------------------------------------------------
                    COPYRIGHT NOTICE
----------------------------------------------------------------------
   ITU-T G.729 Annex  H fixed point ANSI C source code
   Copyright (C) 1999, AT&T, France Telecom, NTT, University of
   Sherbrooke, Conexant, Ericsson. All rights reserved.
----------------------------------------------------------------------
*/

/*
 File : CODERH.C
 */
/* from codere.c G.729 Annex E Version 1.2  Last modified: May 1998 */
/* from coderd.c G.729 Annex D Version 1.2  Last modified: May 1998 */
/* from coder.c G.729 Version 3.3  */

/*--------------------------------------------------------------------------------------*
 * Main program of the ITU-T G.729H   11.8/8/6.4 kbit/s encoder.
 *
 *    Usage : coderh speech_file  bitstream_file  [bit_rate or file_bit_rate]
 *--------------------------------------------------------------------------------------*/

/* ------------------------------------------------------------------------ */
/*                            MAIN PROGRAM                                  */
/* ------------------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>

#include "typedef.h"
#include "basic_op.h"
#include "ld8k.h"
#include "ld8h.h"
#include "ld8e.h"
#if defined(__BORLANDC__)
extern unsigned _stklen = 48000U;
#endif

/*#define SYNC*/
#define PREPROC

int main(int argc, char *argv[] )
{
    FILE *f_speech;               /* File of speech data                   */
    FILE *f_serial;               /* File of serial bits for transmission  */
    FILE  *f_rate;
    Word16 rate, flag_rate;

    extern Word16 *new_speech;     /* Pointer to new speech data            */
    
    Word16 prm[PRM_SIZE_E+1];          /* Analysis parameters.                  */
    Word16 serial[SERIAL_SIZE_E];    /* Output bitstream buffer               */
    
    Word16 i;
    Word32 count_frame;     /* frame counter */

    Word16 nb_words;

    for(i = 0; i < argc; i++){
        printf("argument %d : %s\n", i, argv[i]);
    }
    printf("\n");
    printf("*************************************************************************\n");
    printf("****    ITU G.729 ANNEX H: 6.4, 8.0, and 11.8 KBIT/S SPEECH CODER   ****\n");
    printf("****         WITHOUT VAD/DTX/CNG (ANNEX B)                ****\n");
    printf("*************************************************************************\n");
    printf("\n");
    printf("------------------ Fixed point C simulation ----------------\n");
    printf("\n");
    printf("----------- Version 1.2 (Release 2, November 2006) ---------\n");
    printf("\n");
    printf("                 Bit rates : 6.4, 8.0, or 11.8 kb/s \n");
    printf("\n");

    /*--------------------------------------------------------------------------*
    * Open speech file and result file (output serial bit stream)              *
    *--------------------------------------------------------------------------*/
    if (( argc != 3 ) && (argc != 4) ){
        printf("Usage : coderh speech_file bitstream_file [bitrate or file_bit_rate]\n");
        printf("Format for speech_file:\n");
        printf("  Speech is read from a binary file of 16 bits PCM data.\n");
        printf("\n");
        printf("Format for bitstream_file:\n");
        printf("  One (2-byte) synchronization word \n");
        printf("  One (2-byte) bit-rate word \n");
        printf("\n");
        printf("bitrate = 0 (6.4 kb/s), 1 (8 kb/s)  or 2 (11.8 kb/s)  (default : 8 kb/s)\n");
        printf("Format for bitrate_file:\n");
        printf("  1 16bit-Word per frame , =0 bit rate 6.4 kb/s, =1 bit rate 8 kb/s, or =2 bit rate 11.8 kb/s \n");
        printf("Forward / Backward structure at 11.8 kb/s \n");
        printf("\n");
        exit(1);
    }
    
    if ( (f_speech = fopen(argv[1], "rb")) == NULL) {
        printf("%s - Error opening file  %s !!\n", argv[0], argv[1]);
        exit(0);
    }
    printf(" Input speech file     :  %s\n", argv[1]);
    
    if ( (f_serial = fopen(argv[2], "wb")) == NULL) {
        printf("%s - Error opening file  %s !!\n", argv[0], argv[2]);
        exit(0);
    }
    printf(" Output bitstream file :  %s\n", argv[2]);

    f_rate = NULL; /* to avoid  visual warning */
    rate = G729;  /* to avoid  visual warning */

    if(argc != 3) {
        if ( (f_rate = fopen(argv[3], "rb")) == NULL) {
            rate  = (Word16)atoi(argv[3]);
            if( rate == 2) printf(" Selected Bitrate      :  11.8 kb/s\n");
            else {
                if( rate == 1) printf(" Selected Bitrate      :  8.0 kb/s\n");
                else {
                    if( rate == 0) printf(" Selected Bitrate      :  6.4 kb/s\n");
                    else {
                        printf(" error bit rate indication\n");
                        printf(" argv[3] = 0 for bit rate 6.4 kb/s \n");
                        printf(" argv[3] = 1 for bit rate 8 kb/s \n");
                        printf(" argv[3] = 2 for bit rate 11.8 kb/s\n");
                        exit(-1);
                    }
                }
            }
            flag_rate = 0;
        }
        else {
            printf(" Selected Bitrate  read in file :  %s kb/s\n", argv[3]);
            flag_rate = 1;
        }
    }
    else {
        flag_rate = 0;
        rate = G729;
        printf(" Selected Bitrate      :  8 kb/s\n");
    }
    
    /*--------------------------------------------------------------------------*
    * Initialization of the coder.                                             *
    *--------------------------------------------------------------------------*/
    
    Init_Pre_Process();
    Init_Coder_ld8h();

    for(i=0; i<PRM_SIZE_E; i++) prm[i] = (Word16)0;


    /* To force the input and output to be time-aligned the variable SYNC
    has to be defined. Note: the test vectors were generated with this option
    disabled
    */

#ifdef SYNC
    /* Read L_NEXT first speech data */
    fread(&new_speech[-L_NEXT], sizeof(Word16), L_NEXT, f_speech);
#ifdef HARDW
    /* set 3 LSB's to zero */
    for(i=0; i < L_NEXT; i++)
        new_speech[-L_NEXT+i] = new_speech[-L_NEXT+i] & 0xFFF8;
#endif
#ifdef PREPROC
    Pre_Process(&new_speech[-L_NEXT], L_NEXT);
#endif
#endif

    /* Loop for each "L_FRAME" speech data. */
    count_frame = 0L;
    
    while( fread(new_speech, sizeof(Word16), L_FRAME, f_speech) == L_FRAME) {
        if( flag_rate == 1) {
            if( fread(&rate, sizeof(Word16), 1, f_rate) != 1) {
                printf("error reading bit_rate in file %s for frame %ld\n",
                    argv[3], count_frame);
                exit(-1);
            }
            if( (rate < 0) || (rate > 2) ) {
                printf("error bit_rate in file %s for frame %ld bit rate non avalaible\n", argv[3], count_frame);
                exit(-1);
            }
        }
#ifdef HARDW
        /* set 3 LSB's to zero */
        for(i=0; i < L_FRAME; i++) new_speech[i] = new_speech[i] & 0xFFF8;
#endif
        
#ifdef PREPROC
        Pre_Process(new_speech, L_FRAME);
#else
        /* Division par 2 si pas de preprocessing */
        for(i=0; i < L_FRAME; i++) new_speech[i] = shr(new_speech[i], 1);
#endif
        
        count_frame++;
        printf(" Frame: %ld\r", count_frame);
        Coder_ld8h(prm, rate);

        prm2bits_ld8h( prm, serial);
        
        nb_words = (Word16)serial[1] +  (Word16)2;
        
        if (fwrite(serial, sizeof(Word16), nb_words, f_serial) != (size_t)nb_words)
            printf("Write Error for frame %ld\n", count_frame);
        
    }
    printf("\n");
    printf("%ld frames processed\n", count_frame);
    
    if(f_serial) fclose(f_serial);
    if(f_speech) fclose(f_speech);
    if(f_rate) fclose(f_rate);

    return(0);
    
} /* end of main() */

