/****************************************************************************
 * Copyright (c) 2016 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  base_multi_chan_op.c
 * \ingroup  operators
 *
 *  Multi-channel Base operator.
 *  This is not a complete capability, it contains common operations
 *    utilized by multi-channel capabilities
 */

/******************************************************************************
Include Files
*/
#include "capabilities.h"
#include "base_multi_chan_op.h"

#ifdef INSTALL_METADATA
#include "ttp/ttp.h"
#include "ttp/timed_playback.h"
#include "ttp_utilities.h"
#endif

#ifdef BASE_MULTI_CHAN_OFFLOAD
#ifdef INSTALL_THREAD_OFFLOAD
#include "thread_offload.h"
#else
#error "BASE_MULTI_CHAN_OFFLOAD can't be used without INSTALL_THREAD_OFFLOAD"
#endif

#define SELF_KICK_TIME 2000

#endif

/****************************************************************************
Private Function Declarations
*/
static inline MULTI_CHANNEL_DEF *get_class_data(OPERATOR_DATA *op_data)
{
    return (MULTI_CHANNEL_DEF *) base_op_get_class_ext(op_data);
}

static inline void set_class_data(OPERATOR_DATA *op_data, MULTI_CHANNEL_DEF *class_data)
{
    base_op_set_class_ext(op_data, class_data);
}

inline static MULTI_CHANNEL_CHANNEL_STRUC* GetChannelPtr(MULTI_CHANNEL_DEF *chan_def,unsigned chan_idx)
{
   return (MULTI_CHANNEL_CHANNEL_STRUC*) (&chan_def->chan_data[chan_idx*chan_def->chan_obj_size]);
}

/* 
 * Get a pointer to the source connection buffer pointer
 * For the offloaded case this is different from the buffer the processing code sees 
 */
static inline tCbuffer **chan_connection_src_buff(MULTI_CHANNEL_DEF *chan_def, MULTI_CHANNEL_CHANNEL_STRUC *lp_chan)
{
#ifdef BASE_MULTI_CHAN_OFFLOAD
   if ((chan_def->config & MULTI_OFFLOAD_FLAG) != 0)
   {
       return &lp_chan->connection_output;
   }
   else
#endif
   {
       return &lp_chan->source_buffer_ptr;
   }
}

/* 
 * Get a pointer to the sink connection buffer pointer
 * For the offloaded case this is different from the buffer the processing code sees 
 */
static inline tCbuffer **chan_connection_sink_buff(MULTI_CHANNEL_DEF *chan_def, MULTI_CHANNEL_CHANNEL_STRUC *lp_chan)
{
#ifdef BASE_MULTI_CHAN_OFFLOAD
   if ((chan_def->config & MULTI_OFFLOAD_FLAG) != 0)
   {
       return &lp_chan->connection_input;
   }
   else
#endif
   {
       return &lp_chan->sink_buffer_ptr;
   }
}

#ifdef BASE_MULTI_CHAN_OFFLOAD
/**
 * Clones a given buffer.
 */
static tCbuffer *create_clone_buffer(tCbuffer *buffer_to_clone)
{
    unsigned buffer_flags;
    tCbuffer *ret_buffer;

    /* Make a copy of the connection buffer to give to the processing code
     * This runs asynchronously, so can't use the buffers visible externally
     */
    buffer_flags = buffer_to_clone->descriptor;
    ret_buffer = cbuffer_create(buffer_to_clone->base_addr,
                    cbuffer_get_size_in_words(buffer_to_clone), buffer_flags);


    if (ret_buffer == NULL)
    {
        return NULL;
    }

    /* Copy buffer pointers in case the connection was previously active */
    ret_buffer->read_ptr = buffer_to_clone->read_ptr;
    ret_buffer->write_ptr = buffer_to_clone->write_ptr;

    /* Set the aux pointer in case the connected operator runs in-place.
     * The in place flag is preserved so the amount of space calculation
     * for the buffer should be correct. */
    ret_buffer->aux_ptr = buffer_to_clone->aux_ptr;

    return ret_buffer;
}

#endif /* BASE_MULTI_CHAN_OFFLOAD */


/* Function check if one of the outputs/inputs are connected. */
static bool multi_channel_terminals_connected(MULTI_CHANNEL_CHANNEL_STRUC  *first_active, unsigned config)
{
    while (first_active != NULL)
    {
        if (first_active->source_buffer_ptr != NULL)
        {
            return TRUE;
        }
#ifndef DISABLE_IN_PLACE
        /* For in-place operators the input buffer will be reused,
         * so we need to check if it is connected or not. */
        if(config & MULTI_INPLACE_FLAG)
        {
            if (first_active->sink_buffer_ptr != NULL)
            {
                return TRUE;
            }
        }
#endif
        /* Check the next channel. */
        first_active = first_active->next_active;
    }
    return FALSE;
}

/****************************************************************************
Public Function Declarations
*/

MULTI_CHANNEL_DEF* multi_channel_get_channel_def(OPERATOR_DATA *op_data)
{
   return get_class_data(op_data);
}

void multi_channel_set_callbacks(OPERATOR_DATA *op_data, MULTI_CHANNEL_CREATE_FN create_fn,MULTI_CHANNEL_DESTROY_FN destroy_fn)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   chan_def->create_fn  = create_fn;
   chan_def->destroy_fn = destroy_fn;
}

void multi_channel_set_buffer_size(OPERATOR_DATA *op_data, unsigned buffer_size)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   chan_def->buffer_size  = buffer_size;
}

void multi_channel_set_block_size(OPERATOR_DATA *op_data, unsigned block_size)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   chan_def->block_size  = block_size;
}

