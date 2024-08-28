#include "asterisk/format_cache.h"      /* for ast_format_evs */
#include "asterisk/frame.h"             /* for ast_frame, etc */

/*
Enhanced Voice Services
[Framing Mode: Header-full]
CMR WB 5.9 kbps (VBR)
	1... .... = Header Type identification bit (H): CMR
	.010 .... = Type of Request(T): 2
	.... 0000 = D: WB 5.9 kbps (VBR) (0)
	TOC # 1
	0... .... = Header Type identification bit (H): ToC
	.0.. .... = F: Last frame in payload
	..0. .... = EVS Mode: 0
	...0 .... = Unused: 0
	.... 0001 = EVS mode and bit rate: Primary 7.2 kbps (1)
Speech frame for TOC # 1
	Voice data: 60309c4da25a45f1a8994f2d38bcbe111fdc
*/

static uint8_t ex_vevs[] = { 0xa0, 0x1, 0x60, 0x30, 0x9c, 0x4d, 0xa2, 0x5a,
 							0x45, 0xf1, 0xa8, 0x99, 0x4f, 0x2d, 0x38, 0xbc,
							0xbe, 0x11, 0x1f, 0xdc};

static struct ast_frame *vevs_sample(void)
{
	static struct ast_frame f = {
		.frametype = AST_FRAME_VOICE,
		.datalen = sizeof(ex_vevs),
		.samples = 320,
		.mallocd = 0,
		.offset = 0,
		.src = __PRETTY_FUNCTION__,
		.data.ptr = ex_vevs,
	};

	f.subclass.format = ast_format_vevs;

	return &f;
}
