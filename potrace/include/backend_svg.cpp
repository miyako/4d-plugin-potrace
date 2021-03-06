/* Copyright (C) 2001-2015 Peter Selinger.
   This file is part of Potrace. It is free software and it is covered
   by the GNU General Public License. See the file COPYING for details. */


/* The SVG backend of Potrace. */

#include "backend_svg.h"

/* ---------------------------------------------------------------------- */
/* path-drawing auxiliary functions */

/* coordinate quantization */
static inline point_t unit(dpoint_t p, info_t *info)
{
  point_t q;
  q.x = (long)(floor(p.x*info->unit+.5));
  q.y = (long)(floor(p.y*info->unit+.5));
  return q;
}

static point_t cur;
static char lastop = 0;
static int column = 0;
static int newline = 1;

static void shiptoken(std::string &fout, char *token)
{
  int c = strlen(token);
  if (!newline && column+c+1 > 75)
	{
		fout += "\n";
    column = 0;
    newline = 1;
  } else if (!newline)
	{
		fout += " ";
    column++;
  }
	fout += token;
  column += c;
  newline = 0;
}

static void ship(std::string &fout, char *fmt, ...)
{
  va_list args;
  static char buf[4096]; /* static string limit is okay here because
			    we only use constant format strings - for
			    the same reason, it is okay to use
			    vsprintf instead of vsnprintf below. */
  char *p, *q;

  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  buf[4095] = 0;
  va_end(args);

  p = buf;
  while ((q = strchr(p, ' ')) != NULL)
	{
    *q = 0;
    shiptoken(fout, p);
    p = q+1;
		PA_YieldAbsolute();
  }
  shiptoken(fout, p);
}

static void svg_moveto(std::string &fout, dpoint_t p, info_t *info)
{
  cur = unit(p, info);

  ship(fout, (char *)"M%ld %ld", cur.x, cur.y);
  lastop = 'M';
}

static void svg_rmoveto(std::string &fout, dpoint_t p, info_t *info)
{
  point_t q;

  q = unit(p, info);
  ship(fout, (char *)"m%ld %ld", q.x-cur.x, q.y-cur.y);
  cur = q;
  lastop = 'm';
}

static void svg_lineto(std::string &fout, dpoint_t p, info_t *info)
{
  point_t q;

  q = unit(p, info);

  if (lastop != 'l')
	{
    ship(fout, (char *)"l%ld %ld", q.x-cur.x, q.y-cur.y);
  } else {
    ship(fout, (char *)"%ld %ld", q.x-cur.x, q.y-cur.y);
  }
  cur = q;
  lastop = 'l';
}

static void svg_curveto(std::string &fout, dpoint_t p1, dpoint_t p2, dpoint_t p3, info_t *info)
{
  point_t q1, q2, q3;

  q1 = unit(p1, info);
  q2 = unit(p2, info);
  q3 = unit(p3, info);

  if (lastop != 'c')
	{
    ship(fout, (char *)"c%ld %ld %ld %ld %ld %ld", q1.x-cur.x, q1.y-cur.y, q2.x-cur.x, q2.y-cur.y, q3.x-cur.x, q3.y-cur.y);
  } else {
    ship(fout, (char *)"%ld %ld %ld %ld %ld %ld", q1.x-cur.x, q1.y-cur.y, q2.x-cur.x, q2.y-cur.y, q3.x-cur.x, q3.y-cur.y);
  }
  cur = q3;
  lastop = 'c';
}

/* ---------------------------------------------------------------------- */
/* functions for converting a path to an SVG path element */

/* Explicit encoding. If abs is set, move to first coordinate
   absolutely. */
static int svg_path(std::string &fout, potrace_curve_t *curve, int abs, info_t *info)
{
  int i;
  dpoint_t *c;
  int m = curve->n;

  c = curve->c[m-1];
  if (abs)
	{
    svg_moveto(fout, c[2], info);
  } else {
    svg_rmoveto(fout, c[2], info);
  }

  for (i=0; i<m; i++)
	{
    c = curve->c[i];
    switch (curve->tag[i])
		{
    case POTRACE_CORNER:
      svg_lineto(fout, c[1], info);
      svg_lineto(fout, c[2], info);
      break;
    case POTRACE_CURVETO:
      svg_curveto(fout, c[0], c[1], c[2], info);
      break;
    }
  }
  newline = 1;
  shiptoken(fout, (char *)"z");
  return 0;
}