bool multi_channel_create(OPERATOR_DATA *op_data,unsigned flags,unsigned chan_size)
{
   unsigned num_channels;
   MULTI_CHANNEL_DEF *ptr;

   patch_fn_shared(base_multi_channel);

   num_channels = base_op_get_cap_data(op_data)->max_sources;

   /* Let the channel structure be expanded for application specific data.
   Need the structure size in words */
   chan_size = ((chan_size - 1)/sizeof(unsigned)) + 1;

   ptr = (MULTI_CHANNEL_DEF*)xzpmalloc( sizeof(MULTI_CHANNEL_DEF) +  num_channels*chan_size*sizeof(unsigned));
   if(ptr==NULL)
   {
      return FALSE;
   }

   ptr->chan_obj_size = chan_size;

#ifndef DISABLE_IN_PLACE
   if(flags&MULTI_INPLACE_FLAG)
   {
      ptr->config |= MULTI_INPLACE_FLAG;
   }
#endif
#ifdef INSTALL_METADATA
   ptr->last_tag_type  = UNSUPPORTED_TAG;
   if(flags&MULTI_METADATA_FLAG)
   {
      ptr->config |= MULTI_METADATA_FLAG;
   }
#endif

#ifdef BASE_MULTI_CHAN_OFFLOAD
   if(flags&MULTI_OFFLOAD_FLAG)
   {
      if(flags&MULTI_HOT_CONN_FLAG)
      {
         L2_DBG_MSG("multi_channel_create: offload is not compatible with hot connect /disconnect");
         return FALSE;
      }
      ptr->config |= MULTI_OFFLOAD_FLAG;
   }
#endif

   if(flags&MULTI_HOT_CONN_FLAG)
   {
      ptr->config |= MULTI_HOT_CONN_FLAG;
   }

   ptr->block_size  = MULTI_CHAN_DEFAULT_BLOCK_SIZE;


   /* Save Pointer to channel definition in operator data */
   set_class_data(op_data, ptr);

   return TRUE;
}

void multi_channel_detroy(OPERATOR_DATA *op_data)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   MULTI_CHANNEL_CHANNEL_STRUC *lp_chan;

   if(chan_def==NULL)
   {
      return;
   }

   set_class_data(op_data, NULL);

   /* Destroy any active channels */
   if(chan_def->destroy_fn!=NULL)
   {
      for(lp_chan=chan_def->first_active;lp_chan;lp_chan=lp_chan->next_active)
      {
            chan_def->destroy_fn(op_data,lp_chan);
      }
   }

   pfree(chan_def);
}

bool multi_channel_connect(OPERATOR_DATA *op_data,void *message_data, unsigned *response_id, void **response_data)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   const CAPABILITY_DATA *cap_data = base_op_get_cap_data(op_data);
   unsigned  terminal_id;
   tCbuffer *pbuffer;
   unsigned  terminal_num,term_mask;
   MULTI_CHANNEL_CHANNEL_STRUC  *lp_chan;
   tCbuffer **pChanSinkBuff, **pChanSrcBuff;

   patch_fn_shared(base_multi_channel);
   
   terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
   pbuffer = OPMGR_GET_OP_CONNECT_BUFFER(message_data);

   /* Verify channel system and create response */
   if ((chan_def==NULL) || !base_op_connect(op_data, message_data, response_id, response_data))
   {
        return FALSE;
   }

   /* Can we connect while running ?*/
   if (opmgr_op_is_running(op_data) && !(chan_def->config&MULTI_HOT_CONN_FLAG))
   {
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
   }

   terminal_num = terminal_id & ~TERMINAL_SINK_MASK;
   if (terminal_num >= cap_data->max_sources)
   {
        /* invalid terminal id */
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
   }

   /* Update Channels */
   lp_chan    = GetChannelPtr(chan_def,terminal_num);
   term_mask  = 1<<terminal_num;

   pChanSrcBuff = chan_connection_src_buff(chan_def, lp_chan);
   pChanSinkBuff = chan_connection_sink_buff(chan_def, lp_chan);

   if (terminal_id & TERMINAL_SINK_MASK)
   {
       /* Connecting Sink Terminal of channel */
       *pChanSinkBuff = pbuffer;

#ifdef INSTALL_METADATA
      if((chan_def->config&MULTI_METADATA_FLAG) && (chan_def->metadata_ip_buffer==NULL) && buff_has_metadata(pbuffer))
      {
         chan_def->metadata_ip_buffer = pbuffer;
         /* Reset the tag history as new metadata buffer is used. */
         chan_def->last_tag_type  = UNSUPPORTED_TAG;
      }
#endif /* INSTALL_METADATA */
   }
   else
   {
      /* Connecting Source Terminal of channel */
      *pChanSrcBuff = pbuffer;

#ifdef INSTALL_METADATA
      if((chan_def->config&MULTI_METADATA_FLAG) && (chan_def->metadata_op_buffer==NULL) && buff_has_metadata(pbuffer) )
      {
         chan_def->metadata_op_buffer = pbuffer;
      }
#endif /* INSTALL_METADATA */
   }

   if (*pChanSrcBuff != NULL && *pChanSinkBuff != NULL)
   {
      if((chan_def->create_fn==NULL) || chan_def->create_fn(op_data,lp_chan,terminal_num))
      {
         /* Mark channel as connected */
         chan_def->active_chan_mask |= term_mask;

         /* Add channel to top of active list and mark as active */
         lp_chan->next_active = chan_def->first_active;
         chan_def->first_active     = lp_chan;
      }
      else
      {
         /* Failed to create channel */
         base_op_change_response_status(response_data, STATUS_CMD_FAILED);
         return TRUE;
      }
   }
   return TRUE;
}

