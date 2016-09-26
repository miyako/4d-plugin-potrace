/* --------------------------------------------------------------------------------
 #
 #	4DPlugin.cpp
 #	source generated by 4D Plugin Wizard
 #	Project : Potrace
 #	author : miyako
 #	2016/09/23
 #
 # --------------------------------------------------------------------------------*/

#include "4DPluginAPI.h"
#include "4DPlugin.h"

void PluginMain(PA_long32 selector, PA_PluginParameters params)
{
	try
	{
		PA_long32 pProcNum = selector;
		sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
		PackagePtr pParams = (PackagePtr)params->fParameters;

		CommandDispatcher(pProcNum, pResult, pParams); 
	}
	catch(...)
	{

	}
}

void CommandDispatcher (PA_long32 pProcNum, sLONG_PTR *pResult, PackagePtr pParams)
{
	switch(pProcNum)
	{
// --- Potrace

		case 1 :
			Potrace(pResult, pParams);
			break;

	}
}

#pragma mark -

// ------------------------------------ Potrace -----------------------------------

#define ycorr(y) (bmpinfo.topdown ? bmpinfo.h-1-y : y)
#define TRY(x) if (x) goto try_error
#define TRY_EOF(x) if (x) goto eof
#define INTBITS (8*sizeof(int))
#define COLTABLE(c) ((c) < bmpinfo.ncolors ? coltable[(c)] : 0)

#pragma mark -

void bmp_pad_reset(int *count)
{
	*count = 0;
}

int bmp_forward(std::vector<unsigned char> buf, int *pos, int *count, int newPos)
{
	if(newPos > buf.size()) return 1;
	
	unsigned int distance = (newPos - (*pos));

	*pos += distance;
	*count += distance;

	return 0;
}

int bmp_pad(std::vector<unsigned char> &buf, int *pos, int *count)
{
	unsigned int start = *pos;
	int bmp_count = *count;
	int len = (-bmp_count) & 3;

	if((start + len) > buf.size()) return 1;

	*pos += len;
	*count = 0;

	return 0;
}

int bmp_readint(std::vector<unsigned char> &buf, int *pos, int *count, int len, unsigned int *p)
{
	unsigned int start = *pos;
	unsigned int sum = 0;
	
	if((start + len) > buf.size()) return 1;
	
	for (unsigned int i = 0; i < len; ++i)
	{
		unsigned char byte = buf.at(start + i);
		sum += byte << (8 * i);
	}

	*pos += len;
	*count += len;
	*p = sum;
	
	return 0;
}

static inline double double_of_dim(dim_t d, double def)
{
	if (d.d) {
		return d.x * d.d;
	} else {
		return d.x * def;
	}
}

/* ---------------------------------------------------------------------- */
/* calculations with bitmap dimensions, positioning etc */

/* determine the dimensions of the output based on command line and
 image dimensions, and optionally, based on the actual image outline. */
