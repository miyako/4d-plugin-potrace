/* Copyright (C) 2001-2015 Peter Selinger.
   This file is part of Potrace. It is free software and it is covered
   by the GNU General Public License. See the file COPYING for details. */

#ifndef BACKEND_PDF_H
#define BACKEND_PDF_H

#ifdef _WINDOWS
#include <math.h>
#include <intrin.h>
#include <iterator>
#endif

#include "potrace.h"
#include "curve.h"
#include "lists.h"
#include "auxiliary.h"

#include "4DPluginAPI.h"

int init_pdf(std::vector<unsigned char> &fout, info_t *info);
int term_pdf(std::vector<unsigned char> &fout, info_t *info);
int page_pdf(void *pResult, potrace_path_t *plist, imginfo_t *imginfo, info_t *info);

#endif /* BACKEND_PDF_H */