bool multi_channel_disconnect(OPERATOR_DATA *op_data,void *message_data, unsigned *response_id, void **response_data)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   const CAPABILITY_DATA *cap_data = base_op_get_cap_data(op_data);
   unsigned  terminal_id = OPMGR_GET_OP_DISCONNECT_TERMINAL_ID(message_data);
   MULTI_CHANNEL_CHANNEL_STRUC  *lp_chan;
   unsigned  terminal_num,term_mask;
   tCbuffer **pChanSinkBuff, **pChanSrcBuff;

   /* Verify channel system and create response */
   if ((chan_def==NULL) || !base_op_disconnect(op_data, message_data, response_id, response_data))
   {
        return FALSE;
   }

   /* Can we disconnect while running ?*/
   if (opmgr_op_is_running(op_data) && !(chan_def->config&MULTI_HOT_CONN_FLAG))
   {
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
   }

   terminal_num = terminal_id & ~TERMINAL_SINK_MASK;
   if (terminal_num >= cap_data->max_sources)
   {
        /* invalid terminal id */
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
   }

   lp_chan   = GetChannelPtr(chan_def,terminal_num);
   term_mask = 1<<terminal_num;

   pChanSrcBuff = chan_connection_src_buff(chan_def, lp_chan);
   pChanSinkBuff = chan_connection_sink_buff(chan_def, lp_chan);

   if (terminal_id & TERMINAL_SINK_MASK)
   {
#ifdef INSTALL_METADATA
        if((chan_def->config&MULTI_METADATA_FLAG) && (chan_def->metadata_ip_buffer == *pChanSinkBuff) )
        {
            unsigned i,num_channels = cap_data->max_sources;
            bool found_alternative = FALSE;

            for (i = 0; i < num_channels; i++)
            {
                tCbuffer *pbuffer;

                if (i == terminal_num)
                {
                    continue;
                }
                pbuffer = *chan_connection_sink_buff(chan_def, GetChannelPtr(chan_def,i));;
                if ( (pbuffer!=NULL) && buff_has_metadata(pbuffer))
                {
                    chan_def->metadata_ip_buffer = pbuffer;
                    found_alternative = TRUE;
                    break;
                }
            }
            if (!found_alternative)
            {
                chan_def->metadata_ip_buffer = NULL;
            }
        }
#endif /* INSTALL_METADATA */

        /* Disconnecting Sink */
        *pChanSinkBuff  = NULL;
   }
   else
   {
#ifdef INSTALL_METADATA
        if((chan_def->config&MULTI_METADATA_FLAG) && (chan_def->metadata_op_buffer == *pChanSrcBuff) )
        {
            unsigned i,num_channels = cap_data->max_sources;
            bool found_alternative = FALSE;

            for (i = 0; i < num_channels; i++)
            {
                tCbuffer *pbuffer;

                if (i == terminal_num)
                {
                    continue;
                }
                pbuffer = *chan_connection_src_buff(chan_def, GetChannelPtr(chan_def,i));;
                if ( (pbuffer!=NULL) && buff_has_metadata(pbuffer))
                {
                    chan_def->metadata_op_buffer = pbuffer;
                    found_alternative = TRUE;
                    break;
                }
            }
            if (!found_alternative)
            {
                chan_def->metadata_op_buffer = NULL;
            }
        }
#endif /* INSTALL_METADATA */

       /* Disconnecting Source */
       *pChanSrcBuff = NULL;
   }

   /* Check if channel was active and deactivate it */
   if(chan_def->active_chan_mask & term_mask)
   {
       MULTI_CHANNEL_CHANNEL_STRUC **ppchan_ptr;

       /* Remove active channel */
       chan_def->active_chan_mask &= ~term_mask;

       /* Remove channel from active list */
       for(ppchan_ptr = &chan_def->first_active;*ppchan_ptr;ppchan_ptr = &((*ppchan_ptr)->next_active) )
       {
          if( (*ppchan_ptr) == lp_chan)
          {
               *ppchan_ptr = lp_chan->next_active;
               break;
          }
       }

       /* Cleanup channel */
       lp_chan->next_active = NULL;
       if(chan_def->destroy_fn)
       {
         chan_def->destroy_fn(op_data,lp_chan);
       }
   }

   return TRUE;
}

bool multi_channel_sched_info(OPERATOR_DATA *op_data,void *message_data, unsigned *response_id, void **response_data)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    OP_SCHED_INFO_RSP* resp;

    if (chan_def==NULL)
    {
        return FALSE;
    }

    resp = base_op_get_sched_info_ex(op_data, message_data, response_id);
    if (resp == NULL)
    {
        return base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data);
    }
    *response_data = resp;

    /* Same buffer size for sink and source.
       No additional verification needed.*/
    resp->block_size = chan_def->block_size;

    return TRUE;
}

bool multi_channel_buffer_details(OPERATOR_DATA *op_data,void *message_data, unsigned *response_id, void **response_data)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   OP_BUF_DETAILS_RSP *resp;
   unsigned buffer_size,terminal_id;

   if ((chan_def==NULL) || !base_op_buffer_details(op_data, message_data, response_id, response_data))
   {
        return FALSE;
   }

   resp = (OP_BUF_DETAILS_RSP*)*response_data;

   terminal_id = OPMGR_GET_OP_BUF_DETAILS_TERMINAL_ID(message_data);

   buffer_size = chan_def->buffer_size;
   if(buffer_size == 0)
   {
        buffer_size = resp->b.buffer_size;
   }

#ifdef INSTALL_METADATA
   resp->supports_metadata = (chan_def->config&MULTI_METADATA_FLAG)?TRUE:FALSE;

   if(resp->supports_metadata)
   {
      tCbuffer *pbuffer;

      /* If an input/output connection is already present and has metadata then
      * we are obliged to return that buffer so that metadata can be shared
      * between channels. */
      if(terminal_id&TERMINAL_SINK_MASK)
      {
         pbuffer = chan_def->metadata_ip_buffer;
      }
      else
      {
         pbuffer = chan_def->metadata_op_buffer;
      }
      resp->metadata_buffer = pbuffer;
   }
   else
   {
      resp->metadata_buffer   = NULL;
   }
#endif

#ifndef DISABLE_IN_PLACE
   if(chan_def->config & MULTI_INPLACE_FLAG)
   {
      MULTI_CHANNEL_CHANNEL_STRUC *lp_chan = GetChannelPtr(chan_def,(terminal_id & ~TERMINAL_SINK_MASK) );

      if(terminal_id&TERMINAL_SINK_MASK)
      {
         resp->b.in_place_buff_params.buffer = lp_chan->source_buffer_ptr;
      }
      else
      {
         resp->b.in_place_buff_params.buffer = lp_chan->sink_buffer_ptr;
      }
      resp->runs_in_place = TRUE;
      resp->b.in_place_buff_params.size = buffer_size;

      /* Toggle sink  mask */
      resp->b.in_place_buff_params.in_place_terminal = SWAP_TERMINAL_DIRECTION(terminal_id);
   }
   else
