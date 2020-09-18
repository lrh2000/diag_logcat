#include <stdio.h>
#include <malloc.h>
#include <stdint.h>

struct stamp_log_t {
	uint64_t offset;
	uint64_t stamp;
};

static FILE *data_fp, *stamp_fp, *out_fp;
static char *data, *out_start, *out_current, *out_end;
static struct stamp_log_t *stamps;
static size_t data_len, nr_stamps, out_remained;

static uint16_t calc_crc(const uint8_t *data, size_t len)
{
	static const uint16_t table[256] = {
		0x0000U, 0x1189U, 0x2312U, 0x329BU, 0x4624U, 0x57ADU, 0x6536U, 0x74BFU,
		0x8C48U, 0x9DC1U, 0xAF5AU, 0xBED3U, 0xCA6CU, 0xDBE5U, 0xE97EU, 0xF8F7U,
		0x1081U, 0x0108U, 0x3393U, 0x221AU, 0x56A5U, 0x472CU, 0x75B7U, 0x643EU,
		0x9CC9U, 0x8D40U, 0xBFDBU, 0xAE52U, 0xDAEDU, 0xCB64U, 0xF9FFU, 0xE876U,
		0x2102U, 0x308BU, 0x0210U, 0x1399U, 0x6726U, 0x76AFU, 0x4434U, 0x55BDU,
		0xAD4AU, 0xBCC3U, 0x8E58U, 0x9FD1U, 0xEB6EU, 0xFAE7U, 0xC87CU, 0xD9F5U,
		0x3183U, 0x200AU, 0x1291U, 0x0318U, 0x77A7U, 0x662EU, 0x54B5U, 0x453CU,
		0xBDCBU, 0xAC42U, 0x9ED9U, 0x8F50U, 0xFBEFU, 0xEA66U, 0xD8FDU, 0xC974U,
		0x4204U, 0x538DU, 0x6116U, 0x709FU, 0x0420U, 0x15A9U, 0x2732U, 0x36BBU,
		0xCE4CU, 0xDFC5U, 0xED5EU, 0xFCD7U, 0x8868U, 0x99E1U, 0xAB7AU, 0xBAF3U,
		0x5285U, 0x430CU, 0x7197U, 0x601EU, 0x14A1U, 0x0528U, 0x37B3U, 0x263AU,
		0xDECDU, 0xCF44U, 0xFDDFU, 0xEC56U, 0x98E9U, 0x8960U, 0xBBFBU, 0xAA72U,
		0x6306U, 0x728FU, 0x4014U, 0x519DU, 0x2522U, 0x34ABU, 0x0630U, 0x17B9U,
		0xEF4EU, 0xFEC7U, 0xCC5CU, 0xDDD5U, 0xA96AU, 0xB8E3U, 0x8A78U, 0x9BF1U,
		0x7387U, 0x620EU, 0x5095U, 0x411CU, 0x35A3U, 0x242AU, 0x16B1U, 0x0738U,
		0xFFCFU, 0xEE46U, 0xDCDDU, 0xCD54U, 0xB9EBU, 0xA862U, 0x9AF9U, 0x8B70U,
		0x8408U, 0x9581U, 0xA71AU, 0xB693U, 0xC22CU, 0xD3A5U, 0xE13EU, 0xF0B7U,
		0x0840U, 0x19C9U, 0x2B52U, 0x3ADBU, 0x4E64U, 0x5FEDU, 0x6D76U, 0x7CFFU,
		0x9489U, 0x8500U, 0xB79BU, 0xA612U, 0xD2ADU, 0xC324U, 0xF1BFU, 0xE036U,
		0x18C1U, 0x0948U, 0x3BD3U, 0x2A5AU, 0x5EE5U, 0x4F6CU, 0x7DF7U, 0x6C7EU,
		0xA50AU, 0xB483U, 0x8618U, 0x9791U, 0xE32EU, 0xF2A7U, 0xC03CU, 0xD1B5U,
		0x2942U, 0x38CBU, 0x0A50U, 0x1BD9U, 0x6F66U, 0x7EEFU, 0x4C74U, 0x5DFDU,
		0xB58BU, 0xA402U, 0x9699U, 0x8710U, 0xF3AFU, 0xE226U, 0xD0BDU, 0xC134U,
		0x39C3U, 0x284AU, 0x1AD1U, 0x0B58U, 0x7FE7U, 0x6E6EU, 0x5CF5U, 0x4D7CU,
		0xC60CU, 0xD785U, 0xE51EU, 0xF497U, 0x8028U, 0x91A1U, 0xA33AU, 0xB2B3U,
		0x4A44U, 0x5BCDU, 0x6956U, 0x78DFU, 0x0C60U, 0x1DE9U, 0x2F72U, 0x3EFBU,
		0xD68DU, 0xC704U, 0xF59FU, 0xE416U, 0x90A9U, 0x8120U, 0xB3BBU, 0xA232U,
		0x5AC5U, 0x4B4CU, 0x79D7U, 0x685EU, 0x1CE1U, 0x0D68U, 0x3FF3U, 0x2E7AU,
		0xE70EU, 0xF687U, 0xC41CU, 0xD595U, 0xA12AU, 0xB0A3U, 0x8238U, 0x93B1U,
		0x6B46U, 0x7ACFU, 0x4854U, 0x59DDU, 0x2D62U, 0x3CEBU, 0x0E70U, 0x1FF9U,
		0xF78FU, 0xE606U, 0xD49DU, 0xC514U, 0xB1ABU, 0xA022U, 0x92B9U, 0x8330U,
		0x7BC7U, 0x6A4EU, 0x58D5U, 0x495CU, 0x3DE3U, 0x2C6AU, 0x1EF1U, 0x0F78U,
	};

	uint16_t crc = 0xffff;
	for (--len; ~len; --len)
		crc = table[*data++ ^ (uint8_t) crc] ^ (crc >> 8);
	return crc ^ 0xffff;
}