static void calc_dimensions(imginfo_t *imginfo, potrace_path_t *plist, info_s *info)
{
	double dim_def;
	double maxwidth, maxheight, sc;
	int default_scaling = 0;
	
	/* we take care of a special case: if one of the image dimensions is
	 0, we change it to 1. Such an image is empty anyway, so there
	 will be 0 paths in it. Changing the dimensions avoids division by
	 0 error in calculating scaling factors, bounding boxes and
	 such. This doesn't quite do the right thing in all cases, but it
	 is better than causing overflow errors or "nan" output in
	 backends.  Human users don't tend to process images of size 0
	 anyway; they might occur in some pipelines. */
	if (imginfo->pixwidth == 0)
	{
		imginfo->pixwidth = 1;
	}
	if (imginfo->pixheight == 0)
	{
		imginfo->pixheight = 1;
	}
	
	/* set the default dimension for width, height, margins */
	if (info->backend->pixel)
	{
		dim_def = DIM_PT;
	} else {
		dim_def = DEFAULT_DIM;
	}
	
	/* apply default dimension to width, height, margins */
	imginfo->width = info->width_d.x == UNDEF ? UNDEF : double_of_dim(info->width_d, dim_def);
	imginfo->height = info->height_d.x == UNDEF ? UNDEF : double_of_dim(info->height_d, dim_def);
	imginfo->lmar = info->lmar_d.x == UNDEF ? UNDEF : double_of_dim(info->lmar_d, dim_def);
	imginfo->rmar = info->rmar_d.x == UNDEF ? UNDEF : double_of_dim(info->rmar_d, dim_def);
	imginfo->tmar = info->tmar_d.x == UNDEF ? UNDEF : double_of_dim(info->tmar_d, dim_def);
	imginfo->bmar = info->bmar_d.x == UNDEF ? UNDEF : double_of_dim(info->bmar_d, dim_def);
	
	/* start with a standard rectangle */
	transform::trans_from_rect(&imginfo->trans, imginfo->pixwidth, imginfo->pixheight);
	
	/* if info.tight is set, tighten the bounding box */
	if (info->tight) {
		transform::trans_tighten(&imginfo->trans, plist);
	}
	
	/* sx/rx is just an alternate way to specify width; sy/ry is just an
	 alternate way to specify height. */
	if (info->backend->pixel) {
		if (imginfo->width == UNDEF && info->sx != UNDEF) {
			imginfo->width = imginfo->trans.bb[0] * info->sx;
		}
		if (imginfo->height == UNDEF && info->sy != UNDEF) {
			imginfo->height = imginfo->trans.bb[1] * info->sy;
		}
	} else {
		if (imginfo->width == UNDEF && info->rx != UNDEF) {
			imginfo->width = imginfo->trans.bb[0] / info->rx * 72;
		}
		if (imginfo->height == UNDEF && info->ry != UNDEF) {
			imginfo->height = imginfo->trans.bb[1] / info->ry * 72;
		}
	}
	
	/* if one of width/height is specified, use stretch to determine the
	 other */
	if (imginfo->width == UNDEF && imginfo->height != UNDEF) {
		imginfo->width = imginfo->height / imginfo->trans.bb[1] * imginfo->trans.bb[0] / info->stretch;
	} else if (imginfo->width != UNDEF && imginfo->height == UNDEF) {
		imginfo->height = imginfo->width / imginfo->trans.bb[0] * imginfo->trans.bb[1] * info->stretch;
	}
	
	/* if width and height are still variable, tenatively use the
	 default scaling factor of 72dpi (for dimension-based backends) or
	 1 (for pixel-based backends). For fixed-size backends, this will
	 be adjusted later to fit the page. */
	if (imginfo->width == UNDEF && imginfo->height == UNDEF) {
		imginfo->width = imginfo->trans.bb[0];
		imginfo->height = imginfo->trans.bb[1] * info->stretch;
		default_scaling = 1;
	}
	
	/* apply scaling */
	transform::trans_scale_to_size(&imginfo->trans, imginfo->width, imginfo->height);
	
	/* apply rotation, and tighten the bounding box again, if necessary */
	if (info->angle != 0.0) {
		transform::trans_rotate(&imginfo->trans, info->angle);
		if (info->tight) {
			transform::trans_tighten(&imginfo->trans, plist);
		}
	}
	
	/* for fixed-size backends, if default scaling was in effect,
	 further adjust the scaling to be the "best fit" for the given
	 page size and margins. */
	if (default_scaling && info->backend->fixed) {
		
		/* try to squeeze it between margins */
		maxwidth = UNDEF;
		maxheight = UNDEF;
		
		if (imginfo->lmar != UNDEF && imginfo->rmar != UNDEF) {
			maxwidth = info->paperwidth - imginfo->lmar - imginfo->rmar;
		}
		if (imginfo->bmar != UNDEF && imginfo->tmar != UNDEF) {
			maxheight = info->paperheight - imginfo->bmar - imginfo->tmar;
		}
		if (maxwidth == UNDEF && maxheight == UNDEF) {
			maxwidth = max(info->paperwidth - 144, info->paperwidth * 0.75);
			maxheight = max(info->paperheight - 144, info->paperheight * 0.75);
		}
		
		if (maxwidth == UNDEF) {
			sc = maxheight / imginfo->trans.bb[1];
		} else if (maxheight == UNDEF) {
			sc = maxwidth / imginfo->trans.bb[0];
		} else {
			sc = min(maxwidth / imginfo->trans.bb[0], maxheight / imginfo->trans.bb[1]);
		}
		
		/* re-scale coordinate system */
		imginfo->width *= sc;
		imginfo->height *= sc;
		transform::trans_rescale(&imginfo->trans, sc);
	}
	
	/* adjust margins */
	if (info->backend->fixed) {
		if (imginfo->lmar == UNDEF && imginfo->rmar == UNDEF) {
			imginfo->lmar = (info->paperwidth-imginfo->trans.bb[0])/2;
		} else if (imginfo->lmar == UNDEF) {
			imginfo->lmar = (info->paperwidth-imginfo->trans.bb[0]-imginfo->rmar);
		} else if (imginfo->lmar != UNDEF && imginfo->rmar != UNDEF) {
			imginfo->lmar += (info->paperwidth-imginfo->trans.bb[0]-imginfo->lmar-imginfo->rmar)/2;
		}
		if (imginfo->bmar == UNDEF && imginfo->tmar == UNDEF) {
			imginfo->bmar = (info->paperheight-imginfo->trans.bb[1])/2;
		} else if (imginfo->bmar == UNDEF) {
			imginfo->bmar = (info->paperheight-imginfo->trans.bb[1]-imginfo->tmar);
		} else if (imginfo->bmar != UNDEF && imginfo->tmar != UNDEF) {
			imginfo->bmar += (info->paperheight-imginfo->trans.bb[1]-imginfo->bmar-imginfo->tmar)/2;
		}
	} else
	{
		if (imginfo->lmar == UNDEF)
		{
			imginfo->lmar = 0;
		}
		if (imginfo->rmar == UNDEF)
		{
			imginfo->rmar = 0;
		}
		if (imginfo->bmar == UNDEF)
		{
			imginfo->bmar = 0;
		}
		if (imginfo->tmar == UNDEF)
		{
			imginfo->tmar = 0;
		}
	}
}

static dim_t parse_dimension(char *s, char **endptr) {
	char *p;
	dim_t res;
	
	res.x = strtod(s, &p);
	res.d = 0;
	if (p!=s) {
		if (!strncasecmp(p, "in", 2)) {
			res.d = DIM_IN;
			p += 2;
		} else if (!strncasecmp(p, "cm", 2)) {
			res.d = DIM_CM;
			p += 2;
		} else if (!strncasecmp(p, "mm", 2)) {
			res.d = DIM_MM;
			p += 2;
		} else if (!strncasecmp(p, "pt", 2)) {
			res.d = DIM_PT;
			p += 2;
		}
	}
	if (endptr!=NULL) {
		*endptr = p;
	}
	return res;
}

