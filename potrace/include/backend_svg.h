/* Copyright (C) 2001-2015 Peter Selinger.
   This file is part of Potrace. It is free software and it is covered
   by the GNU General Public License. See the file COPYING for details. */

#ifndef BACKEND_SVG_H
#define BACKEND_SVG_H

#include "potrace.h"
#include "curve.h"
#include "lists.h"
#include "auxiliary.h"

#include "4DPluginAPI.h"

int page_svg(void *pResult, potrace_path_t *plist, imginfo_t *imginfo, info_t *info);

#endif /* BACKEND_SVG_H */