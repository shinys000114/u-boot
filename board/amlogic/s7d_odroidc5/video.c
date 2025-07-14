#include <common.h>
#include <string.h>
#include <fdtdec.h>
#include <env.h>
#include <blk.h>

#include <amlogic/media/vout/hdmitx21/hdmitx_module.h>
#include <amlogic/media/vout/hdmitx21/hdmi_common.h>

extern const struct hdmi_timing *hdmitx21_gettiming_from_name(const char *name);
extern void hdmi_timing_from_display_timing(struct hdmi_timing *ht, struct display_timing *dt);
extern void hdmi_timing_printdebug(const struct hdmi_timing *ht);
extern int load_from_mmc(unsigned long addr, int devnum, int partnum, const char *filename);
extern struct blk_desc *amlogic_get_bootdev(void);

#define DRM_DISPLAY_MODE_LEN		32
#define DISPLAY_TIMING_FILENAME		"display-timings.txt"

static int gcd(int m, int n)
{
	int t;

	while (m > 0) {
		if (n > m) {
			t = m;
			m = n;
			n = t;
		} /* swap */
		m -= n;
	}
	return n;
}

/*
 * Parse decimal string into fixed-point interger
 *
 * example:
 *    parse_string_to_fixed("12.345", 3) -> 12345
 *    parse_string_to_fixed("12.345", 6) -> 12345000
 */
static unsigned int parse_string_to_fixed(char *str, int digits)
{
	unsigned int ipart;
	unsigned int fpart = 0;
	unsigned int len;
	char buf[4] = { 0, };
	char *p;

	if (!str || digits == 0)
		return 0;

	p = memscan(str, '.', strlen(str));
	if (*p == '.') {
		*p++ = '\0';

		len = strnlen(p, digits);
		memcpy(buf, p, len);

		fpart = simple_strtoul(buf, NULL, 10);
		if (fpart) {
			for (; len < digits; len++)
				fpart *= 10;
		}
	}

	ipart = simple_strtoul(str, NULL, 10);
	if (ipart) {
		for (len = 0; len < digits; len++)
			ipart *= 10;
	}

	return ipart + fpart;
}

int hdmi_timing_from_modeline(const char *m, struct hdmi_timing *ht)
{
	char *str;
	char *argv[32];
	int argc = 0;
	int pos = 0;
	char name[DRM_DISPLAY_MODE_LEN];
	int sync_start;
	int sync_end;
	int div;
	char *modeline = strdup(m);

	if (!ht)
		return -EINVAL;

	/* Empty string */
	if (!modeline || strlen(modeline) == 0)
		return -EINVAL;

	/* skip spaces */
	while ((*modeline == ' ') || (*modeline == '\t'))
		modeline++;

	/* Maybe comment line */
	if (*modeline == '#')
		return -EINVAL;

	while ((str = strsep(&modeline, " ,\t")) != NULL)
		argv[argc++] = str;

	/* May start with 'Modeline' */
	if (!strcasecmp(argv[pos], "modeline"))
		pos++;

	/* Video timing name */
	if (argv[pos][0] == '"') {
		char *end;

		strncpy(name, argv[pos] + 1, DRM_DISPLAY_MODE_LEN);

		end = strchr(name, '"');
		if (end)
			*end = '\0';
	} else {
		strncpy(name, argv[pos], DRM_DISPLAY_MODE_LEN);
	}

	name[DRM_DISPLAY_MODE_LEN - 1] = '\0';

	ht->name = strdup(name);
	ht->sname = NULL;

	pos++;

	/* The number of rest tokens must be 9 at least */
	if ((argc - pos) < 9)
		return -EINVAL;

	/* Pixel clock */
	ht->pixel_freq = parse_string_to_fixed(argv[pos++], 3);

	/* Horizontal timing */
	ht->h_active = simple_strtoul(argv[pos++], NULL, 10);
	sync_start = simple_strtoul(argv[pos++], NULL, 10);
	sync_end = simple_strtoul(argv[pos++], NULL, 10);
	ht->h_total = simple_strtoul(argv[pos++], NULL, 10);
	ht->h_blank = ht->h_total - ht->h_active;
	ht->h_front = sync_start - ht->h_active;
	ht->h_sync = sync_end - sync_start;
	ht->h_back = ht->h_total - sync_end;

	/* Vertical timing */
	ht->v_active = simple_strtoul(argv[pos++], NULL, 10);
	sync_start = simple_strtoul(argv[pos++], NULL, 10);
	sync_end = simple_strtoul(argv[pos++], NULL, 10);
	ht->v_total = simple_strtoul(argv[pos++], NULL, 10);
	ht->v_blank = ht->v_total - ht->v_active;
	ht->v_front = sync_start - ht->v_active;
	ht->v_sync = sync_end - sync_start;
	ht->v_back = ht->v_total - sync_end;

	div = gcd(ht->h_active, ht->v_active);
	ht->h_pict = ht->h_active / div;
	ht->v_pict = ht->v_active / div;

	ht->h_freq = (ht->pixel_freq * 1000) / ht->h_total;
	ht->v_freq = (ht->h_freq * 1000) / ht->v_total;

	/* H/V sync polarity & scan mode */
	while (pos < argc) {
		char *str = argv[pos++];

		if (!strcasecmp(str + 1, "hsync"))
			ht->h_pol = (str[0] == '+');
		else if (!strcasecmp(str + 1, "vsync"))
			ht->v_pol = (str[0] == '+');
		else if (!strcasecmp(str, "interlaced"))
			ht->pi_mode = 0;
		else if (!strcasecmp(str, "progressive"))
			ht->pi_mode = 1;
	}

	// FIXME: not being used in HDMI driver
	ht->v_sync_ln = 1;
	ht->h_pixel = 1;
	ht->v_pixel = 1;

	return 0;
}