static void parse_dimensions(char *s, char **endptr, dim_t *dxp, dim_t *dyp) {
	char *p, *q;
	dim_t dx, dy;
	
	dx = parse_dimension(s, &p);
	if (p==s) {
		goto fail;
	}
	if (*p != 'x') {
		goto fail;
	}
	p++;
	dy = parse_dimension(p, &q);
	if (q==p) {
		goto fail;
	}
	if (dx.d && !dy.d) {
		dy.d = dx.d;
	} else if (!dx.d && dy.d) {
		dx.d = dy.d;
	}
	*dxp = dx;
	*dyp = dy;
	if (endptr != NULL) {
		*endptr = q;
	}
	return;
	
fail:
	dx.x = dx.d = dy.x = dy.d = 0;
	*dxp = dx;
	*dyp = dy;
	if (endptr != NULL) {
		*endptr = s;
	}
	return;
}

static int parse_color(char *s)
{
	int i, d;
	int col = 0;
	
	if (s[0] != '#' || strlen(s) != 7) {
		return -1;
	}
	for (i=0; i<6; i++) {
		d = s[6-i];
		if (d >= '0' && d <= '9') {
			col |= (d-'0') << (4*i);
		} else if (d >= 'a' && d <= 'f') {
			col |= (d-'a'+10) << (4*i);
		} else if (d >= 'A' && d <= 'F') {
			col |= (d-'A'+10) << (4*i);
		} else {
			return -1;
		}
	}
	return col;
}

