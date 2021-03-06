/****************************************************************************
 * Copyright (c) 2016 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup buffer Buffer Subsystem
 * \file  buffer_metadata_kip.h
 *
 * \ingroup buffer
 *
 * Buffer metadata private functions related to dual core
 *
 */


typedef struct KIP_METADATA_BUFFER
{
    /**
     * It's important that "parent" remains the _first_ field in this structure
     * as it's typecasted to tCbuffer when necessary.
     */
    tCbuffer parent;

    unsigned prev_rd_index;
    unsigned prev_wr_index;
    bool     kip_sync;
} KIP_METADATA_BUFFER;


/*************************************************************************************
  Function definitions for metadata cross cores
*/

/**
 * \brief   Pushes one tag from local memory to the shared-memory
 * \param   cbuffer Pointer to the shared cbuffer.
 * \param   tag Pointer to the tag to be copied.
 * \return  TRUE if it the passed tag is valid and there's space in the buffer
 *          for the copy and FALSE otherwise.
 */
extern bool buff_metadata_kip_tag_to_buff(tCbuffer *buffer, metadata_tag *tag);

/**
 * \brief   Pops one tag from the shared-memory to local memory.
 * \param   cbuffer Pointer to the shared-memory cbuffer structure.
 * \return  Returns a single tag and updates the buffer details accordingly.
 */
extern metadata_tag* buff_metadata_kip_tag_from_buff(tCbuffer *cbuffer);