void hdmi_timing_from_display_timing(struct hdmi_timing *ht, struct display_timing *dt)
{
	int div;

	ht->sname = NULL;

	ht->pixel_freq = dt->pixelclock.typ / 1000;

	ht->h_active = dt->hactive.typ;
	ht->h_front = dt->hfront_porch.typ;
	ht->h_sync = dt->hsync_len.typ;
	ht->h_back = dt->hback_porch.typ;
	ht->h_blank = ht->h_front + ht->h_sync + ht->h_back;
	ht->h_total = ht->h_active + ht->h_blank;

	ht->v_active = dt->vactive.typ;
	ht->v_front = dt->vfront_porch.typ;
	ht->v_sync = dt->vsync_len.typ;
	ht->v_back = dt->vback_porch.typ;
	ht->v_blank = ht->v_front + ht->v_sync + ht->v_back;
	ht->v_total = ht->v_active + ht->v_blank;

	ht->h_pol = !!(dt->flags & DISPLAY_FLAGS_HSYNC_HIGH);
	ht->v_pol = !!(dt->flags & DISPLAY_FLAGS_VSYNC_HIGH);
	ht->pi_mode = !(dt->flags & DISPLAY_FLAGS_INTERLACED);

	div = gcd(ht->h_active, ht->v_active);
	ht->h_pict = ht->h_active / div;
	ht->v_pict = ht->v_active / div;

	ht->h_freq = (ht->pixel_freq * 1000) / ht->h_total;
	ht->v_freq = (ht->h_freq * 1000) / ht->v_total;

	// FIXME: not being used in HDMI driver
	ht->v_sync_ln = 1;
	ht->h_pixel = 1;
	ht->v_pixel = 1;
}

int lookup_display_timings_node(struct fdt_header *fdt, const char *displaymode,
			struct display_timing *timing)
{
	int index = 0;
	int nodeoffset;
	const struct fdt_property *fdt_prop;
	char data[1024];
	int len;
	char *pos;

	if (!displaymode || !timing)
		return -EINVAL;

	nodeoffset = fdt_path_offset(fdt, "/__symbols__");
	if (nodeoffset <= 0)
		return -EINVAL;

	for (nodeoffset = fdt_first_property_offset(fdt, nodeoffset);
			nodeoffset > 0; nodeoffset = fdt_next_property_offset(fdt, nodeoffset)) {
		fdt_prop = fdt_offset_ptr(fdt, nodeoffset, sizeof(*fdt_prop));
		len = fdt32_to_cpu(fdt_prop->len);
		if (len <= 0)
			continue;

		memcpy(data, fdt_prop->data, len);
		data[len] = '\0';

		pos = strstr(data, "display-timings");
		if (!pos)
			continue;

		pos = strstr(pos, displaymode);
		if (pos) {
			const char *path = "/fragment@0/__overlay__";
			int nodeoffset;

			nodeoffset = fdt_path_offset(fdt, path);
			if (nodeoffset <= 0)
				break;

			fdtdec_decode_display_timing(fdt, nodeoffset, index, timing);

			return 0;
		}

		index++;
	}

	return -ENODEV;
}

const struct hdmi_timing *load_hdmi_timing_from_dt(
		struct blk_desc *dev_desc, const char *displaymode)
{
	const char *fk_kvers = "5.15.153-odroid-arm64";
	char path[4096];

	unsigned long addr = env_get_ulong("loadaddr", 16, 0);
	struct display_timing timing;
	struct hdmi_timing hdmi_timing;
	int ret;

	if (!displaymode)
		return NULL;

	/* Linux */
	snprintf(path, sizeof(path),
			"dtbs/%s/amlogic/overlays/odroidc5/display-timings.dtbo",
			fk_kvers);
	ret = load_from_mmc(addr, dev_desc->devnum, 1, path);
	if (ret) {
		/* Android */
		strcpy(path, "amlogic/overlays/ODROID-C5/display-timings.dtbo");
		ret = load_from_mmc(addr, dev_desc->devnum, 2, path);
	}
	if (ret)
		return NULL;