int bm_readbody_bmp(std::vector<unsigned char> &buf, double threshold, potrace_bitmap_t **bmp)
{
	/* Bitmap file header */
	if (buf.size() < 2)
		return -3;
	
	if (!(buf[0] == 'B' && buf[1] == 'M'))
		return -4;
	
	bmp_info_t bmpinfo;
	int *coltable = NULL;
	unsigned int b, c;
	unsigned int i;
	potrace_bitmap_t *bm = NULL;
	int mask;
	unsigned int x, y;
	int col[2];
	unsigned int bitbuf;
	unsigned int n;
	unsigned int redshift, greenshift, blueshift;
	int col1[2];
	int bmp_count = 0; /* counter for byte padding */
	int bmp_pos = 2;  /* set file position */

	/* Bitmap file header */
	TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.FileSize));
	TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.reserved));
	TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.DataOffset));
	
	/* DIB header (bitmap information header) */
	TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.InfoSize));
	if (   bmpinfo.InfoSize == BITMAPINFOHEADER
			|| bmpinfo.InfoSize == OS22XBITMAPHEADER
			|| bmpinfo.InfoSize == BITMAPV4HEADER
			|| bmpinfo.InfoSize == BITMAPV5HEADER)
	{
		/* Windows or new OS/2 format */
		bmpinfo.ctbits = 32; /* sample size in color table */
		
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.w));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.h));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 2, &bmpinfo.Planes));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 2, &bmpinfo.bits));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.comp));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.ImageSize));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.XpixelsPerM));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.YpixelsPerM));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.ncolors));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.ColorsImportant));
		
		if (bmpinfo.InfoSize == BITMAPV4HEADER || bmpinfo.InfoSize == BITMAPV5HEADER)
		{
			TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.RedMask));
			TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.GreenMask));
			TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.BlueMask));
			TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 4, &bmpinfo.AlphaMask));
		}
		if (bmpinfo.w > 0x7fffffff)
		{
			goto format_error;
		}
		if (bmpinfo.h > 0x7fffffff)
		{
			bmpinfo.h = (-bmpinfo.h) & 0xffffffff;
			bmpinfo.topdown = 1;
		} else {
			bmpinfo.topdown = 0;
		}
		if (bmpinfo.h > 0x7fffffff)
		{
			goto format_error;
		}
	} else if (bmpinfo.InfoSize == BITMAPCOREHEADER)
	{
		/* old OS/2 format */
		bmpinfo.ctbits = 24; /* sample size in color table */
		
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 2, &bmpinfo.w));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 2, &bmpinfo.h));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 2, &bmpinfo.Planes));
		TRY(bmp_readint(buf, &bmp_pos, &bmp_count, 2, &bmpinfo.bits));

		bmpinfo.comp = 0;
		bmpinfo.ncolors = 0;
		bmpinfo.topdown = 0;
		
	} else {
		goto format_error;
	}
	
	if (bmpinfo.comp == 3 && bmpinfo.InfoSize < BITMAPV4HEADER)
	{
		/* bitfield feature is only understood with V4 and V5 format */
		goto format_error;
	}
	
	if (bmpinfo.comp > 3 || bmpinfo.bits > 32)
	{
		goto format_error;
	}
	
	/* forward to color table (e.g., if bmpinfo.InfoSize == 64) */
	TRY(bmp_forward(buf, &bmp_pos, &bmp_count, 14+bmpinfo.InfoSize));
	
	if (bmpinfo.Planes != 1)
	{
		goto format_error;  /* can't handle planes */
	}
	
	if (bmpinfo.ncolors == 0)
	{
		bmpinfo.ncolors = 1 << bmpinfo.bits;
	}
	
	/* color table, present only if bmpinfo.bits <= 8. */
	if (bmpinfo.bits <= 8)
	{
		coltable = (int *) calloc(bmpinfo.ncolors, sizeof(int));
		if (!coltable)
		{
			goto std_error;
		}
		/* NOTE: since we are reading a bitmap, we can immediately convert
		 the color table entries to bits. */
		for (i=0; i<bmpinfo.ncolors; i++)
		{
			TRY(bmp_readint(buf, &bmp_pos, &bmp_count, bmpinfo.ctbits/8, &c));
			c = ((c>>16) & 0xff) + ((c>>8) & 0xff) + (c & 0xff);
			coltable[i] = (c > 3 * threshold * 255 ? 0 : 1);
			if (i<2) {
				col1[i] = c;
			}
		}
	}
	
	/* forward to data */
	if (bmpinfo.InfoSize != 12)
	{ /* not old OS/2 format */
		TRY(bmp_forward(buf, &bmp_pos, &bmp_count, bmpinfo.DataOffset));
	}
	
	/* allocate bitmap */
	bm = bm_new(bmpinfo.w, bmpinfo.h);
	
	if (!bm)
	{
		goto std_error;
	}
	
	/* zero it out */
	bm_clear(bm, 0);
	
	switch (bmpinfo.bits + 0x100*bmpinfo.comp)
	{
			
  default:
			goto format_error;
			break;
			
  case 0x001:  /* monochrome palette */
			if (col1[0] < col1[1]) { /* make the darker color black */
				mask = 0xff;
			} else {
				mask = 0;
			}
			
			/* raster data */
			for (y=0; y<bmpinfo.h; y++) {
				PA_YieldAbsolute();
				bmp_pad_reset(&bmp_count);
				for (i=0; 8*i<bmpinfo.w; i++) {
					TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b));
					b ^= mask;
					*bm_index(bm, i*8, ycorr(y)) |= ((potrace_word)b) << (8*(BM_WORDSIZE-1-(i % BM_WORDSIZE)));
				}
				TRY(bmp_pad(buf, &bmp_pos, &bmp_count));
			}
			break;
   
  case 0x002:  /* 2-bit to 8-bit palettes */
  case 0x003:
  case 0x004:
  case 0x005:
  case 0x006:
  case 0x007:
  case 0x008:
			for (y=0; y<bmpinfo.h; y++) {
				PA_YieldAbsolute();
				bmp_pad_reset(&bmp_count);
				bitbuf = 0;  /* bit buffer: bits in buffer are high-aligned */
				n = 0;       /* number of bits currently in bitbuffer */
				for (x=0; x<bmpinfo.w; x++) {
					if (n < bmpinfo.bits) {
						TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b));
						bitbuf |= b << (INTBITS - 8 - n);
						n += 8;
					}
					b = bitbuf >> (INTBITS - bmpinfo.bits);
					bitbuf <<= bmpinfo.bits;
					n -= bmpinfo.bits;
					BM_UPUT(bm, x, ycorr(y), COLTABLE(b));
				}
				TRY(bmp_pad(buf, &bmp_pos, &bmp_count));
			}
			break;
			
  case 0x010:  /* 16-bit encoding */
			/* can't do this format because it is not well-documented and I
			 don't have any samples */
			goto format_error;
			break;
			
  case 0x018:  /* 24-bit encoding */
  case 0x020:  /* 32-bit encoding */
			for (y=0; y<bmpinfo.h; y++) {
				PA_YieldAbsolute();
				bmp_pad_reset(&bmp_count);
				for (x=0; x<bmpinfo.w; x++) {
					TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, bmpinfo.bits/8, &c));
					c = ((c>>16) & 0xff) + ((c>>8) & 0xff) + (c & 0xff);
					BM_UPUT(bm, x, ycorr(y), c > 3 * threshold * 255 ? 0 : 1);
				}
				TRY(bmp_pad(buf, &bmp_pos, &bmp_count));
			}
			break;
			
  case 0x320:  /* 32-bit encoding with bitfields */
			redshift = lobit(bmpinfo.RedMask);
			greenshift = lobit(bmpinfo.GreenMask);
			blueshift = lobit(bmpinfo.BlueMask);
			
			for (y=0; y<bmpinfo.h; y++) {
				PA_YieldAbsolute();
				bmp_pad_reset(&bmp_count);
				for (x=0; x<bmpinfo.w; x++) {
					TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, bmpinfo.bits/8, &c));
					c = ((c & bmpinfo.RedMask) >> redshift) + ((c & bmpinfo.GreenMask) >> greenshift) + ((c & bmpinfo.BlueMask) >> blueshift);
					BM_UPUT(bm, x, ycorr(y), c > 3 * threshold * 255 ? 0 : 1);
				}
				TRY(bmp_pad(buf, &bmp_pos, &bmp_count));
			}
			break;
			
  case 0x204:  /* 4-bit runlength compressed encoding (RLE4) */
			x = 0;
			y = 0;
			while (1) {
				PA_YieldAbsolute();
				TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b)); /* opcode */
				TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &c)); /* argument */
				if (b>0) {
					/* repeat count */
					col[0] = COLTABLE((c>>4) & 0xf);
					col[1] = COLTABLE(c & 0xf);
					for (i=0; i<b && x<bmpinfo.w; i++) {
						if (x>=bmpinfo.w) {
							x=0;
							y++;
						}
						if (y>=bmpinfo.h) {
							break;
						}
						BM_UPUT(bm, x, ycorr(y), col[i&1]);
						x++;
					}
				} else if (c == 0) {
					/* end of line */
					y++;
					x = 0;
				} else if (c == 1) {
					/* end of bitmap */
					break;
				} else if (c == 2) {
					/* "delta": skip pixels in x and y directions */
					TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b)); /* x offset */
					TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &c)); /* y offset */
					x += b;
					y += c;
				} else {
					/* verbatim segment */
					for (i=0; i<c; i++) {
						if ((i&1)==0) {
							TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b));
						}
						if (x>=bmpinfo.w) {
							x=0;
							y++;
						}
						if (y>=bmpinfo.h) {
							break;
						}
						BM_PUT(bm, x, ycorr(y), COLTABLE((b>>(4-4*(i&1))) & 0xf));
						x++;
					}
					if ((c+1) & 2) {
						/* pad to 16-bit boundary */
						TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b));
					}
				}
			}
			break;
			
  case 0x108:  /* 8-bit runlength compressed encoding (RLE8) */
			x = 0;
			y = 0;
			while (1) {
				PA_YieldAbsolute();
				TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b)); /* opcode */
				TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &c)); /* argument */
				if (b>0) {
					/* repeat count */
					for (i=0; i<b; i++) {
						if (x>=bmpinfo.w) {
							x=0;
							y++;
						}
						if (y>=bmpinfo.h) {
							break;
						}
						BM_UPUT(bm, x, ycorr(y), COLTABLE(c));
						x++;
					}
				} else if (c == 0) {
					/* end of line */
					y++;
					x = 0;
				} else if (c == 1) {
					/* end of bitmap */
					break;
				} else if (c == 2) {
					/* "delta": skip pixels in x and y directions */
					TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b)); /* x offset */
					TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &c)); /* y offset */
					x += b;
					y += c;
				} else {
					/* verbatim segment */
					for (i=0; i<c; i++) {
						TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b));
						if (x>=bmpinfo.w) {
							x=0;
							y++;
						}
						if (y>=bmpinfo.h) {
							break;
						}
						BM_PUT(bm, x, ycorr(y), COLTABLE(b));
						x++;
					}
					if (c & 1) {
						/* pad input to 16-bit boundary */
						TRY_EOF(bmp_readint(buf, &bmp_pos, &bmp_count, 1, &b));
					}
				}
			}
			break;
			
	} /* switch */
	
	/* skip any potential junk after the data section, but don't
	 complain in case EOF is encountered */
	bmp_forward(buf, &bmp_pos, &bmp_count, bmpinfo.FileSize);
	
	free(coltable);
	*bmp = bm;
	return 0;
	
