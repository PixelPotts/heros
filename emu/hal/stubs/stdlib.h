/* Stub stdlib.h for bare-metal stb_image build */
#ifndef _BAREMETAL_STDLIB_H
#define _BAREMETAL_STDLIB_H
/* stb_image only needs malloc/free when STBI_MALLOC is not defined,
   and abs() for BMP loading. Both are handled in hal_image.c. */
#endif