/* produce a jaggy path - for debugging. If abs is set, move to first
   coordinate absolutely. If abs is not set, move to first coordinate
   relatively, and traverse path in the opposite direction. */
static int svg_jaggy_path(std::string &fout, point_t *pt, int n, int abs, info_t *info)
{
  int i;
  point_t cur, prev;
  
  if (abs)
	{
    cur = prev = pt[n-1];
    svg_moveto(fout, dpoint(cur), info);
    for (i=0; i<n; i++)
		{
      if (pt[i].x != cur.x && pt[i].y != cur.y)
			{
				cur = prev;
				svg_lineto(fout, dpoint(cur), info);
      }
      prev = pt[i];
    }
    svg_lineto(fout, dpoint(pt[n-1]), info);
  } else
	{
    cur = prev = pt[0];
    svg_rmoveto(fout, dpoint(cur), info);
    for (i=n-1; i>=0; i--)
		{
      if (pt[i].x != cur.x && pt[i].y != cur.y)
			{
				cur = prev;
				svg_lineto(fout, dpoint(cur), info);
      }
      prev = pt[i];
    }
    svg_lineto(fout, dpoint(pt[0]), info);
  }
  newline = 1;
  shiptoken(fout, (char *)"z");
  return 0;
}

static void write_paths_opaque(std::string &fout, potrace_path_t *tree, info_t *info)
{
  potrace_path_t *p, *q;

  for (p=tree; p; p=p->sibling)
	{
    if (info->grouping == 2)
		{
			fout += "<g>\n";
			fout += "<g>\n";
    }
		
		char _color[7];
		sprintf(_color, "%06x", info->color);
		fout += "<path fill=\"#";
		fout += _color;
		fout += "\" stroke=\"none\" d=\"";
		
    column = strlen("<path fill=\"#______\" stroke=\"none\" d=\"");
    newline = 1;
    lastop = 0;
    if (info->debug == 1)
		{
      svg_jaggy_path(fout, p->priv->pt, p->priv->len, 1, info);
    } else {
      svg_path(fout, &p->curve, 1, info);
    }
		fout += "\"/>\n";
    for (q=p->childlist; q; q=q->sibling)
		{
			char _fillcolor[7];
			sprintf(_fillcolor, "%06x", info->fillcolor);
			fout += "<path fill=\"#";
			fout += _fillcolor;
			fout += "\" stroke=\"none\" d=\"";
      column = strlen("<path fill=\"#______\" stroke=\"none\" d=\"");
      newline = 1;
      lastop = 0;
      if (info->debug == 1)
			{
				svg_jaggy_path(fout, q->priv->pt, q->priv->len, 1, info);
      } else
			{
				svg_path(fout, &q->curve, 1, info);
      }
			fout += "\"/>\n";
    }
    if (info->grouping == 2)
		{
			fout += "</g>\n";
    }
    for (q=p->childlist; q; q=q->sibling)
		{
      write_paths_opaque(fout, q->childlist, info);
    }
    if (info->grouping == 2)
		{
			fout += "</g>\n";
    }
  }
}

static void write_paths_transparent_rec(std::string &fout, potrace_path_t *tree, info_t *info)
{
  potrace_path_t *p, *q;

  for (p=tree; p; p=p->sibling)
	{
    if (info->grouping == 2)
		{
			fout += "<g>\n";
    }
    if (info->grouping != 0)
		{
			fout += "<path d=\"";
      column = strlen("<path d=\"");
      newline = 1;
      lastop = 0;
    }
    if (info->debug == 1)
		{
      svg_jaggy_path(fout, p->priv->pt, p->priv->len, 1, info);
    } else
		{
      svg_path(fout, &p->curve, 1, info);
    }
    for (q=p->childlist; q; q=q->sibling)
		{
      if (info->debug == 1)
			{
				svg_jaggy_path(fout, q->priv->pt, q->priv->len, 0, info);
      } else
			{
				svg_path(fout, &q->curve, 0, info);
      }
    }
    if (info->grouping != 0)
		{
			fout += "\"/>\n";
    }
    for (q=p->childlist; q; q=q->sibling)
		{
      write_paths_transparent_rec(fout, q->childlist, info);
    }
    if (info->grouping == 2)
		{
			fout += "</g>\n";
    }
  }
}