eof:
	if(coltable) free(coltable);
	*bmp = bm;
	return 1;
	
format_error:
try_error:
	if(coltable) free(coltable);
	if(bm) bm_free(bm);
	return -2;
	
std_error:
	if(coltable) free(coltable);
	if(bm) bm_free(bm);
	return -1;
}

#pragma mark -

void Potrace(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT Param2;
	ARRAY_LONGINT Param3Keys;
	ARRAY_TEXT Param4Values;
	
	Param2.fromParamAtIndex(pParams, 2);
	Param3Keys.fromParamAtIndex(pParams, 3);
	Param4Values.fromParamAtIndex(pParams, 4);
	
	PA_Picture p = *(PA_Picture *)(pParams[0]);
	
	if(p)
	{
		CUTF8String codec_u8 = CUTF8String((const uint8_t *)".bmp");
		uint32_t size = ((uint32_t)codec_u8.length() * sizeof(PA_Unichar)) + sizeof(PA_Unichar);
		std::vector<uint8_t> buf(size);
		uint32_t len = PA_ConvertCharsetToCharset(
																							(char *)codec_u8.c_str(),
																							(uint32_t)codec_u8.length(),
																							eVTC_UTF_8,
																							(char *)&buf[0],
																							size,
																							eVTC_UTF_16
																							);
		CUTF16String codec_u16 = CUTF16String((const PA_Unichar *)&buf[0], len);
		
		p = PA_DuplicatePicture(p, kcpRetainOnly); // disposed each time with PA_ClearVariable
		PA_Variable args[2];
		args[0] = PA_CreateVariable(eVK_Picture);
		PA_SetPictureVariable(&args[0], p);
		
		args[1] = PA_CreateVariable(eVK_Unistring);
		PA_Unistring codec = PA_CreateUnistring((PA_Unichar *)codec_u16.c_str()); // disposed each time with PA_ClearVariable
		PA_SetStringVariable(&args[1], &codec);
		
		PA_ExecuteCommandByID(CMD_CONVERT_PICTURE, args, 2);
		
		p = PA_GetPictureVariable(args[0]); // a new picture is created
		
		PA_Handle h = PA_NewHandle(0);
		PA_GetPictureData(p, 1, h);
		
		potrace_bitmap_t *bmp = NULL;
		double threshold = 0.5;
		
		if(PA_GetLastError() == eER_NoErr)
		{
			std::vector<unsigned char> buf(PA_GetHandleSize(h));
			PA_MoveBlock((void *)PA_LockHandle(h), &buf[0], PA_GetHandleSize(h));
			PA_UnlockHandle(h);
			
			if(bm_readbody_bmp(buf, threshold, &bmp) >= 0)
			{
				potrace_param_t *param = potrace_param_default();
				potrace_state_t *st = potrace_trace(param, bmp);
				
				if(st)
				{
					if(st->status == POTRACE_STATUS_OK)
					{
						info_t info;
						memset(&info, 0x00, sizeof(info_t));
						
						info.debug = 0;
						info.width_d.x = UNDEF;
						info.height_d.x = UNDEF;
						info.rx = UNDEF;
						info.ry = UNDEF;
						info.sx = UNDEF;
						info.sy = UNDEF;
						info.stretch = 1;
						info.lmar_d.x = UNDEF;
						info.rmar_d.x = UNDEF;
						info.tmar_d.x = UNDEF;
						info.bmar_d.x = UNDEF;
						info.angle = 0;
						info.paperwidth = DEFAULT_PAPERWIDTH;
						info.paperheight = DEFAULT_PAPERHEIGHT;
						info.tight = 0;
						info.unit = 10;
						info.compress = 0;
						info.pslevel = 2;
						info.color = 0x000000;
						info.gamma = 2.2;
						info.longcoding = 0;
						info.outfile = NULL;
						info.blacklevel = 0.5;
						info.invert = 0;
						info.opaque = 0;
						info.grouping = 1;
						info.fillcolor = 0xFFFFFF;
						info.progress = 0;
						info.progress_bar = NULL;
						info.param = param;
						
						backend_t backend;
						memset(&backend, 0x00, sizeof(backend_t));
						
						if(Param3Keys.getSize() == Param4Values.getSize())
						{
							unsigned int option;
							CUTF8String value;
							for(unsigned int i = 0; i < Param3Keys.getSize(); ++i)
							{
								option = Param3Keys.getIntValueAtIndex(i);
								Param4Values.copyUTF8StringAtIndex(&value, i);
								switch (option)
								{
									case POTRACE_OPT_TURN_POLICY:
									{
										unsigned int turnpolicy = atoi((char *)value.c_str());
										if(turnpolicy <= POTRACE_TURN_RANDOM)
										{
											info.param->turnpolicy = turnpolicy;
										}
									}
										break;
									case POTRACE_OPT_MINIMUM_SPECKLE:
										info.param->turdsize = atoi((char *)value.c_str());
										break;
									case POTRACE_OPT_CORNER_THRESHOLD:
									{
										char *p;
										double alphamax = strtod((char *)value.c_str(), &p);
										if(!(*p)){
											info.param->alphamax = alphamax;
										}
									}
										break;
									case POTRACE_OPT_USE_LONG_CURVE:
										info.param->opticurve = 0;
										break;
									case POTRACE_OPT_CURVE_TOLERANCE:
									{
										char *p;
										double opttolerance = strtod((char *)value.c_str(), &p);
										if(!(*p)){
											info.param->opttolerance = opttolerance;
										}
									}
										break;
									case POTRACE_OPT_OPAQUE:
										info.opaque = 1;
										break;
									case POTRACE_OPT_COLOR_FILL:
									{
										int fillcolor = parse_color((char *)value.c_str());
										info.fillcolor = fillcolor != -1 ? fillcolor : info.fillcolor;
									}
										break;
									case POTRACE_OPT_COLOR:
									{
										int color = parse_color((char *)value.c_str());
										info.color = color != -1 ? color : info.color;
									}
										break;
									case POTRACE_OPT_INVERT:
										info.invert = 1;
										break;
									case POTRACE_OPT_TIGHT:
										info.tight = 1;
										break;
									case POTRACE_OPT_MARGIN_LEFT:
									{
										char *p;
										dim_t lmar_d = parse_dimension((char *)value.c_str(), &p);
										if(!(*p)){
											info.lmar_d.x = lmar_d.x;
											info.lmar_d.d = lmar_d.d;
										}
									}
										break;
									case POTRACE_OPT_MARGIN_RIGHT:
									{
										char *p;
										dim_t rmar_d = parse_dimension((char *)value.c_str(), &p);
										if(!(*p)){
											info.rmar_d.x = rmar_d.x;
											info.rmar_d.d = rmar_d.d;
										}
									}
										break;
									case POTRACE_OPT_MARGIN_TOP:
									{
										char *p;
										dim_t tmar_d = parse_dimension((char *)value.c_str(), &p);
										if(!(*p)){
											info.tmar_d.x = tmar_d.x;
											info.tmar_d.d = tmar_d.d;
										}
									}
										break;
									case POTRACE_OPT_MARGIN_BOTTOM:
									{
										char *p;
										dim_t bmar_d = parse_dimension((char *)value.c_str(), &p);
										if(!(*p)){
											info.bmar_d.x = bmar_d.x;
											info.bmar_d.d = bmar_d.d;
										}
									}
										break;
									case POTRACE_OPT_ROTATE:
									{
										char *p;
										double angle = strtod((char *)value.c_str(), &p);
										if(!(*p)){
											info.angle = angle;
										}
									}
										break;
									case POTRACE_OPT_GAMMA:
										info.gamma = atof((char *)value.c_str());
										break;
									case POTRACE_OPT_BLACK_LEVEL:
									{
										char *p;
										double blacklevel = strtod((char *)value.c_str(), &p);
										if(!(*p)){
											info.blacklevel = blacklevel;
										}
									}
										break;
									case POTRACE_OPT_RESOLUTION:
									{
										char *p;
										dim_t dimx, dimy;
										parse_dimensions((char *)value.c_str(), &p, &dimx, &dimy);
										if (*p == 0 && dimx.d == 0 && dimy.d == 0 && dimx.x != 0.0 && dimy.x != 0.0) {
											info.rx = dimx.x;
											info.ry = dimy.x;
										}
									}
										break;
									case POTRACE_OPT_SCALE:
									{
										char *p;
										dim_t dimx, dimy;
										parse_dimensions((char *)value.c_str(), &p, &dimx, &dimy);
										if (*p == 0 && dimx.d == 0 && dimy.d == 0) {
											info.sx = dimx.x;
											info.sy = dimy.x;
										}
									}
										break;
									case POTRACE_OPT_WIDTH:
									{
										char *p;
										dim_t width_d = parse_dimension((char *)value.c_str(), &p);
										if(!(*p)){
											info.width_d.x = width_d.x;
											info.width_d.d = width_d.d;
										}
									}
										break;
									case POTRACE_OPT_HEIGHT:
									{
										char *p;
										dim_t height_d = parse_dimension((char *)value.c_str(), &p);
										if(!(*p)){
											info.height_d.x = height_d.x;
											info.height_d.d = height_d.d;
										}
									}
										break;
									case POTRACE_OPT_STRETCH:
										info.stretch = atof((char *)value.c_str());
										break;
									case POTRACE_OPT_UNIT:
									{
										char *p;
										double unit = strtod((char *)value.c_str(), &p);
										if(!(*p)){
											info.unit = unit;
										}
									}
										break;
									case POTRACE_OPT_SVG_GROUP:
										info.grouping = 2;
										break;
									case POTRACE_OPT_SVG_FLAT:
										info.grouping = 0;
										break;
									case POTRACE_OPT_PS_LEVEL_2:
//										info.pslevel = 2;
//										info.compress = 1;
										break;
									case POTRACE_OPT_PS_LEVEL_3:
//										info.pslevel = 3;
//										info.compress = 1;
										break;
									case POTRACE_OPT_PS_CLEAR_TEXT:
										info.pslevel = 2;
										info.compress = 0;
										break;
									default:
										break;
								}
							}
						}
						
						switch (Param2.getIntValue())
						{
							case POTRACE_OUTPUT_EPS:
							case POTRACE_OUTPUT_PS:
							case POTRACE_OUTPUT_PDF:
								//backend:pdf
								backend.name = (char *)"pdf";
								backend.ext = (char *)".pdf";
								backend.page_f = page_pdf;
								backend.opticurve = 1;
								break;
							case POTRACE_OUTPUT_SVG:
							default:
								//backend:svg
								backend.name = (char *)"svg";
								backend.ext = (char *)".svg";
								backend.page_f = page_svg;
								backend.opticurve = 1;
								break;
						}

						info.backend = &backend;
						
						imginfo_t imginfo;
						imginfo.pixwidth = bmp->w;
						imginfo.pixheight = bmp->h;						
						calc_dimensions(&imginfo, st->plist, &info);
						
						info.backend->page_f(pResult, st->plist, &imginfo, &info);
					}
					potrace_state_free(st);
				}
				
				potrace_param_free(param);
				bm_free(bmp);
			}
		}
		
		PA_DisposeHandle(h);
		
		PA_ClearVariable(&args[0]);
		PA_ClearVariable(&args[1]);
	}
}