#endif
   {
      /* return the minimum buffer size */
      resp->b.buffer_size = buffer_size;
   }
   return TRUE;
}


bool multi_channel_start(OPERATOR_DATA *op_data,void *message_data, unsigned *response_id, void **response_data)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    /* Create the response. If there aren't sufficient resources for this fail early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, response_data))
    {
        return FALSE;
    }

    /* If already running just ack */
    if (opmgr_op_is_running(op_data))
    {
       return TRUE;
    }

    /* if not hot connection, must have at least one connection */
    if( (chan_def->first_active==NULL) && ((chan_def->config&MULTI_HOT_CONN_FLAG)==0) )
    {
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

#ifdef BASE_MULTI_CHAN_OFFLOAD
    /* Clone the connection buffers for the processing code to use */
    if ((chan_def->config & MULTI_OFFLOAD_FLAG) != 0)
    {
        unsigned i, num_channels = base_op_get_cap_data(op_data)->max_sources;
        for (i = 0; i < num_channels; i++)
        {
            MULTI_CHANNEL_CHANNEL_STRUC *lp_chan = GetChannelPtr(chan_def, i);

            if (lp_chan->connection_input != NULL)
            {
                lp_chan->sink_buffer_ptr = create_clone_buffer(lp_chan->connection_input);
                /* TODO check for NULL & fail */
            }

            if (lp_chan->connection_output != NULL)
            {
                lp_chan->source_buffer_ptr = create_clone_buffer(lp_chan->connection_output);
                /* TODO check for NULL & fail */
            }
        }
    }
#endif /* BASE_MULTI_CHAN_OFFLOAD */

    return TRUE;
}

bool multi_channel_stop_reset(OPERATOR_DATA *op_data,void *message_data, unsigned *response_id, void **response_data)
{
    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, response_data))
    {
        return FALSE;
    }

    /* Mark the operator as stopped. */
    base_op_stop_operator(op_data);

#ifdef BASE_MULTI_CHAN_OFFLOAD

    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);

    if ((chan_def->config & MULTI_OFFLOAD_FLAG) != 0)
    {
        unsigned i, num_channels = base_op_get_cap_data(op_data)->max_sources;
        INT_OP_ID opid = base_op_get_int_op_id(op_data);

        /* Wait for any outstanding processing to complete */
        while (audio_thread_rpc_is_queued(opid));

        /* Synchronise connection buffers in case the operator is restarted */
        (void)multi_chan_sync_buffers(op_data);

        /* Now destroy the cloned connection buffers */
        for (i = 0; i < num_channels; i++)
        {
            MULTI_CHANNEL_CHANNEL_STRUC *lp_chan = GetChannelPtr(chan_def, i);
            cbuffer_destroy_struct(lp_chan->sink_buffer_ptr);
            lp_chan->sink_buffer_ptr = NULL;
            cbuffer_destroy_struct(lp_chan->source_buffer_ptr);
            lp_chan->sink_buffer_ptr = NULL;
        }
    }
#endif /* BASE_MULTI_CHAN_OFFLOAD */

    return TRUE;
}


#ifdef BASE_MULTI_CHAN_OFFLOAD

/*
 * When in offload mode the connection buffers are cloned
 * to avoid threading issues. These buffers need to be
 * synched after every run.
 */
unsigned multi_chan_sync_buffers(OPERATOR_DATA *op_data)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    if ((chan_def->config & MULTI_OFFLOAD_FLAG) != 0)
    {
        MULTI_CHANNEL_CHANNEL_STRUC *lp_chan = chan_def->first_active;

        unsigned ip_offset_pre_sync, ip_offset_post_sync, ip_proc_data;

        ip_offset_pre_sync = cbuffer_get_read_offset(lp_chan->connection_input);
        ip_offset_post_sync = cbuffer_get_read_offset(lp_chan->sink_buffer_ptr);

        /* total data processed on the input */
        if (ip_offset_post_sync >= ip_offset_pre_sync)
        {
            ip_proc_data = ip_offset_post_sync - ip_offset_pre_sync;
        }
        else
        {
            unsigned buffsize = cbuffer_get_size_in_words(lp_chan->sink_buffer_ptr);
            ip_proc_data = buffsize + ip_offset_post_sync - ip_offset_pre_sync;
        }

        for (lp_chan = chan_def->first_active; lp_chan != NULL; lp_chan = lp_chan->next_active)
        {
            /* first sync inputs.
             * make the input available for the lib by updating the write pointer. */
            lp_chan->sink_buffer_ptr->write_ptr = lp_chan->connection_input->write_ptr;
            /* Remove the consumed data by updating the write pointer. */
            lp_chan->connection_input->read_ptr = lp_chan->sink_buffer_ptr->read_ptr;

            /* Now sync the outputs. */
            lp_chan->source_buffer_ptr->read_ptr = lp_chan->connection_output->read_ptr;
            lp_chan->connection_output->write_ptr = lp_chan->source_buffer_ptr->write_ptr;
        }

        return ip_proc_data;
    }
    else
    {
        return 0;
    }
}

/* 
 * Offload RPC completion callback
 * The wrapper should have only scheduled processing if there was work to do
 * so we always kick the operator again from here, to update buffers etc.
 */
void multi_chan_offload_callback(void *context)
{
    OPERATOR_DATA *op_data = (OPERATOR_DATA*) context;
    opmgr_kick_operator(op_data);
}

/* 
 * Update the touched terminal status
 * This is cached from the previous run because of the offload pipelining.
 */
void multi_chan_update_touched_terminals(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    TOUCHED_TERMINALS new_touched  = *touched;
    *touched = chan_def->offload_touched;
    chan_def->offload_touched = new_touched;
}

/* 
 * Query the offload status, to save storing it twice
 */
