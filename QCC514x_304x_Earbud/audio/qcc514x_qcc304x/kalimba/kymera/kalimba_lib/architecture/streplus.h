

#include "io_map.asm"
#include "io_defs.asm"

// Extra defines to work with assembly code

// -- Flag constants --
   .CONST $N_FLAG                         1;
   .CONST $Z_FLAG                         2;
   .CONST $C_FLAG                         4;
   .CONST $V_FLAG                         8;
   .CONST $UD_FLAG                        16;
   .CONST $SV_FLAG                        32;
   .CONST $BR_FLAG                        64;
   .CONST $UM_FLAG                        128;

   .CONST $NOT_N_FLAG                     (65535-$N_FLAG);
   .CONST $NOT_Z_FLAG                     (65535-$Z_FLAG);
   .CONST $NOT_C_FLAG                     (65535-$C_FLAG);
   .CONST $NOT_V_FLAG                     (65535-$V_FLAG);
   .CONST $NOT_UD_FLAG                    (65535-$UD_FLAG);
   .CONST $NOT_SV_FLAG                    (65535-$SV_FLAG);
   .CONST $NOT_BR_FLAG                    (65535-$BR_FLAG);
   .CONST $NOT_UM_FLAG                    (65535-$UM_FLAG);

