/*
 * Copyright (C) 2010 Bluecherry, LLC www.bluecherrydvr.com
 * Copyright (C) 2010 Ben Collins <bcollins@bluecherry.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/font.h>
#include <linux/bitrev.h>

#include "solo6010.h"

#define CAPTURE_MAX_BANDWIDTH		32	// D1 4channel (D1 == 4)

#define VI_PROG_HSIZE			(1280 - 16)
#define VI_PROG_VSIZE			(1024 - 16)

#define IRQ_LEVEL			2

static void solo_capture_config(struct solo6010_dev *solo_dev)
{
	unsigned long height;
	unsigned long width;
	void *buf;
	int i;

	solo_reg_write(solo_dev, SOLO_CAP_BASE,
		       SOLO_CAP_MAX_PAGE(SOLO_CAP_EXT_MAX_PAGE *
					 solo_dev->nr_chans) |
		       SOLO_CAP_BASE_ADDR(SOLO_CAP_EXT_ADDR(solo_dev) >> 16));
	solo_reg_write(solo_dev, SOLO_CAP_BTW,
		       (1 << 17) | SOLO_CAP_PROG_BANDWIDTH(2) |
		       SOLO_CAP_MAX_BANDWIDTH(CAPTURE_MAX_BANDWIDTH));

	/* Set scale 1, 9 dimension */
	width = solo_dev->video_hsize;
	height = solo_dev->video_vsize;
	solo_reg_write(solo_dev, SOLO_DIM_SCALE1,
		       SOLO_DIM_H_MB_NUM(width / 16) |
		       SOLO_DIM_V_MB_NUM_FRAME(height / 8) |
		       SOLO_DIM_V_MB_NUM_FIELD(height / 16));

	/* Set scale 2, 10 dimension */
	width = solo_dev->video_hsize / 2;
	height = solo_dev->video_vsize;
	solo_reg_write(solo_dev, SOLO_DIM_SCALE2,
		       SOLO_DIM_H_MB_NUM(width / 16) |
		       SOLO_DIM_V_MB_NUM_FRAME(height / 8) |
		       SOLO_DIM_V_MB_NUM_FIELD(height / 16));

	/* Set scale 3, 11 dimension */
	width = solo_dev->video_hsize / 2;
	height = solo_dev->video_vsize / 2;
	solo_reg_write(solo_dev, SOLO_DIM_SCALE3,
		       SOLO_DIM_H_MB_NUM(width / 16) |
		       SOLO_DIM_V_MB_NUM_FRAME(height / 8) |
		       SOLO_DIM_V_MB_NUM_FIELD(height / 16));

	/* Set scale 4, 12 dimension */
	width = solo_dev->video_hsize / 3;
	height = solo_dev->video_vsize / 3;
	solo_reg_write(solo_dev, SOLO_DIM_SCALE4,
		       SOLO_DIM_H_MB_NUM(width / 16) |
		       SOLO_DIM_V_MB_NUM_FRAME(height / 8) |
		       SOLO_DIM_V_MB_NUM_FIELD(height / 16));

	/* Set scale 5, 13 dimension */
	width = solo_dev->video_hsize / 4;
	height = solo_dev->video_vsize / 2;
	solo_reg_write(solo_dev, SOLO_DIM_SCALE5,
		       SOLO_DIM_H_MB_NUM(width / 16) |
		       SOLO_DIM_V_MB_NUM_FRAME(height / 8) |
		       SOLO_DIM_V_MB_NUM_FIELD(height / 16));

	/* Progressive */
	width = VI_PROG_HSIZE;
	height = VI_PROG_VSIZE;
	solo_reg_write(solo_dev, SOLO_DIM_PROG,
		       SOLO_DIM_H_MB_NUM(width / 16) |
		       SOLO_DIM_V_MB_NUM_FRAME(height / 16) |
		       SOLO_DIM_V_MB_NUM_FIELD(height / 16));

	/* Clear OSD */
	solo_reg_write(solo_dev, SOLO_VE_OSD_CH, 0);
	solo_reg_write(solo_dev, SOLO_VE_OSD_BASE,
		       SOLO_EOSD_EXT_ADDR(solo_dev) >> 16);
	solo_reg_write(solo_dev, SOLO_VE_OSD_CLR,
		       0xF0 << 16 | 0x80 << 8 | 0x80);

	if (solo_dev->type == SOLO_DEV_6010)
		solo_reg_write(solo_dev, SOLO_VE_OSD_OPT,
			       SOLO_VE_OSD_H_SHADOW | SOLO_VE_OSD_V_SHADOW);
	else
		solo_reg_write(solo_dev, SOLO_VE_OSD_OPT, SOLO_VE_OSD_V_DOUBLE |
			       SOLO_VE_OSD_H_SHADOW | SOLO_VE_OSD_V_SHADOW);

	/* Clear OSG buffer */
	buf = kzalloc(SOLO_EOSD_EXT_SIZE(solo_dev), GFP_KERNEL);
	if (!buf)
		return;

	for (i = 0; i < solo_dev->nr_chans; i++) {
		solo_p2m_dma(solo_dev, 1, buf,
			     SOLO_EOSD_EXT_ADDR(solo_dev) +
			     (SOLO_EOSD_EXT_SIZE(solo_dev) * i),
			     SOLO_EOSD_EXT_SIZE(solo_dev), 0, 0);
	}
	kfree(buf);
}

