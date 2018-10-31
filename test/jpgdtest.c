//------------------------------------------------------------------------------
#include "picojpeg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define RGB565TEST
#ifdef RGB565TEST
#define OUTPUT_TYPE PJPG_RGB565
#else
#define OUTPUT_TYPE PJPG_RGB888
#endif

//------------------------------------------------------------------------------
typedef unsigned char uint8;
//------------------------------------------------------------------------------
static const uint8 BmpHdr[] = {
    0x42, 0x4d, // <<BITMAPFILEHEADER>>'BM' signature
    0x8a, 0xb0, 0x04, 0x00, // File size in bytes (V)
    0x00, 0x00, 0x00, 0x00, // Reserved
    0x8a, 0x00, 0x00, 0x00, // Offset to raster data
    0x7c, 0x00, 0x00, 0x00, // <<BITMAPINFOHEADER>> Size of InfoHeader (40)
    0xe0, 0x01, 0x00, 0x00, // Bitmap Width (V)
    0x40, 0x01, 0x00, 0x00, // Bitmap Height (V)
    0x01, 0x00, // Number of planes
    0x10, 0x00, // BitCount
    0x03, 0x00, 0x00, 0x00, // Compression
    0x00, 0xb0, 0x04, 0x00, // Image size in bytes (V)
    0x23, 0x2e, 0x00, 0x00, // X pixels per meter (300 DPI)
    0x23, 0x2e, 0x00, 0x00, // Y pixels per meter (300 DPI)
    0x00, 0x00, 0x00, 0x00, // Colors Used
    0x00, 0x00, 0x00, 0x00, // Colors Important
    0x00, 0xf8, 0x00, 0x00,
    0xe0, 0x07, 0x00, 0x00,
    0x1f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x42, 0x47, 0x52, 0x73,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
#define FILE_SIZE_OFFSET 2
#define IMAGE_WIDTH_OFFSET 18
#define IMAGE_HEIGHT_OFFSET 22
#define IMAGE_SIZE_OFFSET 34
//------------------------------------------------------------------------------

static int print_usage()
{
   printf("Usage: jpgdtest <-h [height]> <-w [width]> <-x [x_offset]> <-y [y_offset]> [source_file] <[dest_file]>\n");
   printf("source_file: JPEG file to decode. Note: Progressive files are not supported.\n");
   printf("dest_file: Output .bmp file\n");
   printf("\n");
   printf("Outputs 8-bit grayscale or truecolor 24-bit TGA files.\n");
   return EXIT_FAILURE;
}
//------------------------------------------------------------------------------
struct str_callback_data {
    FILE *f;
    int line_size;
};

static int datasrc(void *buffer, int size, void *pdata) {
    FILE *f = (FILE *)pdata;
    return((int)fread(buffer, 1, size, f));
}

static int sinkfn(void *buffer, int size, void *pdata) {
    struct str_callback_data *pcb = (struct str_callback_data *)pdata;
    int rc = (int)fwrite(buffer, 1, size, pcb->f);
    fseek(pcb->f, -pcb->line_size*2, SEEK_CUR);
    return(rc);
}

//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
   const char* p = "?";
   uint8 status;
   FILE *f;
   FILE *pfIn;
   int c;
   int height = 0;
   int width = 0;
   int xoffs = -1;
   int yoffs = -1;
   pjpeg_image_info_t image_info;
   void *storage;
   unsigned int linebuf_size;
   unsigned int image_size;
   unsigned int file_size;
   uint8 hdr[sizeof(BmpHdr)];
   struct str_callback_data cb;
   
   while ((c = getopt(argc, argv, "h:w:x:y:")) != -1) {
      switch (c) {
      case 'h':
         height = atol(optarg);
         break;
      case 'w':
         width = atol(optarg);
         break;
      case 'x':
         xoffs = atol(optarg);
         break;
      case 'y':
         yoffs = atol(optarg);
         break;
      }
   }
   if (argc - optind == 0) {
      return print_usage();
   }

   if ((pfIn = fopen(argv[optind], "rb")) == NULL) {
      printf("Failed opening source image %s\n", argv[2]);
      print_usage();
      return EXIT_FAILURE;
   }
   
   storage = malloc(pjpeg_get_storage_size());
   status = pjpeg_decode_init(&image_info, datasrc, pfIn, storage, OUTPUT_TYPE);
   if (status)
   {
      printf("pjpeg_decode_init() failed with status %u\n", status);
      if (status == PJPG_UNSUPPORTED_MODE)
      {
         printf("Progressive JPEG files are not supported.\n");
      }

      return EXIT_FAILURE;
   }

   if (height <= 0) {
      height = image_info.m_height;
   }
   if (width <= 0) {
      width = image_info.m_width;
   }
   pjpeg_set_window(&image_info, width, height, xoffs, yoffs);
   linebuf_size = pjpeg_get_line_buffer_size(&image_info);
   cb.line_size = (int)linebuf_size/(int)image_info.m_MCUHeight;
   image_size = cb.line_size*image_info.m_height;
   file_size = image_size + sizeof(hdr);
   image_info.m_linebuf = malloc(linebuf_size);
   if (image_info.m_linebuf == NULL) {
      return PJPG_NOTENOUGHMEM;
   }

   printf("Memory allocation: %u for internal decoder, %u for linebuf\n",
         pjpeg_get_storage_size(), linebuf_size);
   f = fopen((argc - optind > 1) ? argv[optind+1] : "picojpeg_out.bmp", "wb");
   if (!f) {
      printf("Error opening the output file.\n");
      return 1;
   }
   cb.f = f;
   // The following line writes a header for a .bmp file.
   memcpy(hdr, BmpHdr, sizeof(BmpHdr));
   hdr[FILE_SIZE_OFFSET] = (uint8)(file_size & 0xFF);
   hdr[FILE_SIZE_OFFSET + 1] = (uint8)((file_size >> 8) & 0xFF);
   hdr[FILE_SIZE_OFFSET + 2] = (uint8)((file_size >> 16) & 0xFF);
   hdr[FILE_SIZE_OFFSET + 3] = (uint8)((file_size >> 24) & 0xFF);
   hdr[IMAGE_WIDTH_OFFSET] = (uint8)(image_info.m_width & 0xFF);
   hdr[IMAGE_WIDTH_OFFSET + 1] = (uint8)((image_info.m_width >> 8) & 0xFF);
   hdr[IMAGE_WIDTH_OFFSET + 2] = (uint8)((image_info.m_width >> 16) & 0xFF);
   hdr[IMAGE_WIDTH_OFFSET + 3] = (uint8)((image_info.m_width >> 24) & 0xFF);
   hdr[IMAGE_HEIGHT_OFFSET] = (uint8)(image_info.m_height & 0xFF);
   hdr[IMAGE_HEIGHT_OFFSET + 1] = (uint8)((image_info.m_height >> 8) & 0xFF);
   hdr[IMAGE_HEIGHT_OFFSET + 2] = (uint8)((image_info.m_height >> 16) & 0xFF);
   hdr[IMAGE_HEIGHT_OFFSET + 3] = (uint8)((image_info.m_height >> 24) & 0xFF);
   hdr[IMAGE_SIZE_OFFSET] = (uint8)(image_size & 0xFF);
   hdr[IMAGE_SIZE_OFFSET + 1] = (uint8)((image_size >> 8) & 0xFF);
   hdr[IMAGE_SIZE_OFFSET + 2] = (uint8)((image_size >> 16) & 0xFF);
   hdr[IMAGE_SIZE_OFFSET + 3] = (uint8)((image_size >> 24) & 0xFF);
   fwrite(hdr, 1, sizeof(hdr), f);
   fseek(f, cb.line_size*(image_info.m_height - 1), SEEK_CUR);
   // pjpeg_decode_scanlines() decodes the image 8 lines at a time and
   // streams them to the output sinkfn()
   status = pjpeg_decode_scanlines(&image_info, sinkfn, &cb);
   free(image_info.m_linebuf);
   if (status)
   {
      printf("pjpeg_decode_scanlines() failed with status %u\n", status);
      return EXIT_FAILURE;
   }
   switch (image_info.m_scanType)
   {
      case PJPG_GRAYSCALE: p = "GRAYSCALE"; break;
      case PJPG_YH1V1: p = "H1V1"; break;
      case PJPG_YH2V1: p = "H2V1"; break;
      case PJPG_YH1V2: p = "H1V2"; break;
      case PJPG_YH2V2: p = "H2V2"; break;
   }
   printf("Width: %i, Height: %i, Comps: %i, Scan type: %s\n",
         image_info.m_width, image_info.m_height, image_info.m_comps, p);
   fclose(pfIn);

   fclose(f);

   printf("Successfully wrote destination file\n");
   
   free(storage);

   return EXIT_SUCCESS;
}
//------------------------------------------------------------------------------

