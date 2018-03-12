
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdbool.h>
#include "ddebug.h"

#ifdef __cplusplus  
extern "C" {  
#endif 

//一般拿到后的验证码都得经过灰度化，二值化，去噪
static ngx_http_module_t ngx_http_lua_tesseract_ctx = {
    NULL,                           		/* preconfiguration */
    NULL,         		/* postconfiguration */
    NULL,		/* create main configuration */
    NULL,       /* init main configuration */
    NULL,                           		/* create server configuration */
    NULL,                           		/* merge server configuration */
    NULL,                           		/* create location configuration */
    NULL                            		/* merge location configuration */
};
static ngx_command_t  ngx_http_lua_tesseract_commands[] = {};

ngx_module_t ngx_http_lua_tesseract_module = {
    NGX_MODULE_V1,
    &ngx_http_lua_tesseract_ctx,  					/* module context */
    ngx_http_lua_tesseract_commands,					/* module directives */
    NGX_HTTP_MODULE,            				/* module type */
    NULL,   									/* init master */
    NULL,              /* init module */
    NULL,              /* init process */
    NULL,                       				/* init thread */
    NULL,                       				/* exit thread */
    NULL,             	/* exit process */
    NULL,             	/* exit master */
    NGX_MODULE_V1_PADDING
};
#define 	VALID_SEQUENCE   "O1.3 + C3.1 + R22 + D2.2 + X4"
   /* Mask at 4x reduction */
static const char *mask_sequence = "r11";
    /* Seed at 4x reduction, formed by doing a 16x reduction,
     * an opening, and finally a 4x replicative expansion. */
static const char *seed_sequence = "r1143 + o5.5+ x4"; 
    /* Simple dilation */
static const char *dilation_sequence = "d3.3";
int ngx_tesseract_ffi_get_utf8_text(unsigned char *impBuf, size_t imgLen, unsigned char *outText,size_t outLen,const char *lang){
	char *text;
	PIX         *pixs, *pixc, *pixg, *pixim, *pixb ,*pixseed4 ,*pixmask4 ,*pixsf4 ,*pixd4 ,*pixd;
	l_int32      w, h, d;
	tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
	pixc=NULL;
    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (api->Init(NULL, lang)) {
        fprintf(stderr, "Could not initialize tesseract.\n");
		delete api;
        return -1;
    }

    // Open input image with leptonica library pixReadStream/pixRead
    pixs = pixReadMem((l_uint8*)impBuf,imgLen);
	/*
	pixGetDimensions(pixs, &w, &h, &d);
	
	if (d == 32) {
        pixc = pixClone(pixs);
        pixg = pixConvertRGBToGray(pixs, 0.33, 0.34, 0.33);
    } else {
        pixc = pixConvertTo32(pixs);
        pixg = pixClone(pixs);
    }
	*/
	if ((d=pixGetDepth(pixs)) == 32)
        pixg = pixConvertRGBToGrayFast(pixs);
    else
        pixg = pixClone(pixs);
	
	if (pixGetDepth(pixg) == 8)
        pixb = pixThresholdToBinary(pixg, 2);
    else
        pixb = pixClone(pixg);

        /* Make seed and mask, and fill seed into mask */
    pixseed4 = pixMorphSequence(pixb, seed_sequence, 0);
    pixmask4 = pixMorphSequence(pixb, mask_sequence, 0);
    pixsf4 = pixSeedfillBinary(NULL, pixseed4, pixmask4, 8);
    pixd4 = pixMorphSequence(pixsf4, dilation_sequence, 0);

        /* Mask at full resolution */
    pixd = pixExpandBinaryPower2(pixd4, 4);
    pixWrite("out.png", pixd, IFF_TIFF_G4);if(1==1)return 0;
	pixg = pixMorphSequence(pixs, VALID_SEQUENCE, 100);//pixMorphSequence(pixg, VALID_SEQUENCE, 100);
	pixWrite("out.png", pixg, IFF_PNG);
	pixim = pixCreate(w, h, 1);
	Pix *grayscaleImage=pixBackgroundNormSimple(pixs,pixim,pixg);
    api->SetImage(grayscaleImage);
    // Get OCR result
    text = api->GetUTF8Text();
	size_t tlen=strlen(text);
	tlen=tlen>outLen?outLen:tlen;
	ngx_memcpy(outText,text,tlen);
    // Destroy used object and release memory
    api->End();
    delete [] text;
    pixDestroy(&pixs);
	return 0;
}
#ifdef __cplusplus  
}  
#endif  
