/* ITU-T G.729 Software Package Release 2 (November 2006) */
/* Version 1.2    Last modified: May 1998 */

/* -------------------------------------------------------------------- */
/*   Decod_ld8e and Init_Decod_ld8e.                        */
/*                                                                      */
/*   (C) Original Copyright 1995, AT&T, France Telecom, NTT,            */
/*   Universite de Sherbrooke.                                          */
/*   All rights reserved.                                               */
/*                                                                      */
/*                                                                      */
/*   Modified to perform higher bit-rate extension of G729 at 11.8 kb/s */
/*                      Bit rate : 11.8 kb/s                            */
/*                                                                      */
/*   (C) Copyright : France Telecom, Universite de Sherbrooke.          */
/*   All rights reserved.                                               */
/*                                                                      */
/* -------------------------------------------------------------------- */



/*-----------------------------------------------------------------*
 *   Functions Init_Decod_ld8e  and Decod_ld8e                     *
 *-----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "typedef.h"
#include "basic_op.h"
#include "ld8k.h"
#include "ld8e.h"

/*---------------------------------------------------------------*
 *   Decoder constant parameters (defined in "ld8k.h")           *
 *---------------------------------------------------------------*
 *   L_FRAME     : Frame size.                                   *
 *   L_SUBFR     : Sub-frame size.                               *
 *   M           : LPC order.                                    *
 *   MP1         : LPC order+1                                   *
 *   PIT_MIN     : Minimum pitch lag.                            *
 *   PIT_MAX     : Maximum pitch lag.                            *
 *   L_INTERPOL  : Length of filter for interpolation            *
 *   PRM_SIZE    : Size of vector containing analysis parameters *
 *---------------------------------------------------------------*/

/*--------------------------------------------------------*
 *         Static memory allocation.                      *
 *--------------------------------------------------------*/

 /* Excitation vector */

   static Word16 old_exc[L_FRAME+PIT_MAX+L_INTERPOL];
   static Word16 *exc;

        /* Lsp (Line spectral pairs) */

   static Word16 lsp_old[M]= { 30000, 26000, 21000, 15000, 8000, 0,
                             -8000,-15000,-21000,-26000};

        /* Filter's memory */

   static Word16 mem_syn[M_BWD];
   static Word16 sharp;           /* pitch sharpening of previous frame */
   static Word16 prev_T0;         /* integer delay of previous frame    */
   static Word16 prev_T0_frac;    /* integer delay of previous frame    */
   static Word16 gain_code;       /* Code gain                          */
   static Word16 gain_pitch;      /* Pitch gain                         */

    /* for the backward analysis */
    static Word32  rexp[M_BWDP1];
    static Word16 A_bwd_mem[M_BWDP1];
    static Word16 A_t_bwd_mem[2*M_BWDP1];
    static Word16  prev_voicing, prev_bfi, prev_mode, C_fe_fix, C_int ;
    static Word16    prev_filter[M_BWDP1]; /* Previous selected filter */
    static Word16 prev_pitch;
    static Word16 stat_pitch;
    static Word16 pitch_sta, frac_sta;
    /* Last backward A(z) for case of unstable filter */
    static Word16 old_A_bwd[M_BWDP1];
    static Word16 old_rc_bwd[2];

    static Word16   gain_pit_mem;
    static Word16   gain_cod_mem;
    static Word16   c_muting;
    static int      count_bfi;
    static int      stat_bwd;

    static Word16 freq_prev[MA_NP][M];   /* Q13 */

    /* static memory for frame erase operation */
    static Word16 prev_ma;                  /* previous MA prediction coef.*/
    static Word16 prev_lsp[M];           /* previous LSP vector */


/*-----------------------------------------------------------------*
 *   Function Init_Decod_ld8e                                      *
 *            ~~~~~~~~~~~~~~~                                      *
 *                                                                 *
 *   ->Initialization of variables for the decoder section.        *
 *                                                                 *
 *-----------------------------------------------------------------*/