#pragma mark bbox.c

/* Copyright (C) 2001-2015 Peter Selinger.
 This file is part of Potrace. It is free software and it is covered
 by the GNU General Public License. See the file COPYING for details. */

/* figure out the true bounding box of a vector image */

/* ---------------------------------------------------------------------- */
/* intervals */

namespace bbox {

	/* initialize the interval to [min, max] */
	static void interval(interval_t *i, double min, double max) {
		i->min = min;
		i->max = max;
	}

	/* initialize the interval to [x, x] */
	static inline void singleton(interval_t *i, double x) {
		interval(i, x, x);
	}

	/* extend the interval to include the number x */
	static inline void extend(interval_t *i, double x) {
		if (x < i->min) {
			i->min = x;
		} else if (x > i->max) {
			i->max = x;
		}
	}

	static inline int in_interval(interval_t *i, double x) {
		return i->min <= x && x <= i->max;
	}

	/* ---------------------------------------------------------------------- */
	/* inner product */

	typedef potrace_dpoint_t dpoint_t;

	static double iprod(dpoint_t a, dpoint_t b) {
		return a.x * b.x + a.y * b.y;
	}

	/* ---------------------------------------------------------------------- */
	/* linear Bezier segments */

	/* return a point on a 1-dimensional Bezier segment */
	static inline double bezier(double t, double x0, double x1, double x2, double x3) {
		double s = 1-t;
		return s*s*s*x0 + 3*(s*s*t)*x1 + 3*(t*t*s)*x2 + t*t*t*x3;
	}