	ret = lookup_display_timings_node((struct fdt_header *)addr,
			displaymode, &timing);
	if (ret == 0) {
		struct hdmi_timing *ht = (struct hdmi_timing *)
			hdmitx21_gettiming_from_vic(HDMI_ODROID_CUSTOM_VIDEO);
		if (ht) {
			hdmi_timing_from_display_timing(&hdmi_timing, &timing);
			memcpy(ht, &hdmi_timing, sizeof(struct hdmi_timing));
			ht->vic = HDMI_ODROID_CUSTOM_VIDEO;
			ht->name = (char *)displaymode;

			return ht;
		}
	}

	return NULL;
}

const struct hdmi_timing *set_odroid_custom_video(struct hdmi_timing *hdmi_timing)
{
	struct hdmi_timing *ht = NULL;

	if (hdmi_timing) {
		ht = (struct hdmi_timing *)hdmitx21_gettiming_from_vic(
				HDMI_ODROID_CUSTOM_VIDEO);
		if (ht) {
			memcpy(ht, hdmi_timing, sizeof(struct hdmi_timing));
			ht->vic = HDMI_ODROID_CUSTOM_VIDEO;
		}
	}

	return ht;
}

const struct hdmi_timing *load_hdmi_timing_from_string(
		const char *modeline, const char *mode)
{
	struct hdmi_timing hdmi_timing;
	int ret;

	if (!modeline || !mode)
		return NULL;

	ret = hdmi_timing_from_modeline(modeline, &hdmi_timing);
	if (ret)
		return NULL;

	if (!strcmp(hdmi_timing.name, mode))
		return set_odroid_custom_video(&hdmi_timing);

	return NULL;
}

const struct hdmi_timing *load_hdmi_timing_from_modeline(
		struct blk_desc *dev_desc, const char *displaymode)
{
	const struct hdmi_timing *ht = NULL;
	unsigned long addr = env_get_ulong("loadaddr", 16, 0);
	int len;
	char *buf = (char *)addr;
	char *str;
	int ret;

	/* read from the root directory in the first partition */
	ret = load_from_mmc(addr, dev_desc->devnum, 1, DISPLAY_TIMING_FILENAME);
	if (ret)
		ret = load_from_mmc(addr, dev_desc->devnum, 2,
				"/usr/lib/firmware/"DISPLAY_TIMING_FILENAME);
	if (ret)
		return NULL;

	/* convert raw file text to string by adding null character */
	len = env_get_ulong("filesize", 10, 0);
	*(char *)(addr + len - 1) = '\0';

	/* traverse lines to find out expected modeline */
	while ((str = strsep(&buf, "\n")) != NULL) {
		ht = load_hdmi_timing_from_string(str, displaymode);
		if (ht)
			break;
	}

	return ht;
}

const struct hdmi_timing *lookup_hdmi_timing_by_name(const char *mode)
{
	const struct hdmi_timing *ht;
	struct blk_desc *dev_desc = amlogic_get_bootdev();

	if (!mode)
		return NULL;

	/* lookup from internal video timings */
	ht = (struct hdmi_timing *)hdmitx21_gettiming_from_name(mode);
	if (!ht) {
		/* look up from video timing string */
		ht = load_hdmi_timing_from_string(
				env_get("video-timing"), mode);
	}

	/* look up from video timing from device tree */
	if (!ht)
		ht = load_hdmi_timing_from_dt(dev_desc, mode);

	/* look up from video timing from modeline text file */
	if (!ht)
		ht = load_hdmi_timing_from_modeline(dev_desc, mode);

	hdmi_timing_printdebug(ht);

	return ht;
}

void hdmi_timing_printdebug(const struct hdmi_timing *ht)
{
	if (!ht)
		return;

	printf("hdmi_timing : vic=%d, name=%s, sname=%s\n"
			"\t\tpixel_freq=%d, pi_mode=%d, h_pol=%d, v_pol=%d\n"
			"\t\th_active=%d, h_front=%d, h_sync=%d, h_back=%d, h_blank=%d, h_total=%d\n"
			"\t\tv_active=%d, v_front=%d, v_sync=%d, v_back=%d, v_blank=%d, v_total=%d\n"
			"\t\th_freq=%d, v_freq=%d, pict=%dx%d, pixel=%dx%d, vsync_ln=%d\n",
			ht->vic, ht->name, ht->sname,
			ht->pixel_freq, ht->pi_mode, ht->h_pol, ht->v_pol,
			ht->h_active, ht->h_front, ht->h_sync, ht->h_back, ht->h_blank, ht->h_total,
			ht->v_active, ht->v_front, ht->v_sync, ht->v_back, ht->v_blank, ht->v_total,
			ht->h_freq, ht->v_freq, ht->h_pict, ht->v_pict,
			ht->h_pixel, ht->v_pixel, ht->v_sync_ln);
}