bool multi_chan_offload_active(OPERATOR_DATA *op_data)
{
    return ((get_class_data(op_data)->config & MULTI_OFFLOAD_FLAG) != 0);
}

#endif /* BASE_MULTI_CHAN_OFFLOAD */


bool multi_channel_stream_based(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);

    /* We received client ID, length and then opmsgID and OBPM params */
    if( OPMSG_FIELD_GET(message_data, OPMSG_COMMON_MSG_SET_DATA_STREAM_BASED, IS_STREAM_BASED) )
    {
       chan_def->config |= MULTI_STREAM_FLAG;
    }
    else
    {
       chan_def->config &= ~MULTI_STREAM_FLAG;
    }

    return TRUE;
}

bool multi_channel_opmsg_set_buffer_size(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    if (opmgr_op_is_running(op_data))
    {
        L2_DBG_MSG("multi_channel_opmsg_set_buffer_size: Cannot set the buffer size for a running operator!");
        return FALSE;
    }

    if  (multi_channel_terminals_connected(chan_def->first_active, chan_def->config))
    {
        L2_DBG_MSG("multi_channel_opmsg_set_buffer_size: Cannot set the buffer size for an operator with connected terminals!");
        return FALSE;
    }

    multi_channel_set_buffer_size(op_data, OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_BUFFER_SIZE, BUFFER_SIZE));

    return TRUE;
}

int multi_channel_check_buffers_adjusted(OPERATOR_DATA *op_data,
                                        TOUCHED_TERMINALS *touched,
                                        MULTICHAN_DATA_ADJUST_FN space_func,
                                        MULTICHAN_DATA_ADJUST_FN data_func)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    MULTI_CHANNEL_CHANNEL_STRUC  *chan_ptr;
    unsigned min_available_data;
    /** Holds the minimum available space when checking the channels. */
    unsigned samples_to_process;
    unsigned block_size;
    bool     bStream_based;
    unsigned active_chan_mask;

    patch_fn_shared(base_multi_channel);

    /* If no channels active, do nothing */
    chan_ptr = chan_def->first_active;
    if(chan_ptr == NULL)
    {
        return(0);
    }

    /* Minimum to process for all active channels */
    bStream_based      = chan_def->config&MULTI_STREAM_FLAG;
    block_size         = chan_def->block_size;
    min_available_data    = UINT_MAX;
    samples_to_process    = UINT_MAX;

    do
    {
        unsigned amount,ch_amount;

        /* Check data at sink */
        amount = cbuffer_calc_amount_data_in_words(chan_ptr->sink_buffer_ptr);
        if(data_func!=NULL)
        {
            amount = data_func(chan_ptr,amount);
        }
        if(amount < min_available_data)
        {
            /* Insufficient data, no action */
            if(amount < block_size)
            {
                return (0);
            }
            min_available_data = amount;
        }

        /* Check space at source */
        ch_amount = cbuffer_calc_amount_space_in_words(chan_ptr->source_buffer_ptr);
        if(space_func!=NULL)
        {
            ch_amount = space_func(chan_ptr,ch_amount);
        }
        if(ch_amount < samples_to_process)
        {
            /* Insufficient space, no action */
            if(ch_amount < block_size)
            {
                return (0);
            }
            samples_to_process = ch_amount;
        }

        /* Only check one channel if stream based */
        if(bStream_based)
        {
            break;
        }
        /* Continue with next active channel */
        chan_ptr=chan_ptr->next_active;
    }while(chan_ptr);

    /* Update Kick flags */
    active_chan_mask = chan_def->active_chan_mask;
    touched->sources = active_chan_mask;

    if(min_available_data < samples_to_process)
    {
        /* The processed data is limited by the available data not the available space.*/
        samples_to_process = min_available_data;
    }

    /* The processing leaves less than block_size, kick backwards.
     * Note: It is possible that the minimum space and data is in different channels.
     * To avoid going through the channels once more just kick backwards anyway. */
    if( min_available_data - samples_to_process < block_size)
    {
        touched->sinks = active_chan_mask;
    }

    /* Return amount of data to process */
    return samples_to_process;
}

#ifdef INSTALL_METADATA
void multi_channel_metadata_propagate(OPERATOR_DATA *op_data,unsigned amount)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    if(chan_def->config&MULTI_METADATA_FLAG)
    {
       /* Propagate any metadata to the output. Any handling of it's presence or
        * not is handled by the metadata library */
       metadata_strict_transport(chan_def->metadata_ip_buffer,
                                   chan_def->metadata_op_buffer,
                                   amount * OCTETS_PER_SAMPLE );
    }
}


/* Function checks if the tag is end of file or void.*/
static inline bool is_eof_or_void(metadata_tag* tag)
{
    return METADATA_STREAM_END(tag) || IS_VOID_TTP_TAG(tag) || IS_EMPTY_TAG(tag);
}

/* Function checks if the tag is end of file, timestamped or time of arrival tag.*/
static inline bool is_eof_or_timestamp_or_toa(metadata_tag* tag)
{
    return METADATA_STREAM_END(tag) ||
            IS_TIME_OF_ARRIVAL_TAG(tag) ||
            IS_TIMESTAMPED_TAG(tag);
}

/**
 *  Returns the first transitional tag. Transitional tag is a tag which has different
 *  type than the previous.
 *
 *  NOTE: There are three types of tags which can cause transition:
 *      1) Timestamped and time of arrival tags are in the same group.
 *      2) Void tags.
 *      3) End of file tags.
 *
 * \param   op_extra_data  The extra operator data of the resampler.
 * \param   tag_list Pointer to the head of tag list.
 *
 * \return  The first tag which will cause a transition in the internal metadata state
 *          of the operator.
 */
