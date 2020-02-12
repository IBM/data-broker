/*
 * Copyright © 2020 IBM Corporation
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

#include <libdatabroker.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> // read

#define FASTQ_TEST_BUFFER_SIZE ( 2ull * 1024ull * 1024ull * 1024ull )

int main( int argc, char **argv )
{
  int rc = 0;

  if( argc < 2 )
  {
    fprintf( stderr, "Usage: %s <FASTQ_input_file>\n", argv[0] );
    return 1;
  }
  char *fname = argv[1];

  int input = open( fname, O_RDONLY );
  if( input < 0 )
  {
    perror("Error opening input file");
    return 1;
  }

  char *fbase = strrchr( fname, '/' );
  if( fbase == NULL ) fbase = fname; else fbase++;
  DBR_Name_t name = strdup( fbase );
  DBR_Tuple_persist_level_t level = DBR_PERST_VOLATILE_SIMPLE;
  DBR_GroupList_t groups = 0;

  DBR_Handle_t cs_hdl = NULL;
  DBR_Errorcode_t ret = DBR_SUCCESS;

  // create a test name space and check
  cs_hdl = dbrCreate ( name, level, groups);
  if( cs_hdl == NULL )
    {
      cs_hdl = dbrAttach( name );
    }

  char *fqbuf = (char*)malloc( FASTQ_TEST_BUFFER_SIZE );
  if( read( input, fqbuf, FASTQ_TEST_BUFFER_SIZE ) < 0 )
  {
    perror("Reading file");
    close( input );
    free( fqbuf );
    return 1;
  }

  dbrPut( cs_hdl, fqbuf, strlen( fqbuf ), fname, 0 );

  dbrDetach( cs_hdl );
  return rc;


  // >>>>>> THIS SECTION IS TO SIMULATE THE CHANGED 'FILE NAME' >>>>>>>>>
  //  change of file name could be avoided by auto-creating a symlink
  int64_t outlen = strlen( fqbuf );
  char *fqout = (char*)calloc( 1, FASTQ_TEST_BUFFER_SIZE );

  // need to figure out the number of keys...
  char *match = (char*)calloc( 1, 255 );
  snprintf( match, 255, ".%s*", fbase );
//  if( dbrDirectory( cs_hdl, match, DBR_GROUP_EMPTY, 1000, fqout, FASTQ_TEST_BUFFER_SIZE, &outlen ) != DBR_SUCCESS )
//    rc = ENOENT;

  fprintf( stdout, "%s\n", fqout );

  int records = 0;
  if( rc == 0 )
  {
    char *tmp = fqout;
    while( strtok_r( tmp, "\n", &tmp ) != NULL ) ++records;
    if( records == 0 )
      rc = ENODATA;
  }

  fprintf( stdout, "#Records: %d\n", records );

  outlen = strlen( fqbuf ); // reset the output len; it was mis-used for directory
  char *oname = (char*)calloc( 1, 255 );
  snprintf( oname, 255, "%s_%"PRId64"_%d-%d.fastq", fname, outlen, 0, records - 1 ); // -1 because it's a range, not a count
  // <<<<<<< THIS SECTION IS TO SIMULATE THE CHANGED 'FILE NAME' <<<<<<<<<

  if( dbrRead( cs_hdl, fqout, &outlen, fname, "", DBR_GROUP_EMPTY, DBR_FLAGS_NOWAIT ) != DBR_SUCCESS )
    fprintf( stderr, "No entry: %s\n", fname );


  if( rc == 0 )
  {
    dbrGet( cs_hdl, fqout, &outlen, oname, "", DBR_GROUP_EMPTY, DBR_FLAGS_NONE );

    if( strncmp( fqbuf, fqout, FASTQ_TEST_BUFFER_SIZE ) != 0 )
      rc = EBADMSG;
  }

  // detach, not delete
  ret = dbrDelete( name );

  close( input );
  free( fqout );
  free( fqbuf );
  free( name );

  fprintf( stdout, "Exiting with rc=%d\n", rc );
  return rc;
}