static char *decode_inplace(char *start)
{
	char c;
	char *out = start;
	char *front = start;
	int esc = 0;
	while ((c = *front++) != 0x7e) {
		if (esc) {
			*out++ = c ^ 0x20;
			esc = 0;
		} else if (c == 0x7d) {
			esc = 1;
		} else {
			*out++ = c;
		}
	}
	if (out - start <= 2)
		return NULL;
	if (calc_crc(start, out - start - 2) != *(uint16_t *)(out - 2))
		return NULL;
	return out;
}

static char *encode_reversed(const char *start, const char *end, char *out)
{
	ssize_t len = end - start;
	uint16_t crc = calc_crc(start, len);
	ssize_t i;
	char c;

	*--out = 0x7e;
	for (i = -1; i <= len; ++i) {
		switch (i) {
		case -1:
			c = (crc & 0xff00u) >> 8;
			break;
		case 0:
			c = crc & 0x00ffu;
			break;
		default:
			c = end[-i];
			break;
		}
		switch (c) {
		case 0x7e:
		case 0x7d:
			*--out = c ^ 0x20;
			*--out = 0x7d;
			break;
		default:
			*--out = c;
			break;
		}
	}
	return out;
}

static ssize_t get_file_size(FILE *fp)
{
	ssize_t ret, len;

	ret = fseek(fp, 0, SEEK_END);
	if (ret < 0)
		return ret;
	len = ftell(fp);
	if (ret < 0)
		return ret;
	ret = fseek(fp, 0, SEEK_SET);
	if (ret < 0)
		return ret;

	return len;
}

static size_t count_characters(const char *start, const char *end, char c)
{
	size_t i = 0;
	while (start != end)
		i += (*start++ == c);
	return i;
}

static uint64_t stamp_posix2qualcomm(uint64_t posix)
{
	uint64_t seconds = posix / 1000000000;
	uint64_t remained = posix % 1000000000;
	seconds -= 315936000;
	remained *= 52428800;
	remained /= 1000000000;
	return seconds * 52428800 + remained;
}

