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


#ifndef BACKEND_NETWORK_DEFINITIONS_H_
#define BACKEND_NETWORK_DEFINITIONS_H_

#define DBR_SERVER_HOST_ENV "DBR_SERVER"
#define DBR_SERVER_AUTHFILE_ENV "DBR_AUTHFILE"
#define DBR_SERVER_DEFAULT_HOST "sock://localhost:6379"
#define DBR_SERVER_DEFAULT_AUTHFILE ".databroker.auth"


#define DBBE_URL_MAX_LENGTH ( 1024 )


#define DBBE_RECONNECT_TIMEOUT ( 5 )


#endif /* BACKEND_NETWORK_DEFINITIONS_H_ */
