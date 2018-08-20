/*
 * Copyright © 2018 IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef BACKEND_REDIS_SR_BUFFER_H_
#define BACKEND_REDIS_SR_BUFFER_H_

/*
 * Send/Recv buffer for interaction with Redis
 *
 * intended usage
 *
 * Recv: receive data on a reset() or empty() buffer and update via set_fill()
 * then get_positition() for parsing and advance() to update processed position until empty()
 *
 * Send: get_positition() to add_data() to fill the buffer for sending
 * then send from get_start() of available() data
 */


typedef struct
{
  size_t _size;
  size_t _available;
  size_t _processed;
  char *_start;
} dbBE_Redis_sr_buffer_t;


/*
 * allocate an initialize the send-receive buffer
 */
dbBE_Redis_sr_buffer_t* dbBE_Redis_sr_buffer_allocate( const size_t size );

/*
 * reset the stats of the send-receive buffer
 */
void dbBE_Redis_sr_buffer_reset( dbBE_Redis_sr_buffer_t *sr_buf );

/*
 * deallocate and reset memory of send-receive buffer
 */
void dbBE_Redis_sr_buffer_free( dbBE_Redis_sr_buffer_t *sr_buf );



/*******************************************************************
 * Macros to unify access to the buffer
 * note that these don't check for NULL pointer access
 * because it would cause a lot of unnecessary checks
 */

/*
 * retrieve the size of the send-receive buffer
 */
#define dbBE_Redis_sr_buffer_get_size( sr_buf ) ( (sr_buf)->_size )

/*
 * retrieve the buffer's starting position
 */
#define dbBE_Redis_sr_buffer_get_start( sr_buf ) ( (sr_buf)->_start )

/*
 * retrieve the current read/write position of the buffer
 */
#define dbBE_Redis_sr_buffer_get_processed_position( sr_buf ) ( (sr_buf)->_start + (sr_buf)->_processed)

/*
 * retrieve the position of the last valid data in the buffer
 */
#define dbBE_Redis_sr_buffer_get_available_position( sr_buf ) ( (sr_buf)->_start + (sr_buf)->_available)

/*
 * check if the buffer contains any remaining unprocessed data
 */
#define dbBE_Redis_sr_buffer_empty( sr_buf ) (( (sr_buf)->_available == 0 ) || ( (sr_buf)->_available - (sr_buf)->_processed <= 0 ))

/*
 * check if the buffer has remaining space for a given number of bytes
 */
#define dbBE_Redis_sr_buffer_full( sr_buf, bytes ) ( ((int64_t)(sr_buf)->_size < (int64_t)(bytes) ) \
                                                 || ( (int64_t)((sr_buf)->_size - (sr_buf)->_available) < (int64_t)(bytes) ))

/*
 * return the number of remaining unused bytes at the end of the buffer
 */
#define dbBE_Redis_sr_buffer_remaining( sr_buf ) ( (sr_buf)->_size - (sr_buf)->_available )

/*
 * return the number of available bytes in the buffer
 */
#define dbBE_Redis_sr_buffer_available( sr_buf ) ( (sr_buf)->_available )

/*
 * return the number of processed bytes in the buffer
 */
#define dbBE_Redis_sr_buffer_processed( sr_buf ) ( (sr_buf)->_processed )

/*
 * return the number of unprocessed bytes in the buffer
 */
#define dbBE_Redis_sr_buffer_unprocessed( sr_buf ) ( (sr_buf)->_available - (sr_buf)->_processed )

/*
 * set the fill status to the given size
 * it will not fill beyond the size of the buffer
 * returns the actual number of bytes filled
 */
static inline size_t dbBE_Redis_sr_buffer_set_fill( dbBE_Redis_sr_buffer_t *sr_buf,
                                                    const size_t fill )
{
  size_t ret = fill;
  if( sr_buf->_size >= fill )
    sr_buf->_available = fill;
  else
  {
    ret = sr_buf->_size - sr_buf->_available;
    sr_buf->_available = sr_buf->_size;
  }
  return ret;
}

/*
 * adds to the available data position
 * it will not move farther than the size
 * returns the actual number of bytes added
 * for write buffers, it also updates the progressed position
 */
static inline size_t dbBE_Redis_sr_buffer_add_data( dbBE_Redis_sr_buffer_t *sr_buf,
                                                    const size_t n, const int write )
{
  size_t ret = n;
  if( sr_buf->_available + n <= sr_buf->_size )
    sr_buf->_available += n;
  else
  {
    ret = sr_buf->_size - sr_buf->_available;
    sr_buf->_available = sr_buf->_size;
  }
  if( write != 0 )
    sr_buf->_processed = sr_buf->_available;
  return ret;
}


/*
 * add n to the number of processed bytes
 * it will not progress farther than the available amount
 * returns the actual number of bytes progressed/advanced
 */
static inline size_t dbBE_Redis_sr_buffer_advance( dbBE_Redis_sr_buffer_t *sr_buf,
                                                   const size_t n )
{
  size_t ret = n;
  if( sr_buf->_processed + n <= sr_buf->_available )
    sr_buf->_processed += n;
  else
  {
    ret = sr_buf->_available - sr_buf->_processed;
    sr_buf->_processed = sr_buf->_available;
  }
  return ret;
}

/*
 * rewind a given number of bytes to reset after add-data errors
 * returns the actual number of rewinded bytes
 * will adjust processed position if available drops below current processed position
 */
static inline size_t dbBE_Redis_sr_buffer_rewind_available_by( dbBE_Redis_sr_buffer_t *sr_buf,
                                                               const size_t n )
{
  size_t ret = n;
  if( sr_buf->_available > n )
    sr_buf->_available -= n;
  else
  {
    ret = sr_buf->_available;
    sr_buf->_available = 0;
  }
  if( sr_buf->_available < sr_buf->_processed )
    sr_buf->_processed = sr_buf->_available;
  return ret;
}

/*
 * rewind to a given position to reset after add-data errors
 * returns the actual position in case it was out of bounds
 * will adjust the processed position if needed
 */
static inline char* dbBE_Redis_sr_buffer_rewind_available_to( dbBE_Redis_sr_buffer_t *sr_buf,
                                                              char *p )
{
  char *ret = p;
  if(( p >= sr_buf->_start ) && ( p < sr_buf->_start + sr_buf->_size ))
  {
    sr_buf->_available = (size_t)(p - sr_buf->_start);
    if( sr_buf->_available < sr_buf->_processed )
      sr_buf->_processed = sr_buf->_available;
  }
  else
  {
    dbBE_Redis_sr_buffer_reset( sr_buf );
    ret = sr_buf->_start;
  }
  return ret;
}

/*
 * rewind to a given position to reset after data processing errors
 * returns the actual position in case it was out of bounds
 * will adjust the processed position if needed
 */
static inline char* dbBE_Redis_sr_buffer_rewind_processed_to( dbBE_Redis_sr_buffer_t *sr_buf,
                                                              char *p )
{
  char *ret = p;
  if(( p >= sr_buf->_start ) && ( p < sr_buf->_start + sr_buf->_size ))
  {
    size_t offset = (size_t)(p - sr_buf->_start);
    if( offset < sr_buf->_available )
      sr_buf->_processed = offset;
    else
      ret = NULL;
  }
  else
  {
    ret = NULL;
  }
  return ret;
}



#endif /* BACKEND_REDIS_SR_BUFFER_H_ */