/* Should be called with enable_lock held */
int solo_osd_print(struct solo_enc_dev *solo_enc)
{
	struct solo6010_dev *solo_dev = solo_enc->solo_dev;
	unsigned char *str = solo_enc->osd_text;
	u8 *buf = solo_enc->osd_buf;
	u32 reg = solo_reg_read(solo_dev, SOLO_VE_OSD_CH);
	const struct font_desc *vga = find_font("VGA8x16");
	const unsigned char *vga_data;
	int len;
	int i, j;

	if (WARN_ON_ONCE(!vga))
		return -ENODEV;

	len = strlen(str);

	if (len == 0) {
		/* Disable OSD on this channel */
		reg &= ~(1 << solo_enc->ch);
		solo_reg_write(solo_dev, SOLO_VE_OSD_CH, reg);
		return 0;
	}

	memset(buf, 0, SOLO_EOSD_EXT_SIZE_MAX);
	vga_data = (const unsigned char *)vga->data;

	for (i = 0; i < len; i++) {
		unsigned char c = str[i];

		for (j = 0; j < 16; j++) {
			buf[(j * 2) + (i % 2) + (i / 2 * 32)] =
				bitrev8(vga_data[(c * 16) + j]);
		}
	}

	solo_p2m_dma(solo_dev, 1, buf,
		     SOLO_EOSD_EXT_ADDR(solo_dev) +
		     (solo_enc->ch * SOLO_EOSD_EXT_SIZE(solo_dev)),
		     SOLO_EOSD_EXT_SIZE(solo_dev), 0, 0);

	/* Enable OSD on this channel */
        reg |= (1 << solo_enc->ch);
        solo_reg_write(solo_dev, SOLO_VE_OSD_CH, reg);

	return 0;
}

/* qp : 0 ~ 3 */
void solo_s_jpeg_qp(struct solo6010_dev *solo_dev, int channel, int qp)
{
	int shift;

	if (solo_dev->type == SOLO_DEV_6010)
		return;

	if((channel > 31) || (qp > 3))
		return;

	spin_lock(&solo_dev->jpeg_qp_lock);
	if(channel < 16)
	{
		shift = channel * 2;
		solo_dev->jpeg_qp[0] &= ~(3<<shift);
		solo_dev->jpeg_qp[0] |= (qp & 3)<<shift;
		solo_reg_write(solo_dev, SOLO_VE_JPEG_QP_CH_L, solo_dev->jpeg_qp[0]);
	} else {
		shift = (channel - 16) * 2;
		solo_dev->jpeg_qp[1] &= ~(3<<shift);
		solo_dev->jpeg_qp[1] |= (qp & 3)<<shift;
		solo_reg_write(solo_dev, SOLO_VE_JPEG_QP_CH_H, solo_dev->jpeg_qp[1]);
	}
	spin_unlock(&solo_dev->jpeg_qp_lock);
}

int solo_g_jpeg_qp(struct solo6010_dev *solo_dev, int channel)
{
	int shift;
	int qp;

	if (solo_dev->type == SOLO_DEV_6010)
		return 2;
	
	if(channel > 31)
		return 0;

	if(channel < 16)
	{
		shift = channel * 2;
		qp = (solo_dev->jpeg_qp[0]>>shift) & 3;
	} else {
		shift = (channel - 16) * 2;
		qp = (solo_dev->jpeg_qp[1]>>shift) & 3;
	}

	return qp;
}

static void solo_jpeg_config(struct solo6010_dev *solo_dev)
{
	if (solo_dev->type == SOLO_DEV_6010) {
		solo_reg_write(solo_dev, SOLO_VE_JPEG_QP_TBL,
			(2 << 24) | (2 << 16) | (2 << 8) | 2);
	} else {
		solo_reg_write(solo_dev, SOLO_VE_JPEG_QP_TBL,
			(4 << 24) | (3 << 16) | (2 << 8) | 1);
	}

	solo_dev->jpeg_qp[0] = 0;
	solo_dev->jpeg_qp[1] = 0;
	spin_lock_init(&solo_dev->jpeg_qp_lock);
	solo_reg_write(solo_dev, SOLO_VE_JPEG_QP_CH_L, solo_dev->jpeg_qp[0]);
	solo_reg_write(solo_dev, SOLO_VE_JPEG_QP_CH_H, solo_dev->jpeg_qp[1]);
	solo_reg_write(solo_dev, SOLO_VE_JPEG_CFG,
		(SOLO_JPEG_EXT_SIZE(solo_dev) & 0xffff0000) |
		((SOLO_JPEG_EXT_ADDR(solo_dev) >> 16) & 0x0000ffff));
	solo_reg_write(solo_dev, SOLO_VE_JPEG_CTRL, 0xffffffff);
}