static void write_paths_transparent(std::string &fout, potrace_path_t *tree, info_t *info)
{
  if (info->grouping == 0)
	{
		fout += "<path d=\"";
    column = strlen("<path d=\"");
    newline = 1;
    lastop = 0;
  }
  write_paths_transparent_rec(fout, tree, info);
  if (info->grouping == 0)
	{
		fout += "\"/>\n";
  }
}

/* ---------------------------------------------------------------------- */
/* Backend. */

/* public interface for SVG */
int page_svg(void *pResult, potrace_path_t *plist, imginfo_t *imginfo, info_t *info){

  double bboxx = imginfo->trans.bb[0]+imginfo->lmar+imginfo->rmar;
  double bboxy = imginfo->trans.bb[1]+imginfo->tmar+imginfo->bmar;
  double origx = imginfo->trans.orig[0] + imginfo->lmar;
  double origy = bboxy - imginfo->trans.orig[1] - imginfo->bmar;
  double scalex = imginfo->trans.scalex / info->unit;
  double scaley = -imginfo->trans.scaley / info->unit;

	std::string fout;
	
  /* header */
//	fout += "<?xml version=\"1.0\" standalone=\"no\"?>\n";
//  fout += "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 20010904//EN\"\n";
//  fout += " \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n";

	char _bboxx[0xFF];
	sprintf(_bboxx, "%f", bboxx);
	char _bboxy[0xFF];
	sprintf(_bboxy, "%f", bboxy);
	
	fout += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
	fout += "<svg width=\"100%\" height=\"100%\" viewBox=\"0 0 ";
	fout += _bboxx;
	fout += " ";
	fout += _bboxy;
	fout += "\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:ns4d=\"http://www.4d.com\" preserveAspectRatio=\"xMidYMid\">\n";

  /* metadata: creator */
  fout += "<metadata>\n";
  fout += "Created by potrace 1.13, written by Peter Selinger 2001-2015\n";
  fout += "</metadata>\n";

  /* use a "group" tag to establish coordinate system and style */
	fout += "<g transform=\"";
  if (origx != 0 || origy != 0)
	{
		char _origx[0xFF];
		sprintf(_origx, "%f", origx);
		char _origy[0xFF];
		sprintf(_origy, "%f", origy);
    fout += "translate(";
		fout += _origx;
		fout += ",";
		fout += _origy;
		fout += ") ";
  }
  if (info->angle != 0)
	{
		char _angle[0xFF];
		sprintf(_angle, "%f", -info->angle);
		fout += "rotate(";
		fout += _angle;
		fout += ") ";
	}
	char _scalex[0xFF];
	sprintf(_scalex, "%f", scalex);
	char _scaley[0xFF];
	sprintf(_scaley, "%f", scaley);
	fout += "scale(";
	fout += _scalex;
	fout += ",";
	fout += _scaley;
	fout += ")";
	fout += "\"\n";
	char _color[7];
	sprintf(_color, "%06x", info->color);
	fout += " fill=\"#";
	fout += _color;
	fout += "\" stroke=\"none\">\n";

  if (info->opaque)
	{
    write_paths_opaque(fout, plist, info);
  } else {
    write_paths_transparent(fout, plist, info);
  }

  /* write footer */
  fout += "</g>\n";
  fout += "</svg>\n";
	
	PA_Picture picture = PA_CreatePicture((void *)fout.c_str(), fout.size());
	*(PA_Picture*) pResult = picture;
		
	return 0;
}
