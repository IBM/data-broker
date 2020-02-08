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

#ifndef BACKEND_COMMON_UTILITY_H_
#define BACKEND_COMMON_UTILITY_H_

#include <stdlib.h>
#include <string.h>

#include "logutil.h"

/*
 * retrieve an environment variable
 * returns the string value of env_var or the default if it's not set
 */
static inline
char* dbBE_Extract_env( const char *env_var, const char *env_default )
{
  char *env = NULL;
  char *envstr = getenv(env_var);
  if( envstr == NULL )
  {
    env = strdup(env_default);
    LOG( DBG_VERBOSE, stdout, "%s environment variable not set/found. Using default: %s\n", env_var, env );
  }
  else
    env = strdup( envstr );

  return env;
}




#endif /* BACKEND_COMMON_UTILITY_H_ */
