// *****************************************************************************
// Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
// %% version
//
// $Change$  $DateTime$
// *****************************************************************************

#include "cvc_send_data.h"

#include "patch_library.h"

// *****************************************************************************
// MODULE:
//    $cvc.init.root
//
// DESCRIPTION:
//    Reset CVC data root object
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - number of microphones
//    - r8 - use case
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - none
//
// TRASHED REGISTERS:
//
// CPU USAGE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.root;

   .CODESEGMENT PM;

$cvc.init.root:
   // microphone mode:  1 - 1mic
   //                   0 - multimic
   Null = r7 - 1;
   if NZ r7 = 0;
   M[r9 + $cvc_send.data.mic_mode] = r7;

   // CVC use case:     HEADSET/SPEAKER/AUTO
   M[r9 + $cvc_send.data.use] = r8;

   // param
   r6 = M[r9 + $cvc_send.data.param];

   LIBS_SLOW_SW_ROM_PATCH_POINT($cvc.init.root.PATCH_ID_0, r1)

   // HFK/DMSS CONFIG
   r0 = M[r6 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_HFK_CONFIG];
   r1 = M[r6 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DMSS_CONFIG];
   M[r9 + $cvc_send.data.hfk_config] = r0;
   M[r9 + $cvc_send.data.dmss_config] = r1;

   // PARAM CE
   M[r9 + $cvc_send.data.end_fire] = 0;
   Null = r8 - $cvc_send.AUTO;
   if Z jump end_CE_fixed_param;
   Null = M[r9 + $cvc_send.data.mic_mode];
   if NZ jump end_CE_fixed_param;
      // DMP_MODE = 0
      M[r6 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DMP_MODE] = 0;
      // DOA1 = DOA0
      r0 = M[r6 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DOA0];
      M[r6 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DOA1] = r0;
      // end_fire flag = (use~=AUTO) && (num_mic~=1) && (DOA0==90)
      r1 = 1;
      Null = r0 - 90;
      if NZ r1 = 0;
      M[r9 + $cvc_send.data.end_fire] = r1;
   end_CE_fixed_param:

   // power adjust: used in ASF/DMS
   r0 = M[r6 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DMSS_LPN_MIC];
   r1 = M[r9 + $cvc_send.data.fftwin_power];
   r0 = r0 + r1;
   M[r9 + $cvc_send.data.power_adjust] = r0;

   // reset wind_flag
   M[r9 + $cvc_send.data.wind_flag] = 0;

   // reset echo_flag
   M[r9 + $cvc_send.data.echo_flag] = 0;

   // reset vad_flag
   M[r9 + $cvc_send.data.vad_flag] = 0;

   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.stream_purge
//
// DESCRIPTION:
//    Purge Streams
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - streams
//
// OUTPUTS:
//    - none
//
// TRASHED REGISTERS:
//
// CPU USAGE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.stream_purge;

   .CODESEGMENT PM;

$cvc.init.stream_purge:
   push rLink;

   call $block_interrupts;

   next_stream:
      r1 = M[r7];
      r0 = M[r1 + $frmbuffer.CBUFFER_PTR_FIELD];
      r1 = M[r0 + $cbuffer.WRITE_ADDR_FIELD];
      M[r0 + $cbuffer.READ_ADDR_FIELD] = r1;

   r7 = r7 + MK1;
   Null = M[r7];
   if NZ jump next_stream;

   call $unblock_interrupts;

   jump $pop_rLink_and_rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.fb.stream_connect
//
// DESCRIPTION:
//    Connect a stream to a filter_bank analysis object
//
//       if Mic_switch
//          fba_left/fba_right stream switch
//       end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - cvc_streams
//    - r8 - fba
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - none
//
// TRASHED REGISTERS:
//
// CPU USAGE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.filter_bank.stream_connect;

   .CODESEGMENT PM;

$cvc.init.fb.stream_connect.left:
   r0 = M[r7 + $cvc_send.stream.sndin_left];
   r7 = M[r7 + $cvc_send.stream.sndin_right];
   jump mic_switch;

$cvc.init.fb.stream_connect.right:
   r0 = M[r7 + $cvc_send.stream.sndin_right];
   r7 = M[r7 + $cvc_send.stream.sndin_left];

   mic_switch:
   // Mic_switch?
   r1 = M[r9 + $cvc_send.data.param];
   Null = M[r1 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_MIC_SWITCH];
   if Z r7 = r0;

   // connect stream
   M[r8 + $M.filter_bank.Parameters.OFFSET_PTR_FRAME] = r7;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.harm
