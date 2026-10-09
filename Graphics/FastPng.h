#pragma once

// Fast PNG encoder for screenshots and test captures.
//
// stb_image_write's PNG path is per-byte code with an exhaustive match search; an unoptimised (Debug) build needs ~4 s for a
// 1466x684 capture, which every GUI test paid on each screenshot. This encoder trades compression ratio for speed:
//   - filter: Sub on every row (flat regions become runs of zeros),
//   - deflate: one fixed-Huffman block, greedy LZ77 with a single-entry 4-byte hash table (no chains, no lazy matching).
// Typical UI / viewport captures still shrink a lot because they are mostly flat; photographic content ends up larger than
// with stb. The output is a valid, standard PNG. Header-only, no dependencies beyond the standard library.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace Phantom {
	namespace Graphics {
		namespace FastPngDetail {

			inline uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0xffffffffu)
			{
				static const std::array<uint32_t, 256> table = [] {
					std::array<uint32_t, 256> t{};
					for (uint32_t i = 0; i < 256; ++i) {
						uint32_t c = i;
						for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1;
						t[i] = c;
					}
					return t;
				}();
				for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
				return crc;
			}

			inline uint32_t adler32(const uint8_t* data, size_t size)
			{
				uint32_t a = 1, b = 0;
				while (size > 0) {
					const size_t n = std::min<size_t>(size, 5552);  // largest run before the sums can overflow 32 bits
					for (size_t i = 0; i < n; ++i) { a += data[i]; b += a; }
					a %= 65521u; b %= 65521u;
					data += n; size -= n;
				}
				return (b << 16) | a;
			}

			class BitWriter {
			public:
				explicit BitWriter(std::vector<uint8_t>& out) : out_(out) {}
				// Writes `count` bits, least-significant bit first (deflate packing order).
				void put(uint32_t value, int count)
				{
					acc_ |= static_cast<uint64_t>(value) << bits_;
					bits_ += count;
					while (bits_ >= 8) { out_.push_back(static_cast<uint8_t>(acc_ & 0xffu)); acc_ >>= 8; bits_ -= 8; }
				}
				void flush() { if (bits_ > 0) { out_.push_back(static_cast<uint8_t>(acc_ & 0xffu)); acc_ = 0; bits_ = 0; } }
			private:
				std::vector<uint8_t>& out_;
				uint64_t acc_ = 0;
				int bits_ = 0;
			};

			inline uint32_t reverseBits(uint32_t v, int count)
			{
				uint32_t r = 0;
				for (int i = 0; i < count; ++i) { r = (r << 1) | (v & 1u); v >>= 1; }
				return r;
			}

			// Fixed-Huffman literal/length code (RFC 1951 3.2.6), already bit-reversed for LSB-first output.
			inline void putLitLen(BitWriter& w, uint32_t sym)
			{
				if (sym <= 143)      w.put(reverseBits(0x30u + sym, 8), 8);
				else if (sym <= 255) w.put(reverseBits(0x190u + (sym - 144), 9), 9);
				else if (sym <= 279) w.put(reverseBits(sym - 256, 7), 7);
				else                 w.put(reverseBits(0xc0u + (sym - 280), 8), 8);
			}

			inline void putMatch(BitWriter& w, uint32_t length, uint32_t distance)
			{
				static const uint16_t lenBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
				static const uint8_t lenExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
				static const uint16_t distBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
				static const uint8_t distExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

				int lc = 28;
				while (lenBase[lc] > length) --lc;
				putLitLen(w, 257u + lc);
				if (lenExtra[lc]) w.put(length - lenBase[lc], lenExtra[lc]);

				int dc = 29;
				while (distBase[dc] > distance) --dc;
				w.put(reverseBits(static_cast<uint32_t>(dc), 5), 5);
				if (distExtra[dc]) w.put(distance - distBase[dc], distExtra[dc]);
			}

			inline void putBe32(std::vector<uint8_t>& v, uint32_t x)
			{
				v.push_back(static_cast<uint8_t>(x >> 24)); v.push_back(static_cast<uint8_t>(x >> 16));
				v.push_back(static_cast<uint8_t>(x >> 8));  v.push_back(static_cast<uint8_t>(x));
			}

			inline void putChunk(std::vector<uint8_t>& png, const char type[4], const std::vector<uint8_t>& body)
			{
				putBe32(png, static_cast<uint32_t>(body.size()));
				const size_t start = png.size();
				png.insert(png.end(), type, type + 4);
				png.insert(png.end(), body.begin(), body.end());
				putBe32(png, ~crc32(png.data() + start, png.size() - start));
			}

		} // namespace FastPngDetail

		/// @brief Encodes 8-bit interleaved pixels (1 = gray, 2 = gray+alpha, 3 = RGB, 4 = RGBA) as a PNG.
		/// @param swapRedBlue the input bytes are B,G,R[,A] (e.g. a Vulkan swapchain readback); they are written as R,G,B[,A].
		/// @param strideBytes bytes between the starts of two rows; 0 means tightly packed.
		/// @return the PNG file bytes, or an empty vector for invalid arguments.
		inline std::vector<uint8_t> encodePngFast(const uint8_t* pixels, uint32_t width, uint32_t height, int channels,
		                                          size_t strideBytes = 0, bool swapRedBlue = false)
		{
			using namespace FastPngDetail;
			if (!pixels || width == 0 || height == 0 || channels < 1 || channels > 4) return {};
			const size_t rowBytes = static_cast<size_t>(width) * channels;
			if (strideBytes == 0) strideBytes = rowBytes;
			if (strideBytes < rowBytes) return {};
			const bool swap = swapRedBlue && channels >= 3;

			// Scanlines with the Sub filter: raw[i] - raw[i - channels].
			std::vector<uint8_t> scan((rowBytes + 1) * height);
			std::vector<uint8_t> row(rowBytes);
			for (uint32_t y = 0; y < height; ++y) {
				const uint8_t* src = pixels + y * strideBytes;
				if (swap) {
					for (uint32_t x = 0; x < width; ++x) {
						const uint8_t* p = src + static_cast<size_t>(x) * channels;
						uint8_t* q = row.data() + static_cast<size_t>(x) * channels;
						q[0] = p[2]; q[1] = p[1]; q[2] = p[0];
						if (channels == 4) q[3] = p[3];
					}
					src = row.data();
				}
				uint8_t* dst = scan.data() + y * (rowBytes + 1);
				dst[0] = 1;  // Sub
				++dst;
				for (size_t i = 0; i < rowBytes; ++i)
					dst[i] = static_cast<uint8_t>(src[i] - (i >= static_cast<size_t>(channels) ? src[i - channels] : 0));
			}

			// zlib stream: header, one fixed-Huffman deflate block, Adler-32.
			std::vector<uint8_t> z;
			z.reserve(scan.size() / 4 + 64);
			z.push_back(0x78); z.push_back(0x01);
			{
				BitWriter w(z);
				w.put(1u, 1);  // BFINAL
				w.put(1u, 2);  // BTYPE = fixed Huffman
				constexpr int kHashBits = 15;
				std::vector<int32_t> table(static_cast<size_t>(1) << kHashBits, -1);
				const uint8_t* d = scan.data();
				const size_t n = scan.size();
				size_t i = 0;
				while (i < n) {
					if (i + 4 <= n) {
						uint32_t quad;
						std::memcpy(&quad, d + i, 4);
						const uint32_t h = (quad * 2654435761u) >> (32 - kHashBits);
						const int32_t cand = table[h];
						table[h] = static_cast<int32_t>(i);
						if (cand >= 0 && i - static_cast<size_t>(cand) <= 32768 && std::memcmp(d + cand, d + i, 4) == 0) {
							size_t len = 4;
							const size_t maxLen = std::min<size_t>(258, n - i);
							while (len < maxLen && d[cand + len] == d[i + len]) ++len;
							putMatch(w, static_cast<uint32_t>(len), static_cast<uint32_t>(i - cand));
							i += len;
							continue;
						}
					}
					putLitLen(w, d[i]);
					++i;
				}
				putLitLen(w, 256);  // end of block
				w.flush();
			}
			putBe32(z, adler32(scan.data(), scan.size()));

			static const uint8_t colorTypes[5] = { 0, 0, 4, 2, 6 };  // indexed by channel count
			std::vector<uint8_t> png = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
			std::vector<uint8_t> ihdr;
			putBe32(ihdr, width); putBe32(ihdr, height);
			ihdr.push_back(8); ihdr.push_back(colorTypes[channels]); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
			putChunk(png, "IHDR", ihdr);
			putChunk(png, "IDAT", z);
			putChunk(png, "IEND", {});
			return png;
		}

		/// @brief encodePngFast() straight to a file (the path is UTF-8). Returns false on invalid input or an I/O error.
		inline bool writePngFast(const std::string& utf8Path, const uint8_t* pixels, uint32_t width, uint32_t height, int channels,
		                         size_t strideBytes = 0, bool swapRedBlue = false)
		{
			const std::vector<uint8_t> png = encodePngFast(pixels, width, height, channels, strideBytes, swapRedBlue);
			if (png.empty()) return false;
#if defined(__cpp_char8_t)  // C++20: u8path() is deprecated, build the path from a u8string instead
			const std::filesystem::path path(std::u8string(reinterpret_cast<const char8_t*>(utf8Path.data()), utf8Path.size()));
#else
			const std::filesystem::path path = std::filesystem::u8path(utf8Path);
#endif
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			if (!file) return false;
			file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
			return static_cast<bool>(file);
		}

	} // namespace Graphics
} // namespace Phantom
