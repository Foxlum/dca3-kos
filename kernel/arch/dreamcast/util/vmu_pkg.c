/* KallistiOS ##version##

   vmu_pkg.c
   (c)2002 Dan Potter
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dc/vmu_pkg.h>

/*

VMU data files can be stored raw, but if you want to interact with the
rest of the DC world, it's much better to package them in a nice data
file format. This module takes care of that for you.

Thanks to Marcus Comstedt for this information.

*/

/* CRC calculation: calculates the CRC on a VMU file to be written out */
static int vmu_pkg_crc(const uint8 * buf, int size) {
	int	i, c, n;

	for (i=0, n=0; i<size; i++) {
		n ^= (buf[i] << 8);
		for (c=0; c<8; c++) {
			if (n & 0x8000)
				n = (n << 1) ^ 4129;
			else
				n = (n << 1);
		}
	}

	return n & 0xffff;
}

static int vmu_eyecatch_size(int eyecatch_type) {
	switch(eyecatch_type) {
		case VMUPKG_EC_NONE:
			return 0;
		case VMUPKG_EC_16BIT:
			return 72 * 56 * 2;
		case VMUPKG_EC_256COL:
			return 512 + 72*56;
		case VMUPKG_EC_16COL:
			return 32 + 72*56/2;
		default:
			return -1;
	}
}

/* Converts a vmu_pkg_t structure into an array of uint8's which may be
   written to a VMU file via fs_vmu, or whatever. */
int vmu_pkg_build(vmu_pkg_t *src, uint8 ** dst, int * dst_size) {
	uint8		*out;
	int		ec_size, out_size;
	vmu_hdr_t	*hdr;

	/* First off, figure out how big it will be */
	out_size = sizeof(vmu_hdr_t) + 512 * src->icon_cnt + src->data_len;
	ec_size = vmu_eyecatch_size(src->eyecatch_type);
	if (ec_size < 0) return -1;

	out_size += ec_size;
	*dst_size = out_size;

	/* Allocate a return array */
	out = *dst = malloc(out_size);

	/* Setup some defaults */
	memset(out, 0, out_size);
	hdr = (vmu_hdr_t *)out;
	memset(hdr->desc_short, 32, sizeof(hdr->desc_short));
	memset(hdr->desc_long, 32, sizeof(hdr->desc_long));

	/* Fill in the data from the pkg struct */
	memcpy(hdr->desc_short, src->desc_short, strlen(src->desc_short));
	memcpy(hdr->desc_long, src->desc_long, strlen(src->desc_long));
	strcpy(hdr->app_id, src->app_id);
	hdr->icon_cnt = src->icon_cnt;
	hdr->icon_anim_speed = src->icon_anim_speed;
	hdr->eyecatch_type = src->eyecatch_type;
	hdr->crc = 0;
	hdr->data_len = src->data_len;
	memcpy(hdr->icon_pal, src->icon_pal, sizeof(src->icon_pal));
	out += sizeof(vmu_hdr_t);

	memcpy(out, src->icon_data, 512 * src->icon_cnt);
	out += 512 * src->icon_cnt;

	memcpy(out, src->eyecatch_data, ec_size);
	out += ec_size;

	memcpy(out, src->data, src->data_len);
	out += src->data_len;

	/* Verify the size */
	assert( (out - *dst) == out_size );
	out = *dst;

	/* Calculate CRC */
	hdr->crc = vmu_pkg_crc(out, out_size);

	return 0;
}

/* Parse an array of uint8's (i.e. a VMU data file) into a
 * vmu_pkg_t package structure. */
int vmu_pkg_parse(uint8 *data, vmu_pkg_t *pkg) {
	uint16 crc, crc_save;
	int ec_size, hdr_size, total_size, icon_size;
	vmu_hdr_t *hdr;

	hdr = (vmu_hdr_t *)data;

	icon_size = 512 * hdr->icon_cnt;
	ec_size = vmu_eyecatch_size(hdr->eyecatch_type);
	hdr_size = sizeof(vmu_hdr_t) + icon_size + ec_size;
	total_size = hdr->data_len + hdr_size;

	/* Verify the CRC.  Note: this writes a zero byte into data[].
	 * The byte is later restored in case data is an mmapped pointer to a
	 * writable file, and a new header is not generated by the user. */
	crc_save = hdr->crc;
	hdr->crc = 0;
	crc = vmu_pkg_crc(data, total_size);
	hdr->crc = crc_save;
	if (crc_save != crc) {
		dbglog(DBG_ERROR, "vmu_pkg_parse: expected CRC %04x, got %04x\n", crc_save, crc);
		return -1;
	}

	/* Fill in pkg struct for caller. */
	pkg->icon_cnt = hdr->icon_cnt;
	pkg->icon_anim_speed = hdr->icon_anim_speed;
	pkg->eyecatch_type = hdr->eyecatch_type;
	pkg->data_len = hdr->data_len;
	memcpy(pkg->icon_pal, hdr->icon_pal, sizeof(hdr->icon_pal));
	pkg->icon_data = data + sizeof(vmu_hdr_t);
	pkg->eyecatch_data = data + sizeof(vmu_hdr_t) + icon_size;
	pkg->data = data + hdr_size;
	/* Copy space and null-padded fields, keeping the padding,
	 * and ensure our representations are null-terminated. */
	memcpy(pkg->desc_short, hdr->desc_short, sizeof(hdr->desc_short));
	memcpy(pkg->desc_long, hdr->desc_long, sizeof(hdr->desc_long));
	memcpy(pkg->app_id, hdr->app_id, sizeof(hdr->app_id));
	*(pkg->desc_short + sizeof(hdr->desc_short)) = '\0';
	*(pkg->desc_long  + sizeof(hdr->desc_long))  = '\0';
	*(pkg->app_id     + sizeof(hdr->app_id))     = '\0';

	return 0;
}