void Init_Decod_ld8e(void)
{


    /* Initialize static pointer */
    exc = old_exc + PIT_MAX + L_INTERPOL;

    /* Static vectors to zero */
     Set_zero(old_exc, PIT_MAX+L_INTERPOL);
     Set_zero(mem_syn, M_BWD);

    sharp  = SHARPMIN;
    prev_T0 = 60;
    prev_T0_frac = 0;
    gain_code = 0;
    gain_pitch = 0;

    Lsp_decw_resete(freq_prev, prev_lsp, &prev_ma);

    Set_zero(A_bwd_mem, M_BWDP1);
    Set_zero(A_t_bwd_mem, M_BWDP1);
    A_bwd_mem[0] = 4096;
    A_t_bwd_mem[0] = 4096;

    prev_voicing = 0;
    prev_bfi = 0;
    prev_mode = 0;
    C_fe_fix = 0;       /* Q12 */
    C_int = 4506;       /* Filter interpolation parameter */
    Set_zero(prev_filter, M_BWDP1);
    prev_filter[0] = 4096;
    prev_pitch = 30;
    stat_pitch = 0;
    Set_zero(old_A_bwd, M_BWDP1);
    old_A_bwd[0]= 4096;
    Set_zero(old_rc_bwd, 2);
    gain_pit_mem = 0;
    gain_cod_mem = 0;
    c_muting = 32767;
    count_bfi = 0;
    stat_bwd = 0;

    return;
}

/*-----------------------------------------------------------------*
 *   Function Decod_ld8e                                           *
 *           ~~~~~~~~~~                                            *
 *   ->Main decoder routine.                                       *
 *                                                                 *
 *-----------------------------------------------------------------*/

void Decod_ld8e (

    Word16  parm[],      /* (i)   : vector of synthesis parameters
                                  parm[0] = bad frame indicator (bfi)  */
    Word16 rate,            /* input   : rate selector/frame  =0 8kbps,= 1 11.8 kbps*/
    Word16  voicing,     /* (i)   : voicing decision from previous frame */
    Word16  synth_buf[],     /* (i/o)   : synthesis speech                     */
    Word16  Az_dec[],       /* (o) : decoded LP filter in 2 subframes     */
    Word16  *T0_first,    /* (o)   : decoded pitch lag in first subframe  */
    Word16 *bwd_dominant,  /* (o)   : */
    Word16 *m_pst         /* (o)   : LPC order for postfilter */
)

