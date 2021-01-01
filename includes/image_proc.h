#ifdef __clang__
#define STBIWDEF static inline
#endif
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct file_path {
	char* dir_name;
	char* img_name;
};

int resize_image3x(struct file_path* path) {
	int scale = 3;
	char* img_name = path->img_name;
	char* dir_name = path->dir_name;

	unsigned char* input_pixels;
	unsigned char* output_pixels;
	int w, h;
	int n;
	int out_w, out_h;

	printf("3x scale %s...\n", img_name);
	char* in_path = malloc(strlen(dir_name) + strlen(img_name) + 2);
	sprintf(in_path, "%s/%s", dir_name, img_name);

	input_pixels = stbi_load(in_path, &w, &h, &n, 0);
	out_w = w*scale;
	out_h = h*scale;
	output_pixels = (unsigned char*) malloc(out_w*out_h*n);
		printf("starting to resize\n");
	stbir_resize_uint8(input_pixels, w, h, 0, output_pixels, out_w, out_h, 0, n);

	char* out_path;
	/* This assumes scale < 10, otherwise need more space */
	out_path = malloc(strlen(OUT_DIR) + 4 + strlen(img_name));
	sprintf(out_path, "%s/%dx-%s", OUT_DIR, scale, img_name);
	stbi_write_jpg(out_path, out_w, out_h, n, output_pixels, 50);
	printf("done %s -> %s\n", in_path, out_path);

	stbi_image_free(input_pixels);
	free(out_path);
	free(in_path);
	return 0;
}


int resize_image2x(struct file_path* path) {
	int scale = 2;
	char* img_name = path->img_name;
	char* dir_name = path->dir_name;

	unsigned char* input_pixels;
	unsigned char* output_pixels;
	int w, h;
	int n;
	int out_w, out_h;

	printf("2x scale %s...\n", img_name);
	char* in_path = malloc(strlen(dir_name) + strlen(img_name) + 2);
	sprintf(in_path, "%s/%s", dir_name, img_name);

	input_pixels = stbi_load(in_path, &w, &h, &n, 0);
	out_w = w*scale;
	out_h = h*scale;
	output_pixels = (unsigned char*) malloc(out_w*out_h*n);
	stbir_resize_uint8(input_pixels, w, h, 0, output_pixels, out_w, out_h, 0, n);

	char* out_path;
	out_path = malloc(strlen(in_path) + 3);
	sprintf(out_path, "%s/%dx-%s", OUT_DIR, scale, img_name);
	stbi_write_jpg(out_path, out_w, out_h, n, output_pixels, 50);
	printf("done %s -> %s\n", in_path, out_path);

	stbi_image_free(input_pixels);
	free(out_path);
	free(in_path);
	return 0;
}

int convert_to_PNG(struct file_path* path) {
	char* img_name = path->img_name;
	char* dir_name = path->dir_name;

	unsigned char* input_pixels;
	int w, h;
	int n;
	int out_w, out_h;

	printf("converting to PNG %s...\n", img_name);
	char* in_path = malloc(strlen(dir_name) + strlen(img_name) + 2);
	sprintf(in_path, "%s/%s", dir_name, img_name);

	input_pixels = stbi_load(in_path, &w, &h, &n, 0);
	char* out_path;
	out_path = malloc(strlen(in_path) + 20);
	sprintf(out_path, "%s/%s", OUT_DIR, img_name);
	int len = strlen(out_path);
	out_path[len-3] = 'p';
	out_path[len-2] = 'n';
	out_path[len-1] = 'g';
	stbi_write_png(out_path, w, h, n, input_pixels, w*n);
	printf("done %s -> %s\n", in_path, out_path);

	stbi_image_free(input_pixels);
	free(out_path);
	free(in_path);
	return 0;
}
