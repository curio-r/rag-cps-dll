#include "stdafx.h"
#include "cps_dll.h"

#include <memory>
#include "LzmaDec.h"
#include "LzmaEnc.h"

// Namespace to avoid conflict with ZLIB's compress/uncompress
namespace ZLIB_NS {
#define ZLIB_WINAPI
#include <zlib.h>
}

extern "C" {

	static void * AllocForLzma(void *p, size_t size) { return malloc(size); }
	static void FreeForLzma(void *p, void *address) { free(address); }
	static ISzAlloc SzAllocForLzma = { &AllocForLzma, &FreeForLzma };

	CPS_EXPORT uncompress(
		unsigned char* buffer,
		unsigned int* real_size,
		unsigned char* input_buffer,
		unsigned int input_size
		) {

		if (input_size == 0) {
			return -1;
		}

		// A single NULL byte denote LZMA
		if (input_buffer[0] == 0) {
			
			if (input_size < LZMA_PROPS_SIZE + 1) {
				// Not enough data to start decode
				return -1;
			}

			unsigned int pack_size = input_size - LZMA_PROPS_SIZE - 1;

			ELzmaStatus detailed_status;

			// Decode, starting from input offset 1
			int ret_status = LzmaDecode(
				&buffer[0],							(size_t*)real_size,	// Decoded
				&input_buffer[1 + LZMA_PROPS_SIZE],	&pack_size,				// Encoded
				&input_buffer[1],					LZMA_PROPS_SIZE,		// Properties
				LZMA_FINISH_END, &detailed_status, &SzAllocForLzma
			);

			char tmp[1024];

			if (ret_status != 0) {
				sprintf(tmp, "Decode failed. Return code = %d, status = %d, real_size = %d, input_size = %d", ret_status, detailed_status, real_size, input_size);
				MessageBoxA(NULL, tmp, "LessGRF", MB_OK);
			}
			
			// Returns codes may not be equivalent with ZLIB, but the client never checks
			// for anything but the Z_OK (ZLIB) which matches SZ_OK (LZMA)
			return ret_status;

		}
		else { // ZLIB/DEFLATE
			
			// Decode, starting from input offset 0
			return ZLIB_NS::uncompress(
				buffer, 
				(ZLIB_NS::uLongf*)real_size, 
				input_buffer, 
				input_size
			);
		}
	}

	CPS_EXPORT compress(
		unsigned char* compressed_buffer,
		unsigned int* compressed_size,
		unsigned char* buffer,
		unsigned int input_size
	) {
		// Compression is used for guild emblems and needs to use ZLIB for server compatibility
		return ZLIB_NS::compress(
			compressed_buffer, 
			(ZLIB_NS::uLongf*)compressed_size,
			buffer, 
			input_size
		);
	}

	// LZMA compression
	CPS_EXPORT compress2(
		unsigned char* compressed_buffer,
		unsigned int* compressed_size,
		unsigned char* buffer,
		unsigned int input_size
		) {

		// Setup compression properties
		CLzmaEncProps props;

		LzmaEncProps_Init(&props);

		props.level = 9;
		props.numThreads = 2;
		props.writeEndMark = 0;
		props.reduceSize = 1024 * 1024 * 3; // 3MB

		size_t propsSize = LZMA_PROPS_SIZE;

		if (*compressed_size < propsSize + 1) {
			return -1;
		}

		// Encoding detection flag
		*compressed_buffer = 0;

		auto ret = LzmaEncode(
			&compressed_buffer[1 + LZMA_PROPS_SIZE], (size_t*)compressed_size,
			&buffer[0], input_size,
			&props, &compressed_buffer[1],
			&propsSize, props.writeEndMark,
			NULL,
			&SzAllocForLzma, &SzAllocForLzma
		);

		if (propsSize != LZMA_PROPS_SIZE) {
			// Don't know if it can happen, but it shouldn't!
			return -1;
		}

		*compressed_size += propsSize + 1;

		if (ret != SZ_OK) {
			return -1;
		}

		return ret;

	}

}