static metadata_tag* get_first_transitional_tag(multi_chan_tag_types last_tag_type,
       metadata_tag* tag_list)
{
    if (last_tag_type == UNSUPPORTED_TAG)
    {
        return tag_list;
    }
    else if ((last_tag_type == TIMESTAMPED_TAG) ||
            (last_tag_type == TIME_OF_ARRIVAL_TAG))
    {
        while ((tag_list != NULL) && !is_eof_or_void(tag_list))
        {
            tag_list = tag_list->next;
        }
    }
    else if (last_tag_type == VOID_TAG)
    {
        while ((tag_list != NULL) && !is_eof_or_timestamp_or_toa(tag_list))
        {
            tag_list = tag_list->next;
        }
    }
    return tag_list;
}
/**
 *  Returns the distance (in samples) from the beginning of the available metadata for
 *  a given tag.
 *
 * \param   tag  Pointer to the tag
 * \param   metadata_buff Pointer to the metadata buffer.
 *
 * \return  Distance in samples to the specified tag.
 */
static unsigned get_samples_to_tag(metadata_tag *tag, tCbuffer  *metadata_buff)
{
    unsigned offset;
    unsigned ret_val;
    unsigned buffer_size = cbuffer_get_size_in_octets(metadata_buff);
    offset = buff_metadata_get_read_offset(metadata_buff);

    /* Calculate the samples for until the tag. */
    ret_val = tag->index - offset;
    if (ret_val > buffer_size)
    {
        ret_val = ret_val + buffer_size;
    }
    return ret_val/ OCTETS_PER_SAMPLE;
}

unsigned multi_channel_metadata_limit_consumption(OPERATOR_DATA *op_data,TOUCHED_TERMINALS *touched,unsigned consumed)
{
   MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
   tCbuffer* metadata_ip_buff;

   patch_fn_shared(base_multi_channel_metadata);

   metadata_ip_buff = chan_def->metadata_ip_buffer;

   if (metadata_ip_buff != NULL)
   {
      unsigned meta_available_samples;
      metadata_tag* tag_list;
      metadata_tag* transition_tag;
      unsigned samples_to_tag;

      /* Limit the consumed data to the available metadata. */
      meta_available_samples = buff_metadata_available_octets(metadata_ip_buff) / OCTETS_PER_SAMPLE;
      if (consumed > meta_available_samples)
      {
         MULTI_CHAN_DBG_MSG2("OP 0x%08x: fewer metadata than data, only consume = %d",
            (uintptr_t)(op_data), meta_available_samples);
         consumed = meta_available_samples;
         /* Not processing all the inputs any more. */
         touched->sinks = 0;
      }

      /* get the tag list */
      tag_list = buff_metadata_peek(metadata_ip_buff);

      transition_tag = get_first_transitional_tag(chan_def->last_tag_type,tag_list);

      if(transition_tag != NULL)
      {
         samples_to_tag = get_samples_to_tag(transition_tag, metadata_ip_buff);
         /* Avoid to consume */
         if ((samples_to_tag != 0)&&(consumed > samples_to_tag))
         {
            MULTI_CHAN_DBG_MSG2("OP 0x%08x: special tag, only consume = %d",
               (uintptr_t)(op_data), meta_available_samples);
            consumed = samples_to_tag;
            /* Not processing all the inputs any more. */
            touched->sinks = 0;
         }
      }
   }
   return consumed;
}

/**
 * Returns the last tag from the list.
 *
 * \param    tag_list Pointer to the head of tag list.
 *
 * \return   Pointer to the last tag from the list.
 */
static metadata_tag* get_last_tag(metadata_tag* tag_list)
{
    if (tag_list == NULL)
    {
        return NULL;
    }

    while (tag_list->next != NULL)
    {
        tag_list = tag_list->next;
    }
    return tag_list;
}



/**
 *  Save a given metadata tag to the last_tag field..
 *
 * \param    op_extra_data  The extra operator data of the resampler.
 * \param    tag    Tag to save in the last tag field.
 *
 * \return   Pointer to the created tag.
 */
static void save_timestamped_tag_data(multi_chan_ttp_last_tag *last_tag, metadata_tag* tag)
{
    unsigned *err_offset_id;
    unsigned length;

    last_tag->samples_after = 0;

    /* Save the timestamp info from the incoming metadata */
    last_tag->timestamp = tag->timestamp;
    last_tag->spa = tag->sp_adjust;
    if (buff_metadata_find_private_data(tag, META_PRIV_KEY_TTP_OFFSET, &length, (void **)&err_offset_id))
    {
        last_tag->err_offset_id = (*err_offset_id);
    }
    else
    {
        last_tag->err_offset_id = INFO_ID_INVALID;
    }
}

/**
 *  Function used to create the output tag. Depending on the last tag read from the
 *  input buffer the created tag can be timestamped, time of arrival or void.
 *
 *  NOTE: The next field of the created output tag is not set.
 *
 * \param    op_extra_data  The extra operator data of the resampler.
 * \param    length  Sets the length of the newly created tag.
 *
 * \return   Pointer to the created tag.
 */