static int work(void)
{
	ssize_t end, start, len, offset;
	char *tmp;
	struct stamp_log_t *stamp_log = stamps + nr_stamps;
	uint64_t *qcom_stamp, sdiff, start_bytes;
	int need_update;

	end = data_len - 1;
	while (end >= 0 && data[end] != 0x7e)
		--end;
	for (; end >= 0; end = start - 1) {
		start = end - 1;
		while (start >= 0 && data[start] != 0x7e)
			--start;
		++start;

		tmp = decode_inplace(data + start);
		if (!tmp) {
			printf("Warning: discarding corrupted frame at %ld\n", start);
			continue;
		}
		end = tmp - data;
		len = end - start;

		offset = 0;
		if (len >= 8) {
			start_bytes = *(uint64_t *)&data[start];
			if (start_bytes == 0x200000198 || start_bytes == 0x100000198)
				offset = 8;
		}
		if (len < offset + 2 || data[start + offset] != 0x10) {
			printf("Warning: discarding unsupported frame at %ld\n", start);
			continue;
		}
		offset += 2;

		if (len < offset + 6 + 8) {
			printf("Warning: frame at %ld is too short, which should never happen\n", start);
			continue;
		}
		qcom_stamp = (uint64_t *)&data[start + offset + 6];

		need_update = 0;
		while (stamp_log != stamps && stamp_log[-1].offset > start) {
			--stamp_log;
			need_update = 1;
		}
		if (stamp_log == stamps + nr_stamps) {
			printf("Warning: discarding tailing frame at %ld\n", start);
			continue;
		}
		if (need_update)
			sdiff = stamp_posix2qualcomm(stamp_log->stamp) - *qcom_stamp;
		*qcom_stamp += sdiff;

		out_current = encode_reversed(data + start, data + end, out_current);
	}

	len = out_end - out_current;
	if (len != fwrite(out_current, 1, len, out_fp)) {
		printf("Failed to write into output log\n");
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	if (argc != 4) {
		printf("Usage: %s [data log] [stamp log] [output log]\n", argv[0]);
		return -1;
	}

	data_fp = fopen(argv[1], "rb");
	if (!data_fp) {
		printf("Cannot open data log %s for reading\n", argv[1]);
		return -2;
	}
	stamp_fp = fopen(argv[2], "rb");
	if (!stamp_fp) {
		printf("Cannot open stamp log %s for reading\n", argv[2]);
		return -2;
	}
	out_fp = fopen(argv[3], "wb");
	if (!out_fp) {
		printf("Cannot open output log %s for writing\n", argv[3]);
		return -2;
	}

	data_len = get_file_size(data_fp);
	if (data_len <= 0) {
		printf("Cannot get file size for data log %s\n", argv[1]);
		return -3;
	}

	nr_stamps = get_file_size(stamp_fp) / sizeof(struct stamp_log_t);
	if (nr_stamps <= 0) {
		printf("Cannot get file size for stamp log %s\n", argv[2]);
		return -3;
	}

	data = malloc(data_len);
	stamps = malloc(nr_stamps * sizeof(struct stamp_log_t));
	if (!data || !stamps) {
		printf("Cannot allocate enough memory for reading data log or stamp log\n");
		return -3;
	}

	if ((data_len != fread(data, 1, data_len, data_fp)) ||
	    (nr_stamps != fread(stamps, sizeof(struct stamp_log_t), nr_stamps, stamp_fp))) {
		printf("Failed to read from data log or stamp log\n");
		return -4;
	}

	out_remained = count_characters(data, data + data_len, 0x7e) * 8;
	out_start = malloc(data_len + out_remained);
	if (!out_start) {
		printf("Cannot allocate memory for generating output log\n");
		return -5;
	}
	out_end = out_current = out_start + data_len + out_remained;

	return work();
}
