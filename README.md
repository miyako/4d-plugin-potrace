# 4d-plugin-potrace

Convert picture to SVG or PDF with [potrace](http://potrace.sourceforge.net).

##Platform

| carbon | cocoa | win32 | win64 |
|:------:|:-----:|:---------:|:---------:|
|🆗|🆗|🆗|🆗|

```c
//Potrace Option (policy)

POTRACE_TURN_BLACK 0
POTRACE_TURN_WHITE 1
POTRACE_TURN_LEFT 2
POTRACE_TURN_RIGHT 3
POTRACE_TURN_MINORITY 4
POTRACE_TURN_MAJORITY 5
POTRACE_TURN_RANDOM 6

//Potrace Output

POTRACE_OUTPUT_SVG 0
POTRACE_OUTPUT_PDF 1
POTRACE_OUTPUT_EPS 2
POTRACE_OUTPUT_PS 3

//Potrace Option (algorithm)

POTRACE_OPT_TURN_POLICY 11
POTRACE_OPT_MINIMUM_SPECKLE 12
POTRACE_OPT_CORNER_THRESHOLD 13
POTRACE_OPT_USE_LONG_CURVE 14
POTRACE_OPT_CURVE_TOLERANCE 15

//Potrace Option (color)

POTRACE_OPT_OPAQUE 21
POTRACE_OPT_COLOR_FILL 22
POTRACE_OPT_COLOR 23
POTRACE_OPT_INVERT 24

//Potrace Option (margin)

POTRACE_OPT_TIGHT 31
POTRACE_OPT_MARGIN_LEFT 32
POTRACE_OPT_MARGIN_RIGHT 33
POTRACE_OPT_MARGIN_TOP 34
POTRACE_OPT_MARGIN_BOTTOM 35

//Potrace Option (transform)

POTRACE_OPT_ROTATE 41
POTRACE_OPT_GAMMA 42
POTRACE_OPT_BLACK_LEVEL 43
POTRACE_OPT_RESOLUTION 44
POTRACE_OPT_SCALE 45
POTRACE_OPT_WIDTH 46
POTRACE_OPT_HEIGHT 47
POTRACE_OPT_STRETCH 48
POTRACE_OPT_UNIT 49

//Potrace Option (SVG)

POTRACE_OPT_SVG_GROUP 51
POTRACE_OPT_SVG_FLAT 52

//Potrace Option (PS)

POTRACE_OPT_PS_LEVEL_2 61
POTRACE_OPT_PS_LEVEL_3 62
POTRACE_OPT_PS_CLEAR_TEXT 63
```

###Note

``POTRACE_OPT_PS_LEVEL_2`` and ``POTRACE_OPT_PS_LEVEL_3`` are not implemented; only plain PDF with no compression is supported.

``POTRACE_OUTPUT_EPS`` and ``POTRACE_OUTPUT_PS`` are not implemented; only SVG and PDF are supported.

##Example

```
//source:http://www.neuro-image.com/?page_id=2276
$path:=Get 4D folder(Current resources folder)+"38648554_shitae2.bmp"

READ PICTURE FILE($path;$image)

ARRAY LONGINT($keys;6)
ARRAY TEXT($values;6)

$keys{1}:=POTRACE_OPT_CORNER_THRESHOLD  //default:1
$values{1}:="1"

$keys{2}:=POTRACE_OPT_CURVE_TOLERANCE  //default:0.2
$values{2}:="0.2"

$keys{3}:=POTRACE_OPT_USE_LONG_CURVE  //default:0
$values{3}:="0"

$keys{4}:=POTRACE_OPT_MINIMUM_SPECKLE  //default:2
$values{4}:="1"

$keys{5}:=POTRACE_OPT_TURN_POLICY  //default:POTRACE_TURN_BLACK
$values{5}:=Char(POTRACE_TURN_BLACK)

$keys{6}:=POTRACE_OPT_COLOR
$values{6}:="#FF00FF"

$svg:=Potrace ($image;POTRACE_OUTPUT_SVG;$keys;$values)
WRITE PICTURE FILE(System folder(Desktop)+"test.svg";$svg)

$svg:=Potrace ($image;POTRACE_OUTPUT_PDF;$keys;$values)
WRITE PICTURE FILE(System folder(Desktop)+"test.pdf";$svg)
```
