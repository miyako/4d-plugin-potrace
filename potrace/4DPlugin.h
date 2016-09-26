/* --------------------------------------------------------------------------------
 #
 #	4DPlugin.h
 #	source generated by 4D Plugin Wizard
 #	Project : Potrace
 #	author : miyako
 #	2016/09/23
 #
 # --------------------------------------------------------------------------------*/

#include "auxiliary.h"
#include "bitmap.h"
#include "bitops.h"
#include "lists.h"

#ifdef _WINDOWS
#define snprintf _snprintf 
#define vsnprintf _vsnprintf 
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp 
#endif

#include "backend_svg.h"
#include "backend_pdf.h"

// --- Potrace
void Potrace(sLONG_PTR *pResult, PackagePtr pParams);

#define POTRACE_TURN_BLACK 0
#define POTRACE_TURN_WHITE 1
#define POTRACE_TURN_LEFT 2
#define POTRACE_TURN_RIGHT 3
#define POTRACE_TURN_MINORITY 4
#define POTRACE_TURN_MAJORITY 5
#define POTRACE_TURN_RANDOM 6

#define POTRACE_OUTPUT_SVG 0
#define POTRACE_OUTPUT_PDF 1
#define POTRACE_OUTPUT_EPS 2
#define POTRACE_OUTPUT_PS 3

#define POTRACE_OPT_TURN_POLICY 11
#define POTRACE_OPT_MINIMUM_SPECKLE 12
#define POTRACE_OPT_CORNER_THRESHOLD 13
#define POTRACE_OPT_USE_LONG_CURVE 14
#define POTRACE_OPT_CURVE_TOLERANCE 15

#define POTRACE_OPT_OPAQUE 21
#define POTRACE_OPT_COLOR_FILL 22
#define POTRACE_OPT_COLOR 23
#define POTRACE_OPT_INVERT 24

#define POTRACE_OPT_TIGHT 31
#define POTRACE_OPT_MARGIN_LEFT 32
#define POTRACE_OPT_MARGIN_RIGHT 33
#define POTRACE_OPT_MARGIN_TOP 34
#define POTRACE_OPT_MARGIN_BOTTOM 35

#define POTRACE_OPT_ROTATE 41
#define POTRACE_OPT_GAMMA 42
#define POTRACE_OPT_BLACK_LEVEL 43
#define POTRACE_OPT_RESOLUTION 44
#define POTRACE_OPT_SCALE 45
#define POTRACE_OPT_WIDTH 46
#define POTRACE_OPT_HEIGHT 47
#define POTRACE_OPT_STRETCH 48
#define POTRACE_OPT_UNIT 49

#define POTRACE_OPT_SVG_GROUP 51
#define POTRACE_OPT_SVG_FLAT 52

#define POTRACE_OPT_PS_LEVEL_2 61
#define POTRACE_OPT_PS_LEVEL_3 62
#define POTRACE_OPT_PS_CLEAR_TEXT 63

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define UNDEF ((double)(1e30))   /* a value to represent "undefined" */

#define CMD_CONVERT_PICTURE 1002
#define kcpRetainOnly 1

#define DIM_IN (72)
#define DIM_CM (72 / 2.54)
#define DIM_MM (72 / 25.4)
#define DIM_PT (1)

/* set some configurable defaults */

#ifdef USE_METRIC
#define DEFAULT_DIM DIM_CM
#define DEFAULT_DIM_NAME "centimeters"
#else
#define DEFAULT_DIM DIM_IN
#define DEFAULT_DIM_NAME "inches"
#endif

#ifdef USE_A4
#define DEFAULT_PAPERWIDTH 595
#define DEFAULT_PAPERHEIGHT 842
#define DEFAULT_PAPERFORMAT "a4"
#else
#define DEFAULT_PAPERWIDTH 612
#define DEFAULT_PAPERHEIGHT 792
#define DEFAULT_PAPERFORMAT "letter"
#endif

struct bmp_info_s {
	unsigned int FileSize;
	unsigned int reserved;
	unsigned int DataOffset;
	unsigned int InfoSize;
	unsigned int w;              /* width */
	unsigned int h;              /* height */
	unsigned int Planes;
	unsigned int bits;           /* bits per sample */
	unsigned int comp;           /* compression mode */
	unsigned int ImageSize;
	unsigned int XpixelsPerM;
	unsigned int YpixelsPerM;
	unsigned int ncolors;        /* number of colors in palette */
	unsigned int ColorsImportant;
	unsigned int RedMask;
	unsigned int GreenMask;
	unsigned int BlueMask;
	unsigned int AlphaMask;
	unsigned int ctbits;         /* sample size for color table */
	int topdown;                 /* top-down mode? */
};
typedef struct bmp_info_s bmp_info_t;

#define BITMAPCOREHEADER 12
#define OS22XBITMAPHEADER 64
#define BITMAPINFOHEADER 40
#define BITMAPV2INFOHEADER 52
#define BITMAPV3INFOHEADER 56
#define BITMAPV4HEADER 108
#define BITMAPV5HEADER 124

namespace transform
{
	void trans_rotate(trans_t *r, double alpha);
	void trans_from_rect(trans_t *r, double w, double h);
	void trans_rescale(trans_t *r, double sc);
	void trans_scale_to_size(trans_t *r, double w, double h);
	void trans_tighten(trans_t *r, potrace_path_t *plist);
}