static metadata_tag* create_output_tag(OPERATOR_DATA *op_data, multi_chan_ttp_last_tag *last_tag, unsigned length)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    metadata_tag* ret_tag;

    /* Bail out early if the length is 0. This is could happen if the operator only
     * consumed data. */
    if (length == 0)
    {
        return NULL;
    }

    /* Create only one tag for the outgoing data.*/
    ret_tag = buff_metadata_new_tag();
    if(ret_tag == NULL)
    {
        MULTI_CHAN_DBG_MSG1("OP 0x%08x: Metadata allocation failed.", (uintptr_t)(op_data));
        return NULL;
    }

    if (chan_def->last_tag_type == TIMESTAMPED_TAG)
    {
        unsigned new_ttp;
        ttp_status status;

        status.sp_adjustment = last_tag->spa;
        status.err_offset_id = last_tag->err_offset_id;
        status.stream_restart = (METADATA_STREAM_START(ret_tag) != 0);

        /* Calculate new TTP from incoming data and sample offset */
        new_ttp = ttp_get_next_timestamp(
                last_tag->timestamp,
                last_tag->samples_after,
                last_tag->in_rate,
                last_tag->spa);
        /* add phase correction */
        new_ttp = time_add(new_ttp, last_tag->timestamp_phase_correction);
        status.ttp = new_ttp;
        ttp_utils_populate_tag(ret_tag, &status);

        MULTI_CHAN_DBG_MSG4(
                "OP 0x%08x: TIMESTAMPED last tag timestamp = 0x%08x, "
                "samples_after = 0x%08x, input_buff_after_index = 0x%08x",
                (uintptr_t)(op_data),
                last_tag->timestamp,
                last_tag->samples_after,
                new_ttp
                );
    }
    else if (chan_def->last_tag_type == TIME_OF_ARRIVAL_TAG)
    {
        unsigned time_of_arrival;
        /* Create a time of arrival tag. */
        time_of_arrival = ttp_get_next_timestamp(
                last_tag->timestamp,
                last_tag->samples_after,
                last_tag->in_rate,
                last_tag->spa);
        /* add phase correction */
        time_of_arrival = time_add(time_of_arrival, last_tag->timestamp_phase_correction);
        ret_tag->sp_adjust = last_tag->spa;
        METADATA_TIME_OF_ARRIVAL_SET(ret_tag, time_of_arrival);

        MULTI_CHAN_DBG_MSG4(
                "OP 0x%08x: TIME_OF_ARRIVAL last tag time of arrival = 0x%08x, "
                "samples_after = 0x%08x, input_buff_after_index = 0x%08x",
                (uintptr_t)(op_data),
                last_tag->timestamp,
                last_tag->samples_after,
                time_of_arrival);
    }
    else if (chan_def->last_tag_type == VOID_TAG)
    {
        /* Just mark the tag as a void one. */
        METADATA_VOID_TTP_SET(ret_tag);
    }
    else
    {
        /* Check if the state is valid. */
        PL_ASSERT(chan_def->last_tag_type != UNSUPPORTED_TAG);
    }

    /* Set the length of the tag. */
    ret_tag->length = length;

    return ret_tag;
}


/**
 *  Returns the first tag which changes the type.
 *
 *  NOTE: There are three types of tags which can cause transition:
 *      1) Timestamped and time of arrival tags are in the same group.
 *      2) Void tags.
 *      3) End of file tags.
 *
 * \param   op_extra_data  The extra operator data of the resampler.
 * \param   tag_list Pointer to the head of tag list.
 *
 * \return  The first tag which will case a transition in the internal metadata state
 *          of the operator.
 */
static void insert_samples_to_output_channels(MULTI_CHANNEL_DEF *chan_def, unsigned samples)
{
    MULTI_CHANNEL_CHANNEL_STRUC *chan = chan_def->first_active;
    tCbuffer * output_buffer;
    unsigned buffer_size;
    int last_sample;

    if (samples == 0)
    {
        /* Nothing to do*/
        return;
    }

    MULTI_CHAN_DBG_MSG1("Muli-Chan: insert samples 0x%08x", samples);
    while(chan)
    {
        output_buffer = chan->source_buffer_ptr;
        buffer_size = cbuffer_get_size_in_words(output_buffer);

        MULTI_CHAN_DBG_MSG3("Muli-Chan channel 0x%08x: output_buffer space  = 0x%08x, output_buffer data = 0x%08x",
                  (uintptr_t)(chan), cbuffer_calc_amount_space_in_words(output_buffer), cbuffer_calc_amount_data_in_words(output_buffer));

        /*get the last sample by advanceing the read pointer by buffer size - 1 */
        cbuffer_advance_read_ptr(output_buffer, buffer_size - 1);
        cbuffer_read(output_buffer, &last_sample, 1);

        /* due to the cbuffer_read the read pointer should be in the same place. */
        if (output_buffer != NULL)
        {
            /* TODO use the last sample value! */
            cbuffer_block_fill(output_buffer, samples, last_sample);
        }

        MULTI_CHAN_DBG_MSG4("Muli-Chan channel 0x%08x: output_buffer space  = 0x%08x, output_buffer data = 0x%08x, last_sample = 0x%08x",
                  (uintptr_t)(chan), cbuffer_calc_amount_space_in_words(output_buffer), cbuffer_calc_amount_data_in_words(output_buffer), last_sample);

        chan = chan->next_active;
    }
}

/**
 *  Function used to transport metadata from the input buffer to the output buffer.
 *
 *  NOTE: Tags from the input metadata buffer will be recreated (and not transported) on
 *  the output.
 *
 * \param    op_extra_data  The extra operator data of the resampler.
 * \param    consumed  Samples consumed form the input channels.
 * \param    produced  Samples porduce on the output channels.
 */
