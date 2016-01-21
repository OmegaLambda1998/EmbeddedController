/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STDDEF_H__
#define __CROS_EC_STDDEF_H__

#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ unsigned long
#endif

typedef __SIZE_TYPE__ size_t;
/* There is a GCC macro for a size_t type, but not for a ssize_t type.
 * The following construct convinces GCC to make __SIZE_TYPE__ signed.
 */
#define unsigned signed
typedef __SIZE_TYPE__ ssize_t;
#undef unsigned

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef __WCHAR_TYPE__
#define __WCHAR_TYPE__ int
#endif
typedef __WCHAR_TYPE__ wchar_t;

#endif /* __CROS_EC_STDDEF_H__ */
