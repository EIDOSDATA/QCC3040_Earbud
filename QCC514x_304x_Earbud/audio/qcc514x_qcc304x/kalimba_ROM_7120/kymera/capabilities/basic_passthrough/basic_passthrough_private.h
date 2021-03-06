/****************************************************************************
 * Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup basic_passthrough
 * \file  basic_passthrough_private.h
 * \ingroup capabilities
 *
 * Basic passthrough operator private header file. <br>
 *
 */

#ifndef _BASIC_PASSTHROUGH_PRIVATE_H_
#define _BASIC_PASSTHROUGH_PRIVATE_H_
/*****************************************************************************
Include Files
*/
#include "capabilities.h"
#include "basic_passthrough.h"
#include "basic_passthrough_shared_const.h"
#include "basic_passthrough_struct.h"
#include "common_conversions.h"
#ifdef INSTALL_TTP
#include "ttp/ttp.h"
#include "ttp_utilities.h"
#endif

/****************************************************************************
Public Constant Definitions
*/
#define BASIC_PASSTHROUGH_VERSION_LENGTH                       2

/** default block size for this operator's terminals */
#define BASIC_PASSTHROUGH_DEFAULT_BLOCK_SIZE                   1

/** Active channel mask. */
#define CHANNEL_MASK(ID) (1<<(ID))


/* these to be in audio functions or wherever */
#define GAIN_DEFAULT                                    FRACTIONAL(1.0)
#define XOR_DEFAULT                                     0x555555
#define GARG_DEFAULT                                    1

/* These labels are exposed in a bit of a strange way, so the address of the
 * variables declared below is the program address of the label.
 */
/** ASM audio copy operation label */
extern unsigned mono_audio_loop;

/** ASM data copy operation label */
extern unsigned mono_data_loop;

/** ASM XOR copy operation label */
extern unsigned mono_xor_loop;

/** ASM gargle copy operation label */
extern unsigned mono_garg_loop;

/****************************************************************************
Public Type Declarations
*/

/*****************************************************************************
Private Function Definitions
*/
/* ASM processing function */
extern void basic_passthrough_processing(BASIC_PASSTHROUGH_OP_DATA *op_data, unsigned samples_to_process);

/* Message handlers */
extern bool basic_passthrough_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);
extern bool basic_passthrough_destroy(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);
extern bool basic_passthrough_reset(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);
extern bool basic_passthrough_connect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);
extern bool basic_passthrough_disconnect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);
extern bool basic_passthrough_buffer_details(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);
extern bool basic_passthrough_get_sched_info(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);
extern bool basic_passthrough_get_data_format(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);

/* Op msg handlers */
extern bool basic_passthrough_opmsg_enable_fadeout(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_disable_fadeout(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_change_input_data_type(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_change_output_data_type(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_data_stream_based(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_obpm_set_control(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_obpm_get_params(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_obpm_get_defaults(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_obpm_set_params(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_obpm_get_status(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_set_buffer_size(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_load_config(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool basic_passthrough_opmsg_set_sample_rate(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);

#ifdef INSTALL_OPERATOR_TTP_PASS
extern bool ttp_passthrough_opmsg_set_ttp_latency(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool ttp_passthrough_opmsg_set_latency_limits(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool ttp_passthrough_opmsg_set_ttp_params(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool ttp_passthrough_opmsg_set_sample_rate(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
#endif /* INSTALL_OPERATOR_TTP_PASS */


/* Data processing function */
extern void basic_passthrough_process_data(OPERATOR_DATA*, TOUCHED_TERMINALS*);

extern bool ups_params_basic_passthrough(void* instance_data,PS_KEY_TYPE key,PERSISTENCE_RANK rank,
                 uint16 length,  unsigned* data, STATUS_KYMERA status,uint16 extra_status_info);

#endif /* _BASIC_PASSTHROUGH_PRIVATE_H_ */