{


    /* Scalars */
    Word16 i, j, i_subfr;
    Word16 T0, T0_frac, index;
    Word16 bfi;
    Word16 mode;                   /* Backward / Forward mode indication */
    Word16 g_p, g_c;               /* fixed and adaptive codebook gain */
    Word16 bad_pitch;              /* bad pitch indicator */
    Word16 tmp, tmp1, tmp2;
    Word16 sat_filter;
    Word32 L_temp;
    Word32 energy;


 /* Tables */
    Word16 A_t_bwd[2*M_BWDP1];   /* LPC Backward filter */
    Word16 A_t_fwd[2*MP1];     /* LPC Forward filter */
    Word16 rc_bwd[M_BWD];      /* LPC backward reflection coefficients */
    Word32 r_bwd[M_BWDP1];   /* Autocorrelations (backward) */
    Word16 r_l_bwd[M_BWDP1];   /* Autocorrelations low (backward) */
    Word16 r_h_bwd[M_BWDP1];   /* Autocorrelations high (backward) */
    Word16 lsp_new[M];         /* LSPs             */
    Word16 code[L_SUBFR];      /* ACELP codevector */
    Word16 *pA_t;                /* Pointer on A_t   */
    Word16 stationnary;
    Word16 m_aq;
    Word16 *synth;
    extern        Flag Overflow;

    /* Test bad frame indicator (bfi) */
    bfi = *parm++;

    /* Decoding the Backward/Forward LPC decision */
     /* ------------------------------------------ */
    if( rate == 0) mode = 0;
    else {
        if (bfi != 0) {
            mode = prev_mode; /* Frame erased => mode = previous mode */
            *parm++ = mode;
        }
        else {
            mode = *parm++;
        }
        if(prev_bfi != 0) voicing = prev_voicing;
    }
    if( bfi == 0) {
        c_muting = 32767;
        count_bfi = 0;
    }
    /* -------------------- */
    /* Backward LP analysis */
    /* -------------------- */
    if (rate == 1) {
        /* LPC recursive Window as in G728 */
        autocorr_hyb_window(synth_buf, r_bwd, rexp); /* Autocorrelations */

        Lag_window_bwd(r_bwd, r_h_bwd, r_l_bwd); /* Lag windowing    */

        /* Fixed Point Levinson (as in G729) */
        Levinsone(M_BWD, r_h_bwd, r_l_bwd, &A_t_bwd[M_BWDP1], rc_bwd,
                    old_A_bwd, old_rc_bwd );

        /* Tests saturation of A_t_bwd */
        sat_filter = 0;
        for (i=M_BWDP1; i<2*M_BWDP1; i++) if (A_t_bwd[i] >= 32767) sat_filter = 1;
        if (sat_filter == 1) Copy(A_t_bwd_mem, &A_t_bwd[M_BWDP1], M_BWDP1);
        else Copy(&A_t_bwd[M_BWDP1], A_t_bwd_mem, M_BWDP1);

        /* Additional bandwidth expansion on backward filter */
        Weight_Az(&A_t_bwd[M_BWDP1], GAMMA_BWD, M_BWD, &A_t_bwd[M_BWDP1]);
    }
    /*--------------------------------------------------*
    * Update synthesis signal for next frame.          *
    *--------------------------------------------------*/
    Copy(&synth_buf[L_FRAME], &synth_buf[0], MEM_SYN_BWD);

    if(mode == 1) {
        if ((C_fe_fix != 0)) {
            /* Interpolation of the backward filter after a bad frame */
            /* A_t_bwd(z) = C_fe . A_bwd_mem(z) + (1 - C_fe) . A_t_bwd(z) */
            /* ---------------------------------------------------------- */
            tmp = sub(4096, C_fe_fix);
            pA_t = A_t_bwd + M_BWDP1;
            for (i=0; i<M_BWDP1; i++) {
                L_temp = L_mult(pA_t[i], tmp);
                L_temp = L_shr(L_temp, 13);
                tmp1 = extract_l(L_temp);
                L_temp = L_mult(A_bwd_mem[i], C_fe_fix);
                L_temp = L_shr(L_temp, 13);
                tmp2 = extract_l(L_temp);
                pA_t[i] = add(tmp1, tmp2);
            }
        }
    }


    /* Memorize the last good backward filter when the frame is erased */
    if ((bfi != 0)&&(prev_bfi == 0))
        for (i=0; i<M_BWDP1; i++) A_bwd_mem[i] = A_t_bwd[i+M_BWDP1];

    /* ---------------------------- */
    /* LPC decoding in forward mode */
    /* ---------------------------- */
    if (mode == 0) {

        /* Decode the LSPs */
        D_lspe(parm, lsp_new, bfi, freq_prev, prev_lsp, &prev_ma);
        parm += 2;
        if( prev_mode == 0) { /* Interpolation of LPC for the 2 subframes */
            Int_qlpc(lsp_old, lsp_new, A_t_fwd);
        }
        else {
            /* no interpolation */
            Lsp_Az(lsp_new, A_t_fwd);           /* Subframe 1*/
            Copy(A_t_fwd, &A_t_fwd[MP1], MP1); /* Subframe 2 */
        }

        /* update the LSFs for the next frame */
        Copy(lsp_new, lsp_old, M);

        C_int = 4506;
        pA_t = A_t_fwd;
        m_aq = M;
        /* update the previous filter for the next frame */
        Copy(&A_t_fwd[MP1], prev_filter, MP1);
        for(i=MP1; i<M_BWDP1; i++) prev_filter[i] = 0;
    }
    else {
        Int_bwd(A_t_bwd, prev_filter, &C_int);
        pA_t = A_t_bwd;
        m_aq = M_BWD;
        /* update the previous filter for the next frame */
        Copy(&A_t_bwd[M_BWDP1], prev_filter, M_BWDP1);
     }

    /*------------------------------------------------------------------------*
    *          Loop for every subframe in the analysis frame                 *
    *------------------------------------------------------------------------*
    * The subframe size is L_SUBFR and the loop is repeated L_FRAME/L_SUBFR  *
    *  times                                                                 *
    *     - decode the pitch delay                                           *
    *     - decode algebraic code                                            *
    *     - decode pitch and codebook gains                                  *
    *     - find the excitation and compute synthesis speech                 *
    *------------------------------------------------------------------------*/

    synth = synth_buf + MEM_SYN_BWD;
    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR) {

        index = *parm++;            /* pitch index */

        if(i_subfr == 0) {

            i = *parm++;             /* get parity check result */
            bad_pitch = add(bfi, i);
            if( bad_pitch == 0) {
                Dec_lag3(index, PIT_MIN, PIT_MAX, i_subfr, &T0, &T0_frac);
                prev_T0 = T0;
                prev_T0_frac = T0_frac;
            }
            else {                     /* Bad frame, or parity error */
                if (bfi == 0) printf(" ! Wrong Pitch 1st subfr. !   ");
                T0  =  prev_T0;
                if (rate == 1) {
                    T0_frac = prev_T0_frac;
                }
                else {
                    T0_frac = 0;
                    prev_T0 = add( prev_T0, 1);
                    if( sub(prev_T0, PIT_MAX) > 0) {
                        prev_T0 = PIT_MAX;
                    }
                }
            }
            *T0_first = T0;         /* If first frame */
        }
        else {                       /* second subframe */

            if( bfi == 0) {
                Dec_lag3(index, PIT_MIN, PIT_MAX, i_subfr, &T0, &T0_frac);
                prev_T0 = T0;
                prev_T0_frac = T0_frac;
            }
            else {
                T0  =  prev_T0;
                if (rate == 1) {
                    T0_frac = prev_T0_frac;
                }
                else {
                    T0_frac = 0;
                    prev_T0 = add( prev_T0, 1);
                    if( sub(prev_T0, PIT_MAX) > 0) prev_T0 = PIT_MAX;
                }
            }
        }
       /*-------------------------------------------------*
        * - Find the adaptive codebook vector.            *
        *-------------------------------------------------*/
        Pred_lt_3(&exc[i_subfr], T0, T0_frac, L_SUBFR);

        /* --------------------------------- */
        /* pitch tracking for frame erasures */
        /* --------------------------------- */
        if( rate == 1) {
            track_pit(&prev_T0, &prev_T0_frac, &prev_pitch, &stat_pitch,
                        &pitch_sta, &frac_sta);
        }
        else {
            i = prev_T0;
            j = prev_T0_frac;
            track_pit(&i, &j, &prev_pitch, &stat_pitch,
                        &pitch_sta, &frac_sta);
        }

       /*-------------------------------------------------------*
        * - Decode innovative codebook.                         *
        *-------------------------------------------------------*/
        if(bfi != 0) {        /* Bad frame */

            parm[0] = Random();

            if (rate == 0) parm[1] = Random();
            else {
                parm[1] = Random();
                parm[2] = Random();
                parm[3] = Random();
                parm[4] = Random();
            }
        }

        if (rate == 0) {
            /* case 8 kbps */
            Decod_ACELP(parm[1], parm[0], code);
            parm += 2;
            /* for gain decoding in case of frame erasure */
            stat_bwd = 0;
            stationnary = 0;
        }
        else {
            /* case 11.8 kbps */
            if (mode == 0) {
                Dec_ACELP_10i40_35bits(parm, code);
                /* for gain decoding in case of frame erasure */
                stat_bwd = 0;
                stationnary = 0;
            }
            else {
                Dec_ACELP_12i40_44bits(parm, code);
                /* for gain decoding in case of frame erasure */
                stat_bwd += 1;
                if (stat_bwd >= 30) {
                    stationnary = 1;
                    stat_bwd = 30;
                }
                else stationnary = 0;
            }
            parm += 5;
        }


       /*-------------------------------------------------------*
        * - Add the fixed-gain pitch contribution to code[].    *
        *-------------------------------------------------------*/
        j = shl(sharp, 1);          /* From Q14 to Q15 */
        if(sub(T0, L_SUBFR) <0 ) {
            for (i = T0; i < L_SUBFR; i++) {
                code[i] = add(code[i], mult(code[i-T0], j));
            }
        }

       /*-------------------------------------------------*
        * - Decode pitch and codebook gains.              *
        *-------------------------------------------------*/
        index = *parm++;      /* index of energy VQ */

        Dec_gaine(index, code, L_SUBFR, bfi, &gain_pitch, &gain_code, rate,
           gain_pit_mem, gain_cod_mem, &c_muting, count_bfi, stationnary);
       /*-------------------------------------------------------------*
        * - Update previous gains
        *-------------------------------------------------------------*/
        gain_pit_mem = gain_pitch;
        gain_cod_mem = gain_code;

       /*-------------------------------------------------------------*
        * - Update pitch sharpening "sharp" with quantized gain_pitch *
        *-------------------------------------------------------------*/
        sharp = gain_pitch;
        if (sub(sharp, SHARPMAX) > 0) sharp = SHARPMAX;
        if (sub(sharp, SHARPMIN) < 0) sharp = SHARPMIN;

       /*-------------------------------------------------------*
        * - Find the total excitation.                          *
        * - Find synthesis speech corresponding to exc[].       *
        *-------------------------------------------------------*/
        if(bfi != 0) {       /* Bad frame */
            count_bfi += 1;
            if (voicing == 0 ) {
                g_p = 0;
                g_c = gain_code;
            }
            else {
                g_p = gain_pitch;
                g_c = 0;
            }
        }
        else {
            g_p = gain_pitch;
            g_c = gain_code;
        }
        for (i = 0; i < L_SUBFR;  i++) {
            /* exc[i] = g_p*exc[i] + g_c*code[i]; */
            /* exc[i]  in Q0   g_p in Q14         */
            /* code[i] in Q13  g_code in Q1       */

            L_temp = L_mult(exc[i+i_subfr], g_p);
            L_temp = L_mac(L_temp, code[i], g_c);
            L_temp = L_shl(L_temp, 1);
            exc[i+i_subfr] = round(L_temp);
        }

        Overflow = 0;
        Syn_filte(m_aq, pA_t, &exc[i_subfr], &synth[i_subfr], L_SUBFR,
                                                &mem_syn[M_BWD-m_aq], 0);
        if(Overflow != 0) {
            /* In case of overflow in the synthesis          */
            /* -> Scale down vector exc[] and redo synthesis */
            for(i=0; i<PIT_MAX+L_INTERPOL+L_FRAME; i++) old_exc[i] = shr(old_exc[i], 2);
            Syn_filte(m_aq, pA_t, &exc[i_subfr], &synth[i_subfr], L_SUBFR,
                    &mem_syn[M_BWD-m_aq], 0);
        }
        pA_t += m_aq+1;    /* interpolated LPC parameters for next subframe */
        Copy(&synth[i_subfr+L_SUBFR-M_BWD], mem_syn, M_BWD);

    }

    energy = ener_dB(synth, L_FRAME);
    if (energy >= 8192) tst_bwd_dominant(bwd_dominant, mode);

    /*--------------------------------------------------*
    * Update signal for next frame.                    *
    * -> shift to the left by L_FRAME  exc[]           *
    *--------------------------------------------------*/
    Copy(&old_exc[L_FRAME], &old_exc[0], PIT_MAX+L_INTERPOL);

    if( mode == 0) {
        Copy(A_t_fwd, Az_dec, 2*MP1);
        *m_pst = M;
    }
    else {
        Copy(A_t_bwd, Az_dec, 2*M_BWDP1);
        *m_pst = M_BWD;
    }


    prev_bfi = bfi;
    prev_mode = mode;
    prev_voicing = voicing;

    if (bfi != 0) C_fe_fix = 4096;
    else {
        if (mode == 0) C_fe_fix = 0;
        else {
            if (*bwd_dominant == 1) C_fe_fix = sub(C_fe_fix, 410);
            else C_fe_fix = sub(C_fe_fix, 2048);
            if (C_fe_fix < 0)  C_fe_fix= 0;
        }
    }
    return;

}

