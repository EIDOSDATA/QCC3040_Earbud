/*****************************************************************************
Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.

FILE NAME
    ama_send_envelope.c

DESCRIPTION
    Sends control envelope to transport
*/

#ifdef INCLUDE_AMA
#include <panic.h>
#include <stdlib.h>
#include "logging.h"
#include "ama_send_envelope.h"
#include "ama_private.h"
#include "ama_log.h"

void AmaSendEnvelope_Send(ControlEnvelope* control_envelope_out)
{
#define AMA_HEADER_SIZE_SMALL_ENVELOPE 3
#define AMA_HEADER_SIZE_LARGE_ENVELOPE 4
#define SIZE_LARGE_ENVELOPE 0xff

    uint16 header_size = AMA_HEADER_SIZE_SMALL_ENVELOPE;
    size_t envelope_size = control_envelope__get_packed_size(control_envelope_out);
    uint8 *packed_envelope;

    if(envelope_size > SIZE_LARGE_ENVELOPE)
    {
        header_size = AMA_HEADER_SIZE_LARGE_ENVELOPE;
    }
    packed_envelope = PanicUnlessMalloc(envelope_size + header_size);

    if(control_envelope__pack(control_envelope_out, (packed_envelope + header_size)) != envelope_size)
    {
        DEBUG_LOG("AMA Error building packed envelope %d", envelope_size);
    }

    /* don't include the header size ... transport will take care of that */
    envelope_size = AmaParse_PrepareControlData(packed_envelope, envelope_size);
    AmaLog_ControlEnvelope(AMA_LOG_SENDING, control_envelope_out, packed_envelope, envelope_size);
    AmaNotifyAppMsg_ControlPktMsg(packed_envelope, envelope_size);

    free(packed_envelope);
}

#endif /* INCLUDE_AMA */
