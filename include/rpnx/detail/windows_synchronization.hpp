// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_DETAIL_WINDOWS_SYNCHRONIZATION_HPP
#define RPNX_DETAIL_WINDOWS_SYNCHRONIZATION_HPP

#ifndef NOMINMAX
#define NOMINMAX
#define RPNX_RESTORE_SYNCHRONIZATION_NOMINMAX
#endif
#include <windows.h>
#ifdef RPNX_RESTORE_SYNCHRONIZATION_NOMINMAX
#undef NOMINMAX
#undef RPNX_RESTORE_SYNCHRONIZATION_NOMINMAX
#endif

#endif
