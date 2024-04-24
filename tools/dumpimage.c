// SPDX-License-Identifier: GPL-2.0+
/*
 * Based on mkimage.c.
 *
 * Written by Guilherme Maciel Ferreira <guilherme.maciel.ferreira@gmail.com>
 */

#include "dumpimage.h"
#include <image.h>
#include <version.h>
#include <linux/libfdt.h>

static void usage(void);

#define UBI_DEV_START "/dev/ubi"
#define UBI_SYSFS "/sys/class/ubi"

#define MTD_DEV_START "/dev/mtd"
#define MTD_SYSFS "/sys/class/mtd"

static int is_ubi_devname(const char *devname)
{
	return !strncmp(devname, UBI_DEV_START, sizeof(UBI_DEV_START) - 1);
}

static off_t get_ubi_data_size(const char *devname)
{
	FILE *file;
	off_t res = 0;
	char *type, path[256];

	snprintf(path, sizeof(path), UBI_SYSFS "%s/type", devname + 4);
	file = fopen(path, "r");
	if (file) {
		type = fgets(path, 20, file);
		fclose(file);

		if (!type)
			return 0;

		type[strcspn(type, "\n")] = 0;
		 if(strcmp(type, "static"))
			return 0;
	}

	snprintf(path, sizeof(path), UBI_SYSFS "%s/data_bytes", devname + 4);

	file = fopen(path, "r");
	if (file) {
		if (fscanf(file, "%ld", &res) != 1)
			res = 0;
		fclose(file);
	}

	return res;
}

static int is_mtd_devname(const char *devname)
{
	return !strncmp(devname, MTD_DEV_START, sizeof(MTD_DEV_START) - 1);
}

static off_t get_mtd_data_size(const char *devname)
{
	FILE *file;
	off_t res = 0;
	char path[256];

	snprintf(path, sizeof(path), MTD_SYSFS "%s/size", devname + 4);

	file = fopen(path, "r");
	if (file) {
		if (fscanf(file, "%ld", &res) != 1)
			res = 0;
		fclose(file);
	}

	return res;
}
/* parameters initialized by core will be used by the image type code */
static struct image_tool_params params;

/*
 * dumpimage_extract_subimage -
 *
 * It scans all registered image types,
 * verifies image_header for each supported image type
 * if verification is successful, it extracts the desired file,
 * indexed by pflag, from the image
 *
 * returns negative if input image format does not match with any of
 * supported image types
 */
static int dumpimage_extract_subimage(struct image_type_params *tparams,
		void *ptr, struct stat *sbuf)
{
	int retval = -1;

	if (tparams->verify_header) {
		retval = tparams->verify_header((unsigned char *)ptr,
				sbuf->st_size, &params);
		if (retval != 0) {
			fprintf(stderr, "%s: failed to verify header of %s\n",
				params.cmdname, tparams->name);
			return -1;
		}

		/*
		 * Extract the file from the image
		 * if verify is successful
		 */
		if (tparams->extract_subimage) {
			retval = tparams->extract_subimage(ptr, &params);
			if (retval != 0) {
				fprintf(stderr, "%s: extract_subimage failed for %s\n",
					params.cmdname, tparams->name);
				return -3;
			}
		} else {
			fprintf(stderr,
				"%s: extract_subimage undefined for %s\n",
				params.cmdname, tparams->name);
			return -2;
		}
	}

	return retval;
}