static void solo_mp4e_config(struct solo6010_dev *solo_dev)
{
	int i;

	solo_reg_write(solo_dev, SOLO_VE_CFG0,
		       SOLO_VE_INTR_CTRL(IRQ_LEVEL) |
		       SOLO_VE_BLOCK_SIZE(SOLO_MP4E_EXT_SIZE(solo_dev) >> 16) |
		       SOLO_VE_BLOCK_BASE(SOLO_MP4E_EXT_ADDR(solo_dev) >> 16));

	if (solo_dev->type == SOLO_DEV_6010) {
		solo_reg_write(solo_dev, SOLO_VE_CFG1,
			       SOLO_VE_BYTE_ALIGN(2) |
			       SOLO_VE_INSERT_INDEX | SOLO_VE_MOTION_MODE(0));
	} else {
		solo_reg_write(solo_dev, SOLO_VE_CFG1,
		  (SOLO_VE_MPEG_SIZE_H(SOLO_MP4E_EXT_SIZE(solo_dev) >> 24) & 0x0f) |
		  (SOLO_VE_JPEG_SIZE_H(SOLO_JPEG_EXT_SIZE(solo_dev) >> 24) & 0x0f) |
		  SOLO_VE_BYTE_ALIGN(2) | SOLO_VE_INSERT_INDEX |
		  SOLO_VE_MOTION_MODE(0));
	}

	solo_reg_write(solo_dev, SOLO_VE_WMRK_POLY, 0);
	solo_reg_write(solo_dev, SOLO_VE_VMRK_INIT_KEY, 0);
	solo_reg_write(solo_dev, SOLO_VE_WMRK_STRL, 0);
	if (solo_dev->type == SOLO_DEV_6110)
		solo_reg_write(solo_dev, SOLO_VE_WMRK_ENABLE, 0);
	solo_reg_write(solo_dev, SOLO_VE_ENCRYP_POLY, 0);
	solo_reg_write(solo_dev, SOLO_VE_ENCRYP_INIT, 0);

	solo_reg_write(solo_dev, SOLO_VE_ATTR,
		       SOLO_VE_LITTLE_ENDIAN |
		       SOLO_COMP_ATTR_FCODE(1) |
		       SOLO_COMP_TIME_INC(0) |
		       SOLO_COMP_TIME_WIDTH(15) |
		       SOLO_DCT_INTERVAL(solo_dev->type == SOLO_DEV_6010 ? 9 : 10));

	for (i = 0; i < solo_dev->nr_chans; i++) {
		solo_reg_write(solo_dev, SOLO_VE_CH_REF_BASE(i),
			       (SOLO_EREF_EXT_ADDR(solo_dev) +
			       (i * SOLO_EREF_EXT_SIZE)) >> 16);
		solo_reg_write(solo_dev, SOLO_VE_CH_REF_BASE_E(i),
			       (SOLO_EREF_EXT_ADDR(solo_dev) +
			       ((i + 16) * SOLO_EREF_EXT_SIZE)) >> 16);
	}

	if (solo_dev->type == SOLO_DEV_6110) {
		solo_reg_write(solo_dev, SOLO_VE_COMPT_MOT, 0x00040008);
	} else {
		for (i = 0; i < solo_dev->nr_chans; i++)
			solo_reg_write(solo_dev, SOLO_VE_CH_MOT(i), 0x100);
	}
}

int solo_enc_init(struct solo6010_dev *solo_dev)
{
	int i;

	solo_capture_config(solo_dev);
	solo_mp4e_config(solo_dev);
	solo_jpeg_config(solo_dev);

	for (i = 0; i < solo_dev->nr_chans; i++) {
		solo_reg_write(solo_dev, SOLO_CAP_CH_SCALE(i), 0);
		solo_reg_write(solo_dev, SOLO_CAP_CH_COMP_ENA_E(i), 0);
	}

	return 0;
}

void solo_enc_exit(struct solo6010_dev *solo_dev)
{
	int i;

	for (i = 0; i < solo_dev->nr_chans; i++) {
		solo_reg_write(solo_dev, SOLO_CAP_CH_SCALE(i), 0);
		solo_reg_write(solo_dev, SOLO_CAP_CH_COMP_ENA_E(i), 0);
	}
}
