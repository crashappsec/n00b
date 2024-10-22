/*
 * Copyright © 2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           hatrack.h
 *  Description:    Single header file to pull in all functionality.
 *
 *  Author:         John Viega, john@zork.org
 */

#include "hatrack/base.h"
#include "hatrack/hatomic.h"

// Our dictionary algorithms
#include "hatrack/crown.h"
#include "hatrack/woolhat.h"
#include "hatrack/refhat.h" // single threaded hash.

// Dict algorithms that should only be used for reference.
#ifdef HATRACK_REFERENCE_ALGORITHMS
#include "hatrack/llstack.h"
#include "hatrack/witchhat.h"
#include "hatrack/hihat.h"
#include "hatrack/oldhat.h"
#include "hatrack/tiara.h"
#include "hatrack/ballcap.h"
#include "hatrack/newshat.h"
#include "hatrack/swimcap.h"
#include "hatrack/duncecap.h"
#include "hatrack/lohat-a.h"
#include "hatrack/lohat.h"
#include "hatrack/tophat.h"
#endif

#include "hatrack/dict.h"
#include "hatrack/set.h"
#include "hatrack/flexarray.h"
#include "hatrack/hash.h"
#include "hatrack/queue.h"
#include "hatrack/stack.h"
#include "hatrack/hatring.h"
#include "hatrack/logring.h"
#include "hatrack/zeroarray.h"

// These aren't fully finished.
#ifdef HATRACK_UNFINISHED_ALGORITHMS
#include "hatrack/q64.h"
#include "hatrack/hq.h"
#include "hatrack/capq.h"
#include "hatrack/vector.h"
#include "hatrack/helpmanager.h"
#endif