void multi_channel_metadata_transfer(OPERATOR_DATA *op_data,unsigned consumed,unsigned produced,multi_chan_ttp_last_tag *op_last_tag)
{
    MULTI_CHANNEL_DEF *chan_def = get_class_data(op_data);
    metadata_tag* tag_list;
    tCbuffer* metadata_ip_buff;
    tCbuffer* metadata_op_buff;

    patch_fn(base_multi_channel_metadata_transfer);

    metadata_ip_buff = chan_def->metadata_ip_buffer;
    metadata_op_buff = chan_def->metadata_op_buffer;

    if ((metadata_ip_buff != NULL) && (metadata_op_buff != NULL))
    {
        /* transport the metadata. */

       /* Remove the tags for the data that has been consumed during this kick
        * update the length fields by the conversion factor as the data has
        * been resampled then append the tags indicating the number of octets
        * the operation produced.
        */
       unsigned output_buff_after_index;
       unsigned input_buff_before_index, input_buff_after_index;
       unsigned produced_octets = produced * OCTETS_PER_SAMPLE;

       metadata_tag  *out_tag, *last_tag = NULL, *eof_tag = NULL;

       tag_list = buff_metadata_remove(metadata_ip_buff, consumed * OCTETS_PER_SAMPLE,
               &input_buff_before_index, &input_buff_after_index);

       MULTI_CHAN_DBG_MSG4(
               "OP 0x%08x: tag_list = 0x%08x, input_buff_before_index = 0x%08x, input_buff_after_index = 0x%08x",
                  (uintptr_t)(op_data), (uintptr_t)(tag_list), input_buff_before_index, input_buff_after_index
                  );

       if(tag_list != NULL)
       {
           /* Check if the tag is void or eof */
           if(IS_VOID_TTP_TAG(tag_list))
           {
               if (chan_def->last_tag_type != VOID_TAG)
               {
                   MULTI_CHAN_DBG_MSG1("OP 0x%08x: Switching to void mode",
                               (uintptr_t)(op_data));
                   chan_def->last_tag_type = VOID_TAG;
               }
               out_tag = create_output_tag(op_data, op_last_tag, produced_octets);
           }
           else if(IS_EMPTY_TAG(tag_list))
           {
               if (chan_def->last_tag_type != EMPTY_TAG)
               {
                   MULTI_CHAN_DBG_MSG1("OP 0x%08x: Switching to empty mode",
                               (uintptr_t)(op_data));
                   chan_def->last_tag_type = EMPTY_TAG;
               }
               out_tag = create_output_tag(op_data, op_last_tag, produced_octets);
           }
           else if(IS_TIMESTAMPED_TAG(tag_list))
           {
               if(chan_def->last_tag_type != TIMESTAMPED_TAG)
               {
                   MULTI_CHAN_DBG_MSG1("OP 0x%08x: Switching to timestamped mode",
                              (uintptr_t)(op_data));
                   chan_def->last_tag_type = TIMESTAMPED_TAG;
                   /* First valid tag save it for the first tag creation. */
                   save_timestamped_tag_data(op_last_tag, tag_list);
               }
               out_tag = create_output_tag(op_data, op_last_tag, produced_octets);

           }
           else if(IS_TIME_OF_ARRIVAL_TAG(tag_list))
           {
               if(chan_def->last_tag_type != TIME_OF_ARRIVAL_TAG)
               {
                   MULTI_CHAN_DBG_MSG1("OP 0x%08x: Switching to time of arrival mode",
                               (uintptr_t)(op_data));
                   chan_def->last_tag_type = TIME_OF_ARRIVAL_TAG;
                   /* First valid tag save it for the first tag creation. */
                   save_timestamped_tag_data(op_last_tag, tag_list);
               }
               out_tag = create_output_tag(op_data, op_last_tag, produced_octets);
           }
           else if(METADATA_STREAM_END(tag_list))
           {
               MULTI_CHAN_DBG_MSG1("OP 0x%08x: End of file tag received. ",
                           (uintptr_t)(op_data));
               /* First create the tag based on the previous state. */
               if (produced_octets == 0)
               {
                   /* In some cases the operator only consumes data (If the produced
                    * samples is 0). In this case if an eof tag is read a sample needs
                    * generating to transport this tag.*/
                   insert_samples_to_output_channels(chan_def, 1);
                   /* To keep the eof tag 0 length an additional tag will be created. */
                   produced_octets = OCTETS_PER_SAMPLE;
               }
               out_tag = create_output_tag(op_data, op_last_tag, produced_octets);
               chan_def->last_tag_type = UNSUPPORTED_TAG;
           }
           else
           {
               MULTI_CHAN_DBG_MSG1("OP 0x%08x: Unsupported tag.",
                           (uintptr_t)(op_data));
               out_tag = NULL;
           }

           /* Save the last timestamp/time of arrival to calculate the timestamp/time of
            * arrival for the next run. If the last tag is a eof or a void tag then there
            * is no need to save it. */
           last_tag = get_last_tag(tag_list);
           if(!is_eof_or_void(tag_list))
           {
               save_timestamped_tag_data(op_last_tag, last_tag);
               /* Check if input_buff_after_index is sample aligned. */
               PL_ASSERT(input_buff_after_index%OCTETS_PER_SAMPLE == 0);
               op_last_tag->samples_after =
                       (input_buff_after_index / OCTETS_PER_SAMPLE);
           }
       }
       else
       {
           /* Create empty tags if we haven't received any real tags */
           if(chan_def->last_tag_type == UNSUPPORTED_TAG && op_last_tag->samples_after == 0)
           {
               chan_def->last_tag_type = EMPTY_TAG;               
           }

           /* Check if the state is different than INVALID.*/
           PL_ASSERT(chan_def->last_tag_type != UNSUPPORTED_TAG);

           out_tag = create_output_tag(op_data, op_last_tag, produced_octets);
           if(chan_def->last_tag_type != VOID_TAG)
           {
               op_last_tag->samples_after += consumed;
           }
       }

       /* save the end of file tag */
       if ((last_tag != NULL) && METADATA_STREAM_END(last_tag))
       {
           PL_ASSERT(last_tag->next == NULL);
           eof_tag = buff_metadata_copy_tag(last_tag);
           output_buff_after_index = 0;
       }
       else
       {
           /* Set the after index values to the produced octets. */
           output_buff_after_index = produced_octets;
       }

       /* Free all the incoming tags */
       buff_metadata_tag_list_delete(tag_list);

       /*In some cases the operator only consumes data.*/
       if (out_tag)
       {
           /* Set the next tag to null or to the end of file tag. Not eof_tag is NULL by default.*/
           out_tag->next = eof_tag;

           buff_metadata_append(metadata_op_buff, out_tag, 0, output_buff_after_index);
       }
    }
    else if ((metadata_ip_buff != NULL) && (metadata_op_buff == NULL))
    {
        unsigned b4idx, afteridx;
        /* Remove the tags from the input buffer. */
        tag_list = buff_metadata_remove(metadata_ip_buff, consumed* OCTETS_PER_SAMPLE ,
                                        &b4idx, &afteridx);
        buff_metadata_tag_list_delete(tag_list);
    }
    else if (metadata_op_buff != NULL)
    {
        if(produced > 0)
        {

            unsigned produced_octets = produced* OCTETS_PER_SAMPLE;
            /* there is no input metadata. Just create an empty tag to cover the samples. */
            tag_list = buff_metadata_new_tag();
            if (tag_list != NULL)
            {
                tag_list->length = produced_octets;
            }
            buff_metadata_append(metadata_op_buff, tag_list, 0, produced_octets);
        }
    }
}
#endif