//
// DESCRIPTION:
//    Reset harm bypass flag
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (snd_harm_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.harmonicity;

   .CODESEGMENT PM;

$cvc.init.harm:
   r0 = 1;
   M[r8 + $harm100.FLAG_BYPASS_FIELD] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.harm_export
//
// DESCRIPTION:
//    Connect harmonicity value pointer to a given user
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (oms_in_obj/dms200_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.harmonicity_export;

   .CODESEGMENT PM;

$cvc.init.harm_export:
   // Export harm value
   r1 = M[r9 + $cvc_send.data.harm_obj];
   r0 = r1 + $harm100.HARM_VALUE_FIELD;
   M[r8 + $M.oms280.PTR_HARM_VALUE_FIELD] = r0;

   // Harmonicity is active
   M[r1 + $harm100.FLAG_BYPASS_FIELD] = 0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.oms_in
//
// DESCRIPTION:
//    if HandsFree_on
//       Connect Harm to oms_in_obj
//    end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (oms_in_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.oms_in;

   .CODESEGMENT PM;

$cvc.init.oms_in:
   // HandsFree_on?
   r1 = M[r9 + $cvc_send.data.use];
   Null = r1 - $cvc_send.HEADSET;
   if NZ jump $cvc.init.harm_export;

   // Now HandsFree_on is 0, harmonicity is not used
   M[r8 + $M.oms280.PTR_HARM_VALUE_FIELD] = 0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.oms_in
//
// DESCRIPTION:
//    OMSin_on = AEC_on || NDVC_on || MGDC_on
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (oms_in_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~OMSin_on (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.oms_in;

   .CODESEGMENT PM;

$cvc.mc.oms_in:
   // MGDC_on?
   r1 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_MGDC;
   if Z rts;

   // NDVC_on?
   r1 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_NDVC;
   if Z rts;

   // AEC_on?
   r0 = M[r9 + $cvc_send.data.aec_inactive];
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.dmss_rnr
//
// DESCRIPTION:
//    CVC_CE:
//       RNR_G_FLAG = 0;
//       RNR_ON = (APP_b.Mode == 0 && ~Wind_Flag && RNR_on)
//       if RNR_ON
//          if (DMSout_b.SNR_mn < 2 && TP_mode <3)
//             RNR_G_FLAG = AEC_ON ? 1 : 2;
//          end
//       end
//
//    CVC_AUTO:
//       RNR_G_FLAG = 0;
//       RNR_ON = (APP_b.Mode == 0 && ~Wind_Flag && RNR_on)
//       if RNR_ON
//          if (DMSout_b.SNR_mn < 1.5)
//             RNR_G_FLAG = AEC_ON ? 1 : 2;
//          end
//       end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - dmss_obj
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~RNR_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.dmss.residule_noise_reduction;

   .CODESEGMENT PM;

$cvc.mc.dmss_rnr:
   // clear RNR_G_FLAG
   M[r8 + $dmss.rnr.G_FLAG_FIELD] = 0;

   // RNR_on?
   r1 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_RNR;
   if NZ rts;

   // Wind_Flag?
   r0 = M[r9 + $cvc_send.data.wind_flag];
   if NZ rts;

   // Mic_mode?
   r0 = M[r9 + $cvc_send.data.mic_mode];
   if NZ rts;

   // rnr ON, (r0 = 0), don't corrupt r0 before return

   // RNR_G_FLAG decision
   r1 = 2;
   Null = M[r9 + $cvc_send.data.aec_inactive];
   if Z r1 = 1;

   r2 = M[r9 + $cvc_send.data.dms200_obj];
   r2 = M[r2 + $dms200.SNR_MN_FIELD];

   // CE : AUTO ?
   r3 = M[r9 + $cvc_send.data.use];
   Null = r3 - $cvc_send.AUTO;
   if Z jump rnr_auto;

   // DMSout_b.SNR_mn < 2 ?
   Null = r2 - Qfmt_(2.0, 8);
   if GE rts;

   // TP_mode < 3 ?
   r2 = M[r9 + $cvc_send.data.TP_mode];
   Null = r2 - 3;
   if GE rts;

   M[r8 + $dmss.rnr.G_FLAG_FIELD] = r1;

   // r0 = 0
   rts;

rnr_auto:
   // DMSout_b.SNR_mn < 1.5 ?
   // r1 = RNR_G_FLAG (target flag - 1 or 2)
   // r2 = SNR_mn
   Null = r2 - Qfmt_(1.5, 8);
   if GE rts;

   M[r8 + $dmss.rnr.G_FLAG_FIELD] = r1;

   // r0 = 0
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.dmss_tp
//
// DESCRIPTION:
//    TP_ON = (APP_b.Mode == 0 && TP_on && DOA0 == 90)
//    if TP_ON
//       MSC_ADPAT = (DMSout_b.VAD_voiced && ~Wind_Flag && ~Echo_Flag)
//    end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - dmss_obj
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~TP_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
//    Target Protection is for headset 2mic end-fire only.
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.dmss.target_protection;

   .CODESEGMENT PM;

$cvc.mc.dmss_tp:
   // TP_on?
   r1 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_TP;
   if NZ rts;

   // Mic_mode?
   r0 = M[r9 + $cvc_send.data.mic_mode];
   if NZ rts;

   // DOA0 == 90 ?
   r1 = M[r9 + $cvc_send.data.param];
   r0 = M[r1 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DOA0];
   r0 = r0 - 90;
   if NZ rts;

   // Now, r0 = 0 (TP_ON), don't corrupt r0 before return

   // MSC_ADPAT flag decision

   // DMSout_b.VAD_voiced?
   r1 = M[r9 + $cvc_send.data.dms200_obj];
   r1 = M[r1 + $dms200.VAD_VOICED_FIELD];

   // Wind_Flag ?
   Null = M[r9 + $cvc_send.data.wind_flag];
   if NZ r1 = 0;

   // Echo_Flag ?
   Null = M[r9 + $cvc_send.data.echo_flag];
   if NZ r1 = 0;

   // set MSC_ADPAT flag
   M[r8 + $dmss.tp.MSC_ADAPT_FLAG_FIELD] = r1;

   // r0 = 0 (return TP_ON flag)
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.aec520
//
// DESCRIPTION:
//    aec520 module configuration
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (aec_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.aec520;

   .CODESEGMENT PM;

$cvc.init.aec520:

   LIBS_SLOW_SW_ROM_PATCH_POINT($cvc.init.aec510.PATCH_ID_0, r3)

   // OMS/DMS AGGR needed for CNG offset
   r3 = M[r9 + $cvc_send.data.param];
   r2 = M[r3 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DMS_AGGR];
   M[r8 + $aec520.OFFSET_OMS_AGGRESSIVENESS] = r2;

   // voice off?
   r1 = 0;
   r3 = M[r9 + $cvc_send.data.cap_root_ptr];
   r0 = M[r3 + $cvc_send.cap.OP_FEATURE_REQUESTED];
   Null = r0 AND $cvc_send.REQUESTED_FEATURE_VOICE;
   if Z r1 = 1;

   // HFK_CONFIG word: AEC sub-module on/off flags
   r0 = M[r9 + $cvc_send.data.hfk_config];

   // CNG on/off
   r2 = r0 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_CNG;
   r2 = r2 OR r1;
   M[r8 + $aec520.FLAG_BYPASS_CNG_FIELD] = r2;

   // RER on/off
   r2 = r0 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_RER;
   r2 = r2 OR r1;
   M[r8 + $aec520.FLAG_BYPASS_RER_FIELD] = r2;

   // FBC on/off
   r2 = r0 AND ($M.GEN.CVC_SEND.CONFIG.HFK.BYP_FBC);
   M[r8 + $aec520.FLAG_BYPASS_FBC_FIELD] = r2;

   rts;

$cvc.init.vsm_fdnlp:
   // HD on/off flags
   r2 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r2 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_HD;
   M[r8 + $aec520.nlp.FLAG_BYPASS_HD_FIELD] = r0;
   rts;

.ENDMODULE;

// *****************************************************************************
// MODULE:
//    $cvc.mc.aec520
//
// DESCRIPTION:
//    aec520 module control
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (aec_obj / vsm_fdnlp)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~AEC_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.aec520;

   .CODESEGMENT PM;

$cvc.mc.aec520:

   LIBS_SLOW_SW_ROM_PATCH_POINT($cvc.mc.aec510.aec510.PATCH_ID_0, r1)

   // requested AEC?
   r0 = 1;
   r2 = M[r9 + $cvc_send.data.cap_root_ptr];
   r1 = M[r2 + $cvc_send.cap.OP_FEATURE_REQUESTED];
   Null = r1 AND $cvc_send.REQUESTED_FEATURE_AEC;
   if NZ r0 = 0;

   // AEC_ON ?
   r1 = M[r9 + $cvc_send.data.hfk_config];
   r1 = r1 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_AEC;
   r0 = r0 OR r1;
   M[r9 + $cvc_send.data.aec_inactive] = r0;
   if NZ rts;

   // Disable AEC if Low Volume Mode - headset only
   r1 = M[r9 + $cvc_send.data.use];
   Null = r1 - $cvc_send.HEADSET;
   if NZ rts;

   r1 = M[r2 + $cvc_send.cap.CUR_MODE];
   r1 = r1 - $M.GEN.CVC_SEND.SYSMODE.LOWVOLUME;
   if Z r0 = 1;

   M[r9 + $cvc_send.data.aec_inactive] = r0;
   rts;

$cvc.mc.aec520_nlp:
$cvc.mc.aec520_cng:
   r0 = M[r9 + $cvc_send.data.aec_inactive];
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.ref_delay
//
// DESCRIPTION:
//    aec520 reference delay process control
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - proc_obj (aec_obj or fba_ref)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    r0 -> ~(AEC_on || FBC_on)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.aec_ref_delay;

   .CODESEGMENT PM;

$cvc.mc.ref_delay:
   r2 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r2 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_FBC;
   Null = M[r9 + $cvc_send.data.aec_inactive];
   if Z r0 = 0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.event.echo_flag
//
// DESCRIPTION:
//    if(HD_mode) , Echo_Flag = 1
//    else Echo_Flag = VAD_REF
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - AEC_NLP data object
//    - r8 - rcv_vad flag pointer
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.event.echo_flag;

   .CODESEGMENT PM;

$cvc.event.echo_flag:
   // VAD_AEC
   r0 = M[r8];
   // HD_mode
   r1 = M[r7 + $aec520.nlp.FLAG_HD_MODE_FIELD];
   Null = r7;
   if Z r1 = 0;
   // Echo_Flag
   r0 = r0 OR r1;
   M[r9 + $cvc_send.data.echo_flag] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.aed100
//
// DESCRIPTION:
//    aed100 module configuration
//
//       DOA = ~~(M_Mic-1)*user.DOA + ~(M_Mic-1)*90;
//
//    DOA = user.DOA    : Multi-Mic
//    DOA = 90          : 1-mic
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (aed100_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.aed100;

   .CODESEGMENT PM;

$cvc.init.aed100:
   r2 = 90;
   r1 = M[r9 + $cvc_send.data.param];
   r0 = M[r1 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DOA0];
   Null = M[r9 + $cvc_send.data.mic_mode];
   if NZ r0 = r2;
   M[r8 + $aed100.DOA_FIELD] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.aed100
//
// DESCRIPTION:
//    aed100 module control
//
//       AED.voiced = DMSout_b.VAD_voiced && ~Echo_Flag && ~Wind_Flag;
//       if Mode == 0
//          AED.VAD_G = DMSS_TR0
//       else
//          AED.VAD_G = DMSout_b.G_G_interpolated
//       end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (aed100_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~AED_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.aed100;

   .CODESEGMENT PM;

$cvc.mc.aed100:
   r3 = M[r9 + $cvc_send.data.dmss_obj];
   r2 = M[r9 + $cvc_send.data.dms200_obj];

   // AED.voiced = DMSout_b.VAD_voiced && ~Echo_Flag && ~Wind_Flag;
   r0 = M[r2 + $dms200.VAD_VOICED_FIELD];
   Null = M[r9 + $cvc_send.data.wind_flag];
   if NZ r0 = 0;
   Null = M[r9 + $cvc_send.data.echo_flag];
   if NZ r0 = 0;
   M[r8 + $aed100.VOICED_FIELD] = r0;

   // if Mode == 0
   //    AED.VAD_G = DMSS_TR0
   // else
   //    AED.VAD_G = DMSout_b.G_G_interpolated
   // end
   r0 = M[r3 + $dmss.BEAM0_TR_FIELD];
   r1 = M[r2 + $dms200.PTR_G_FIELD];
   Null = M[r9 + $cvc_send.data.mic_mode];
   if NZ r0 = r1;
   M[r8 + $aed100.G_IN_FIELD] = r0;

   // ~AED_ON
   r0 = 0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.agc400
//
// DESCRIPTION:
//    agc400 module configuration - enable AGC VAD hold
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (agc400_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.agc400;

   .CODESEGMENT PM;

$cvc.init.agc400:
   // VAD hold on/off
   r0 = 1;
   M[r8 + $agc400.VAD_HOLD_ENABLE_FIELD] = r0;

   // AGC on/off
   r1 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_AGC;
   M[r8 + $agc400.FLAG_BYPASS_AGC] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.agc400
//
// DESCRIPTION:
//    agc400 module control
//
//       if AGC_ON
//           if Echo_Flag
//               AGC_Echo_hold = -AGC_Th_hang_Echo;
//           else
//               AGC_Echo_hold = min(AGC_Echo_hold + 1, 0);
//           end
//
//           if  ~VAD_Flag
//               AGC_Noise_hold = -AGC_Th_hang_Noise;
//           else
//               AGC_Noise_hold = min(AGC_Noise_hold + 1, 0);
//           end
//
//           VAD_AGC = (AGC_Echo_hold >= 0)  && (AGC_Noise_hold >= 0);
//       end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (agc400_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~AGC_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.agc400;

   .CODESEGMENT PM;

$cvc.mc.agc400:
   // vad_agc_obj -> r4
   r4 = M[r8 + $agc400.OFFSET_PTR_VAD_VALUE_FIELD];

   // if Echo_Flag
   //     AGC_Echo_hold = -AGC_Th_hang_Echo;
   // else
   //     AGC_Echo_hold = min(AGC_Echo_hold + 1, 0);
   // end
   r2 = M[r4 + $agc400.vad.ECHO_HOLD_FIELD];
   if NEG r2 = r2 + 1;
   r1 = M[r4 + $agc400.vad.ECHO_THRES_FIELD];
   Null = M[r9 + $cvc_send.data.echo_flag];
   if NZ r2 = -r1;
   M[r4 + $agc400.vad.ECHO_HOLD_FIELD] = r2;

   // if ~VAD_Flag
   //     AGC_Noise_hold = -AGC_Th_hang_Noise;
   // else
   //     AGC_Noise_hold = min(AGC_Noise_hold + 1, 0);
   // end
   r3 = M[r4 + $agc400.vad.NOISE_HOLD_FIELD];
   if NEG r3 = r3 + 1;
   r1 = M[r4 + $agc400.vad.NOISE_THRES_FIELD];
   Null = M[r9 + $cvc_send.data.vad_flag];
   if Z r3 = -r1;
   M[r4 + $agc400.vad.NOISE_HOLD_FIELD] = r3;

   // VAD_AGC = (AGC_Echo_hold >= 0)  && (AGC_Noise_hold >= 0);
   r1 = 0;
   Null = r2 OR r3;
   if Z r1 = 1;
   M[r4 + $agc400.vad.VAD_AGC_FIELD] = r1;

   // r0 = ~AGC_ON
   r0 = M[r8 + $agc400.FLAG_BYPASS_AGC];
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.mgdc_persist
//
// DESCRIPTION:
//    if MGDC_persist_on
//       MGDC.L2FBpXD = MGDC_state;
//    end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - mgdc_state_ptr
//    - r8 - module object (mgdc100_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.mgdc_persist;

   .CODESEGMENT PM;

$cvc.mgdc_persist.init:
   // MGDC_persist_on?
   r1 = M[r9 + $cvc_send.data.dmss_config];
   Null = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_MGDCPERSIST;
   if NZ rts;
   // set MGDC state
   r0 = M[r7];
   M[r8 + $mgdc100.L2FBPXD_FIELD] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.event.mgdc_state_upload
//
// DESCRIPTION:
//    MGDC_state = MGDC.L2FBpXD;
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - mgdc_state_ptr
//    - r8 - module object (mgdc100_obj)
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.event.mgdc_persist;

   .CODESEGMENT PM;

$cvc.mgdc_persist.state_upload:
   r0 = M[r8 + $mgdc100.L2FBPXD_FIELD];
   M[r7] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.mgdc100
//
// DESCRIPTION:
//
//    MGDC_ON = (Mode~=1) && (MGDC_on || omni_mode)
//    
//    if (~Echo_Flag) && (~Wind_Flag)
//      if OMSin_b.voiced
//         MGDC_update = 1;
//      else
//         MGDC_update = 2;
//      end
//    else
//      MGDC_update = 0;
//    end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (mgdc100_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~MGDC_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.mgdc100;

   .CODESEGMENT PM;

$cvc.mc.mgdc100:
   // Mode~=1?
   r0 = M[r9 + $cvc_send.data.mic_mode];
   Null = r0 - 1;
   if Z rts;

   // OMSin_b.voiced?
   r1 = 2;
   r2 = M[r8 + $mgdc100.PTR_OMS_VAD_FIELD];
   Null = M[r2];
   if NZ r1 = 1;
   // Echo_Flag?
   Null = M[r9 + $cvc_send.data.echo_flag];
   if NZ r1 = 0;
   // Wind_Flag?
   Null = M[r9 + $cvc_send.data.wind_flag];
   if NZ r1 = 0;
   // MGDC_update
   M[r8 + $mgdc100.MGDC_UPDATE_FIELD] = r1;

   // MGDC always ON if omni_mode.
   r0 = 0;
   Null = M[r8 + $mgdc100.OMNI_MODE_FIELD];
   if NZ rts;

   // MGDC_on?
   r1 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_MGDC;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mgdc.harm_dynamic
//
// DESCRIPTION:
//    if mic_mode == 3           % Mic_0 Malfunction, Mic_1 is used
//       harm.inp_x = inp_d1;
//    else                       % Mic_0 is used
//       harm.inp_x = inp_d0;
//    end 
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - harm_inputs
//    - r8 - module object (snd_harm_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.event.mgdc_harm_dynamic;

   .CODESEGMENT PM;

$cvc.mgdc.harm_dynamic:
   // Mode ~= 1 ?
   r2 = M[r9 + $cvc_send.data.mic_mode];
   Null = r2 - 1;
   if Z rts;

   // inp_d0
   r0 = M[r7 + 0*ADDR_PER_WORD];
   // inp_d1
   r1 = M[r7 + 1*ADDR_PER_WORD];
   // mic_mode == 3 ?
   Null = r2 - 3;
   if Z r0 = r1;
   // set harm.inp_x
   r0 = M[r0 + $M.filter_bank.Parameters.OFFSET_PTR_FRAME];
   M[r8 + $harm100.INP_X_FIELD] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.ndvc200
//
// DESCRIPTION:
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (ndvc_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.ndvc200;

   .CODESEGMENT PM;

$cvc.init.ndvc200:
   r1 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_NDVC;
   M[r8 + $ndvc200.OFFSET_BYPASS_FLAG] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.asf100
//    $cvc.init.asf200
//
// DESCRIPTION:
//    Beam1_DOA = AUTO ? DOA1 : DOA0
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (asf_object)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
//    2mic WNR only available for headset_2mic_ef
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.asf200;

   .CODESEGMENT PM;

$cvc.init.asf100:
   // 2mic WNR
   r1 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_WNR;
   r1 = M[r9 + $cvc_send.data.use];
   Null = r1 - $cvc_send.HEADSET;
   if NZ r0 = 1;
   Null = M[r9 + $cvc_send.data.end_fire];
   if Z r0 = 1;
   M[r8 + $asf100.BYPASS_FLAG_WNR_FIELD] = r0;

   // SPP
   r1 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_SPP;
   M[r8 + $asf100.BYPASS_FLAG_COH_FIELD] = r0;
   rts;

$cvc.init.asf200:
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.asf100
//
// DESCRIPTION:
//
// MODIFICATIONS:
//    ASF.Beam0_Switchable = AUTO ? Default : TP_mode < 1;
//    ASF_ON = (Mode == 0 && ASF_on)
//
// INPUTS:
//    - r8 - module object (asf_object)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~ASF_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.asf100;

   .CODESEGMENT PM;

$cvc.mc.asf100:
   // AUTO?
   r1 = M[r9 + $cvc_send.data.use];
   Null = r1 - $cvc_send.AUTO;
   if Z jump end_beam0_switch;
      // ASF.Beam0_Switchable = TP_mode < 1
      r0 = 0;
      r1 = M[r9 + $cvc_send.data.TP_mode];
      r1 = r1 - 1;
      if LT r0 = 1;
      M[r8 + $asf100.bf.BEAM0_SWITCHABLE_FLAG_FIELD] = r0;
   end_beam0_switch:

$cvc.mc.asf200:
   // Mode == 0?
   r0 = M[r9 + $cvc_send.data.mic_mode];
   if NZ rts;

   // ASF_on?
   r1 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_ASF;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.user.dms200.wnr.initialize
//
// DESCRIPTION:
//    User Wrapper
//
//    WNR 1mic initialization
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - wnr_obj
//    - r8 - dms200_obj
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.user.wnr_oms.init;

   .CODESEGMENT PM;

$cvc.user.dms200.wnr.initialize:
   // OMS_WBM_on?
   r0 = M[r9 + $cvc_send.data.hfk_config];
   Null = r0 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_WNR;
   if NZ rts;

   // end_fire?
   Null = M[r9 + $cvc_send.data.end_fire];
   if NZ rts;

   // OMS WNR initialize
   // r7 -> wnr_obj
   // r8 -> dms200_obj
   jump $dms200.wnr.initialize;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.user.dms200.va_init
//
// DESCRIPTION:
//    Set DMS ouput, according to VOICE/VA configuration.
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - pointer to VA output (channel structure: real/imag/BExp)
//    - r8 - dms200_obj
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.user.dms200.va_init;

   .CODESEGMENT PM;

$cvc.user.dms200.va_init:
   // get feature configuration
   r1 = M[r9 + $cvc_send.data.cap_root_ptr];
   r1 = M[r1 + $cvc_send.cap.OP_FEATURE_REQUESTED];

   // VA enabled?
   Null = r1 AND $cvc_send.REQUESTED_FEATURE_VA;
   if Z r7 = 0;
   // set VA output
   M[r8 + $dms200.Y_VA_FIELD] = r7;

   // VOICE enabled?
   r0 = M[r8 + $M.oms280.X_FIELD];
   Null = r1 AND $cvc_send.REQUESTED_FEATURE_VOICE;
   if Z r0 = 0;
   // set VOICE output
   M[r8 + $M.oms280.Y_FIELD] = r0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.dms200
//
// DESCRIPTION:
//    DMSout.Mode = AUTO ? TMP_mode : 0
//    Auto_Th_on = HS ? (DOA==90) : 0
//    if DMSout_Harm_on
//       Connect Harm to dms200_obj
//    end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (dms200_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.dms200;

   .CODESEGMENT PM;

$cvc.init.dms200:

   LIBS_SLOW_SW_ROM_PATCH_POINT($cvc.init.dms200.PATCH_ID_0, r2)

   // DMSout.Mode: DMP_MODE is always 0 in CE
   r2 = M[r9 + $cvc_send.data.param];
   r0 = M[r2 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DMP_MODE];
   r1 = M[r8 + $dms200.DMS_OBJ_FIELD];
   M[r1 + $dms200.dms.MASTER_DMS_MODE_FIELD] = r0;

   // Auto_Th_on = HS ? (DOA==90) : 0
   r0 = 0;
   r1 = M[r9 + $cvc_send.data.use];
   Null = r1 - $cvc_send.HEADSET;
   if NZ r0 = 1;
   Null = M[r9 + $cvc_send.data.end_fire];
   if Z r0 = 1;
   M[r8 + $dms200.BYPASS_AUTO_TH_FIELD] = r0;

   // SPP_on
   r2 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r2 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_SPP;
   M[r8 + $dms200.BYPASS_SPP_FIELD] = r0;

   // VAD_S_on
   r0 = r2 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_VAD_S;
   M[r8 + $dms200.BYPASS_VAD_S_FIELD] = r0;

$cvc.init.dms200.common:
   // NFloor_on
   r1 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_NFLOOR;
   M[r8 + $dms200.BYPASS_NFLOOR_FIELD] = r0;

   // DMSout_Harm_on?
   Null = r1 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_HARM;
   if Z jump $cvc.init.harm_export;

   // Now DMSout_Harm_on is 0, harmonicity is not used
   M[r8 + $M.oms280.PTR_HARM_VALUE_FIELD] = 0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.dms200.speaker_va
//
// DESCRIPTION:
//    initialize DMS objects for speaker VA channels
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - DMS200 object 
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.dms200.speaker_va;

   .CODESEGMENT PM;

$cvc.init.dms200.speaker_va:
   // DMSout.Mode
   r0 = 1;
   r1 = M[r8 + $dms200.DMS_OBJ_FIELD];
   M[r1 + $dms200.dms.MASTER_DMS_MODE_FIELD] = r0;

   r0 = $dms200.MS_DUR.VA;
   M[r8 + $M.oms280.MIN_SEARCH_TIME_FIELD] = r0;

   jump $cvc.init.dms200.common;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.dms200
//
// DESCRIPTION:
//    if CE && mic_mode==0
//       NSN_Aggrt = NSN_Aggr * (TP_mode < 4);
//    end
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (dms200_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~DMS_ON (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.dms200;

   .CODESEGMENT PM;

$cvc.mc.dms200:
   // DMS_ON - always on
   r0 = 0;

   // CE : AUTO ?
   r1 = M[r9 + $cvc_send.data.use];
   Null = r1 - $cvc_send.AUTO;
   if Z rts;

   // 1mic?
   Null = M[r9 + $cvc_send.data.mic_mode];
   if NZ rts;

   // NSN_Aggrt = NSN_Aggr * (TP_mode < 4);
   r2 = M[r8 + $M.oms280.PARAM_FIELD];
   r2 = M[r2 + $dms200.param.NSN_AGGR_FIELD];
   r1 = M[r9 + $cvc_send.data.TP_mode];
   Null = r1 - 4;
   if GE r2 = 0;
   M[r8 + $dms200.NSN_AGGRT_FIELD] = r2;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.dms_out
//
// DESCRIPTION:
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (dms200_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - ~DMSout_on (bypass flag)
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.dms_out;

   .CODESEGMENT PM;

$cvc.mc.dms_out:
   r0 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r0 AND $M.GEN.CVC_SEND.CONFIG.HFK.BYP_DMS;
   rts;

.ENDMODULE;

#if defined(CVC_INCLUDE_NC)
// *****************************************************************************
// MODULE:
//    $cvc.init.nc100
//
// DESCRIPTION:
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - module object (nc100_obj)
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_init.nc100;

   .CODESEGMENT PM;

$cvc.init.nc100:
   // Export 'LRatio' to dms200 module when nc100 is active
   r0 = M[r8 + $nc100.LRATIO_PTR_FIELD];
   r1 = M[r9 + $cvc_send.data.dmss_config];
   Null = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_NC;
   if NZ r0 = 0;

   r2 = M[r9 + $cvc_send.data.dms200_obj];
   r2 = M[r2 + $dms200.DMS_OBJ_FIELD];
   M[r2 + $dms200.dms.LRATIO_INTERPOLATED_FIELD] = r0;

   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.nc100_process
//
// DESCRIPTION:
//    CVC_CE:
//    if S.NC_on && ~SND.APP_b.Wind_Flag
//         NC_ctrl   = SND.DMSout_b.LRatio_interpolated;
//         L_compara = SND.APP_p.NC_ctrl_trans*(NC_ctrl+ SND.APP_p.NC_ctrl_bias);
//         compara   = 1./(1+pow2(L_compara));
//         SND.NC_b  = func_NoiseCanceller(SND.SYS_c,Z1,BExp_Z1,Z0,BExp_Z0,SND.NC_p,SND.NC_b,compara);
//
//         if  SND.DMSout_b.SNR_mn < 2 && (SND.APP_b.TP_mode < 2)
//            Z0 =  Z0.*(SND.NC_p.G_yWei*SND.NC_b.G_interpolated + SND.NC_p.G_xWei);
//         end
//    end 
//
//    CVC_AUTO:
//    if S.NC_on && ~SND.APP_b.Wind_Flag && ~S.TMP_mode
//         NC_ctrl   = SND.DMSout_b.LRatio_interpolated;
//         L_compara = SND.APP_p.NC_ctrl_trans*(NC_ctrl+ SND.APP_p.NC_ctrl_bias);
//         compara   = 1./(1+pow2(L_compara));
//         SND.NC_b  = func_NoiseCanceller(SND.SYS_c,Z1,BExp_Z1,Z0,BExp_Z0,SND.NC_p,SND.NC_b,compara);
         
//         if  SND.DMSout_b.SNR_mn < 1.5
//            Z0 =  Z0.*(SND.NC_p.G_yWei*SND.NC_b.G_interpolated + SND.NC_p.G_xWei);
//         end
//    end 
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - nc100_obj
//    - r9 - cvc data root
//
// OUTPUTS: none
//
// TRASHED REGISTERS:
//
// CPU USAGE:
// *****************************************************************************

.MODULE $M.CVC_SEND.module_control.nc;

   .CODESEGMENT PM;

   .CONST $cvc.DMSS_AGGR                  0.4;
   .CONST $cvc.DMSS_GMAX                  1.0;
   .CONST $cvc.DMSS_GMIN                 (1.0 - $cvc.DMSS_AGGR);

$cvc.mc.nc100_process:
   push rLink;

   //NC_on?
   r1 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r1 AND $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_NC;
   if NZ jump $pop_rLink_and_rts;
  
   //Wind_Flag
   r0 = M[r9 + $cvc_send.data.wind_flag];
   if NZ jump $pop_rLink_and_rts;

   // Mic_mode?
   r0 = M[r9 + $cvc_send.data.mic_mode];
   if NZ jump $pop_rLink_and_rts;

   // CE : AUTO ?
   r3 = M[r9 + $cvc_send.data.use];
   Null = r3 - $cvc_send.AUTO;
   if Z jump nc100_auto;

      //CE: NC process
      push r9;
      call $nc100.process;
      pop  r9;
      //if  SND.DMSout_b.SNR_mn < 2 && (SND.APP_b.TP_mode < 2)
      r0 = M[FP + $nc100.SNR_MN_FIELD];
      r0 = M[r0];
      Null = r0 - Qfmt_(2.0, 8);
      if GE jump $pop_rLink_and_rts;
   
      r0 = M[r9 + $cvc_send.data.TP_mode];     
      Null = r0 - 2;
      if GE jump $pop_rLink_and_rts;
      
      //CE: NC gain apply  
      call $nc100.gain_apply;
      jump $pop_rLink_and_rts;
   
      //Auto: 
nc100_auto:
      r3   = M[r9 + $cvc_send.data.param];
      Null = M[r3 + $M.GEN.CVC_SEND.PARAMETERS.OFFSET_DMP_MODE];
      if NZ jump $pop_rLink_and_rts;

      //Auto: NC process      
      call $nc100.process;
      
      //if  SND.DMSout_b.SNR_mn < 1.5 
      r0 = M[FP + $nc100.SNR_MN_FIELD];
      r0 = M[r0];
      Null = r0 - Qfmt_(1.5, 8);
      if GE jump $pop_rLink_and_rts;
      
      //Auto: NC gain apply
      call $nc100.gain_apply;
      jump $pop_rLink_and_rts;

.ENDMODULE;
#endif // CVC_INCLUDE_NC


// *****************************************************************************
// MODULE:
//    $cvc.mc.voice
//
// DESCRIPTION:
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - voice processing object
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - bypass flag
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.voice;

   .CODESEGMENT PM;

$cvc.mc.voice:
   // requested voice feature?
   r0 = 1;
   r1 = M[r9 + $cvc_send.data.cap_root_ptr];
   r1 = M[r1 + $cvc_send.cap.OP_FEATURE_REQUESTED];
   Null = r1 AND $cvc_send.REQUESTED_FEATURE_VOICE;
   if NZ r0 = 0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.mc.va
//
// DESCRIPTION:
//
// MODIFICATIONS:
//
// INPUTS:
//    - r8 - VA processing object
//    - r9 - cvc data root object
//
// OUTPUTS:
//    - r0 - bypass flag
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.module_control.va;

   .CODESEGMENT PM;

$cvc.mc.va:
   // requested VA feature?
   r0 = 1;
   r1 = M[r9 + $cvc_send.data.cap_root_ptr];
   r1 = M[r1 + $cvc_send.cap.OP_FEATURE_REQUESTED];
   Null = r1 AND $cvc_send.REQUESTED_FEATURE_VA;
   if NZ r0 = 0;
   rts;

.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.pre_process
//
// DESCRIPTION:
//
// MODIFICATIONS:
//
// INPUTS:
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.pre_process;

   .CODESEGMENT PM;

$cvc.pre_process:
   LIBS_SLOW_SW_ROM_PATCH_POINT($cvc.pre_process.PATCH_ID_0, r1)

   rts;

.ENDMODULE;