	/* Extend the interval i to include the minimum and maximum of a
	 1-dimensional Bezier segment given by control points x0..x3. For
	 efficiency, x0 in i is assumed as a precondition. */
	static void bezier_limits(double x0, double x1, double x2, double x3, interval_t *i) {
		double a, b, c, d, r;
		double t, x;
		
		/* the min and max of a cubic curve segment are attained at one of
		 at most 4 critical points: the 2 endpoints and at most 2 local
		 extrema. We don't check the first endpoint, because all our
		 curves are cyclic so it's more efficient not to check endpoints
		 twice. */
		
		/* endpoint */
		extend(i, x3);
		
		/* optimization: don't bother calculating extrema if all control
		 points are already in i */
		if (in_interval(i, x1) && in_interval(i, x2)) {
			return;
		}
		
		/* solve for extrema. at^2 + bt + c = 0 */
		a = -3*x0 + 9*x1 - 9*x2 + 3*x3;
		b = 6*x0 - 12*x1 + 6*x2;
		c = -3*x0 + 3*x1;
		d = b*b - 4*a*c;
		if (d > 0) {
			r = sqrt(d);
			t = (-b-r)/(2*a);
			if (t > 0 && t < 1) {
				x = bezier(t, x0, x1, x2, x3);
				extend(i, x);
			}
			t = (-b+r)/(2*a);
			if (t > 0 && t < 1) {
				x = bezier(t, x0, x1, x2, x3);
				extend(i, x);
			}
		}
		return;
	}

	/* ---------------------------------------------------------------------- */
	/* Potrace segments, curves, and pathlists */

