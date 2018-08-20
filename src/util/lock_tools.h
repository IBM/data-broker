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

#ifndef SRC_UTIL_LOCK_TOOLS_H_
#define SRC_UTIL_LOCK_TOOLS_H_


#define BIGLOCK_LOCK( ctx ) pthread_mutex_lock( &(ctx)->_biglock )

#define BIGLOCK_UNLOCK( ctx ) pthread_mutex_unlock( &(ctx)->_biglock )

#define BIGLOCK_UNLOCKRETURN( ctx, rc ) { BIGLOCK_UNLOCK( ctx ); return (rc); }


#endif /* SRC_UTIL_LOCK_TOOLS_H_ */
