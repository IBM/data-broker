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

#ifndef BACKEND_TRANSPORTS_MEMCOPY_H_
#define BACKEND_TRANSPORTS_MEMCOPY_H_

#include <stddef.h>

#include "../common/dbbe_api.h"
#include "../common/data_transport.h"

extern dbBE_Data_transport_t dbBE_Memcopy_transport;

int64_t dbBE_Transport_memory_gather( dbBE_Data_transport_device_t* destbuf,
                                      size_t len,
                                      int sge_count,
                                      dbBE_sge_t *sge );

int64_t dbBE_Transport_memory_scatter( dbBE_Data_transport_device_t* srcbuf,
                                       size_t len,
                                       int sge_count,
                                       dbBE_sge_t *sge);

#endif /* BACKEND_TRANSPORTS_MEMCOPY_H_ */