	/* extend the interval i to include the inner product <v | dir> for
	 all points v on the segment. Assume precondition a in i. */
	static inline void segment_limits(int tag, dpoint_t a, dpoint_t c[3], dpoint_t dir, interval_t *i) {
		switch (tag) {
		case POTRACE_CORNER:
				extend(i, iprod(c[1], dir));
				extend(i, iprod(c[2], dir));
				break;
		case POTRACE_CURVETO:
				bezier_limits(iprod(a, dir), iprod(c[0], dir), iprod(c[1], dir), iprod(c[2], dir), i);
				break;
		}
	}

	/* extend the interval i to include <v | dir> for all points v on the
	 curve. */
	static void curve_limits(potrace_curve_t *curve, dpoint_t dir, interval_t *i) {
		int k;
		int n = curve->n;
		
		segment_limits(curve->tag[0], curve->c[n-1][2], curve->c[0], dir, i);
		for (k=1; k<n; k++) {
			segment_limits(curve->tag[k], curve->c[k-1][2], curve->c[k], dir, i);
		}
	}

	/* compute the interval i to be the smallest interval including all <v
	 | dir> for points in the pathlist. If the pathlist is empty, return
	 the singleton interval [0,0]. */
	void path_limits(potrace_path_t *path, dpoint_t dir, interval_t *i) {
		potrace_path_t *p;
		
		/* empty image? */
		if (path == NULL) {
			interval(i, 0, 0);
			return;
		}
		
		/* initialize interval to a point on the first curve */
		singleton(i, iprod(path->curve.c[0][2], dir));
		
		/* iterate */
		list_forall(p, path) {
			curve_limits(&p->curve, dir, i);
		}
	}
	
}

#pragma mark trans.c

/* Copyright (C) 2001-2015 Peter Selinger.
 This file is part of Potrace. It is free software and it is covered
 by the GNU General Public License. See the file COPYING for details. */

/* transform jaggy paths into smooth curves */

namespace transform {
	
	/* rotate the coordinate system counterclockwise by alpha degrees. The
	 new bounding box will be the smallest box containing the rotated
	 old bounding box */
	void trans_rotate(trans_t *r, double alpha) {
  double s, c, x0, x1, y0, y1, o0, o1;
  trans_t t_struct;
  trans_t *t = &t_struct;
		
  memcpy(t, r, sizeof(trans_t));
		
  s = sin(alpha/180*M_PI);
  c = cos(alpha/180*M_PI);
		
  /* apply the transformation matrix to the sides of the bounding box */
  x0 = c * t->bb[0];
  x1 = s * t->bb[0];
  y0 = -s * t->bb[1];
  y1 = c * t->bb[1];
		
  /* determine new bounding box, and origin of old bb within new bb */
  r->bb[0] = fabs(x0) + fabs(y0);
  r->bb[1] = fabs(x1) + fabs(y1);
  o0 = - min(x0,0) - min(y0,0);
  o1 = - min(x1,0) - min(y1,0);
		
  r->orig[0] = o0 + c * t->orig[0] - s * t->orig[1];
  r->orig[1] = o1 + s * t->orig[0] + c * t->orig[1];
  r->x[0] = c * t->x[0] - s * t->x[1];
  r->x[1] = s * t->x[0] + c * t->x[1];
  r->y[0] = c * t->y[0] - s * t->y[1];
  r->y[1] = s * t->y[0] + c * t->y[1];
	}
	
	/* return the standard cartesian coordinate system for an w x h rectangle. */
	void trans_from_rect(trans_t *r, double w, double h) {
  r->bb[0] = w;
  r->bb[1] = h;
  r->orig[0] = 0.0;
  r->orig[1] = 0.0;
  r->x[0] = 1.0;
  r->x[1] = 0.0;
  r->y[0] = 0.0;
  r->y[1] = 1.0;
  r->scalex = 1.0;
  r->scaley = 1.0;
	}
	
	/* rescale the coordinate system r by factor sc >= 0. */
	void trans_rescale(trans_t *r, double sc) {
  r->bb[0] *= sc;
  r->bb[1] *= sc;
  r->orig[0] *= sc;
  r->orig[1] *= sc;
  r->x[0] *= sc;
  r->x[1] *= sc;
  r->y[0] *= sc;
  r->y[1] *= sc;
  r->scalex *= sc;
  r->scaley *= sc;
	}
	
	/* rescale the coordinate system to size w x h */
	void trans_scale_to_size(trans_t *r, double w, double h) {
  double xsc = w/r->bb[0];
  double ysc = h/r->bb[1];
		
  r->bb[0] = w;
  r->bb[1] = h;
  r->orig[0] *= xsc;
  r->orig[1] *= ysc;
  r->x[0] *= xsc;
  r->x[1] *= ysc;
  r->y[0] *= xsc;
  r->y[1] *= ysc;
  r->scalex *= xsc;
  r->scaley *= ysc;
  
  if (w<0) {
		r->orig[0] -= w;
		r->bb[0] = -w;
	}
  if (h<0) {
		r->orig[1] -= h;
		r->bb[1] = -h;
	}
	}
	
	/* adjust the bounding box to the actual vector outline */
	void trans_tighten(trans_t *r, potrace_path_t *plist) {
  interval_t i;
  dpoint_t dir;
  int j;
  
  /* if pathlist is empty, do nothing */
  if (!plist) {
		return;
	}
		
  for (j=0; j<2; j++) {
		dir.x = r->x[j];
		dir.y = r->y[j];
		bbox::path_limits(plist, dir, &i);
		if (i.min == i.max) {
			/* make the extent non-zero to avoid later division by zero errors */
			i.max = i.min+0.5;
			i.min = i.min-0.5;
		}
		r->bb[j] = i.max - i.min;
		r->orig[j] = -i.min;
	}
	}
	
}