int main(int argc, char **argv)
{
	int opt;
	int ifd = -1;
	struct stat sbuf;
	char *ptr;
	int retval = EXIT_SUCCESS;
	struct image_type_params *tparams = NULL;
	bool is_ubi, is_mtd;

	params.cmdname = *argv;

	while ((opt = getopt(argc, argv, "hlo:T:p:V")) != -1) {
		switch (opt) {
		case 'l':
			params.lflag = 1;
			break;
		case 'o':
			params.outfile = optarg;
			params.iflag = 1;
			break;
		case 'T':
			params.type = genimg_get_type_id(optarg);
			if (params.type < 0) {
				fprintf(stderr, "%s: Invalid type\n",
					params.cmdname);
				exit(EXIT_FAILURE);
			}
			break;
		case 'p':
			params.pflag = strtoul(optarg, &ptr, 10);
			if (*ptr) {
				fprintf(stderr,
					"%s: invalid file position %s\n",
					params.cmdname, *argv);
				exit(EXIT_FAILURE);
			}
			break;
		case 'V':
			printf("dumpimage version %s\n", PLAIN_VERSION);
			exit(EXIT_SUCCESS);
		case 'h':
		default:
			usage();
			break;
		}
	}

	if (argc < 2 || (params.iflag && params.lflag))
		usage();

	if (optind >= argc) {
		fprintf(stderr, "%s: image file missing\n", params.cmdname);
		exit(EXIT_FAILURE);
	}

	params.imagefile = argv[optind];

	/* set tparams as per input type_id */
	tparams = imagetool_get_type(params.type);
	if (!params.lflag && tparams == NULL) {
		fprintf(stderr, "%s: unsupported type: %s\n",
			params.cmdname, genimg_get_type_name(params.type));
		exit(EXIT_FAILURE);
	}

	/*
	 * check the passed arguments parameters meets the requirements
	 * as per image type to be generated/listed
	 */
	if (tparams && tparams->check_params) {
		if (tparams->check_params(&params)) {
			fprintf(stderr, "%s: Parameter check failed\n",
				params.cmdname);
			exit(EXIT_FAILURE);
		}
	}

	if (!params.lflag && !params.outfile) {
		fprintf(stderr, "%s: No output file provided\n",
			params.cmdname);
		exit(EXIT_FAILURE);
	}

	ifd = open(params.imagefile, O_RDONLY|O_BINARY);
	if (ifd < 0) {
		fprintf(stderr, "%s: Can't open \"%s\": %s\n", params.cmdname,
			params.imagefile, strerror(errno));
		exit(EXIT_FAILURE);
	}

	is_ubi = is_ubi_devname(params.imagefile);
	is_mtd = is_mtd_devname(params.imagefile);
	if (is_ubi || is_mtd) {
		memset(&sbuf, 0, sizeof(sbuf));
		sbuf.st_size = is_ubi ?
			get_ubi_data_size(params.imagefile) :
			get_mtd_data_size(params.imagefile);
		sbuf.st_mode = S_IFREG;

		if (!sbuf.st_size) {
			struct fdt_header fdt;

			retval = read(ifd, &fdt, sizeof(fdt));
				if (retval < sizeof(fdt) || fdt_magic(&fdt) != FDT_MAGIC) {
				fprintf(stderr, "%s: Can't stat \"%s\"\n",
					params.cmdname, params.imagefile);
				exit(EXIT_FAILURE);
			}
			sbuf.st_size = fdt_totalsize(&fdt);
			lseek(ifd, 0, SEEK_SET);
		}
	} else if (fstat(ifd, &sbuf) < 0) {
		fprintf(stderr, "%s: Can't stat \"%s\": %s\n", params.cmdname,
			params.imagefile, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (tparams && (uint32_t)sbuf.st_size < tparams->header_size) {
		fprintf(stderr, "%s: Bad size: \"%s\" is not valid image\n",
			params.cmdname, params.imagefile);
		exit(EXIT_FAILURE);
	}

	if (is_ubi || is_mtd) {
		ptr = malloc(sbuf.st_size + 1);
		if (!ptr) {
			fprintf(stderr, "%s: Not enough memory \"%s\": %s\n",
				params.cmdname, params.imagefile,
				strerror(errno));
			exit(EXIT_FAILURE);
		}

		retval = read(ifd, ptr, sbuf.st_size);
		if (retval < 0) {
			fprintf(stderr, "%s: Can't read \"%s\": %s\n",
				params.cmdname, params.imagefile,
				strerror(errno));
			free(ptr);
			exit(EXIT_FAILURE);
		}
	} else {
		ptr = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, ifd, 0);
		if (ptr == MAP_FAILED) {
			fprintf(stderr, "%s: Can't read \"%s\": %s\n", params.cmdname,
				params.imagefile, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/*
	 * Both calls bellow scan through dumpimage registry for all
	 * supported image types and verify the input image file
	 * header for match
	 */
	if (params.iflag) {
		/*
		 * Extract the data files from within the matched
		 * image type. Returns the error code if not matched
		 */
		retval = dumpimage_extract_subimage(tparams, ptr, &sbuf);
		if (retval)
			fprintf(stderr, "%s: Can't extract subimage from %s\n",
				params.cmdname, params.imagefile);
	} else {
		/*
		 * Print the image information for matched image type
		 * Returns the error code if not matched
		 */
		retval = imagetool_verify_print_header(ptr, &sbuf, tparams,
						       &params);
	}

	if (is_ubi)
		free(ptr);
	else
		(void)munmap((void *)ptr, sbuf.st_size);
	(void)close(ifd);

	return retval;
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-T type] -l image\n"
		"          -l ==> list image header information\n"
		"          -T ==> parse image file as 'type'\n",
		params.cmdname);
	fprintf(stderr,
		"       %s [-T type] [-p position] [-o outfile] image\n"
		"          -T ==> declare image type as 'type'\n"
		"          -p ==> 'position' (starting at 0) of the component to extract from image\n"
		"          -o ==> extract component to file 'outfile'\n",
		params.cmdname);
	fprintf(stderr,
		"       %s -h ==> print usage information and exit\n",
		params.cmdname);
	fprintf(stderr,
		"       %s -V ==> print version information and exit\n",
		params.cmdname);

	exit(EXIT_SUCCESS);
}
