#pragma once

#include "CGLib/Math/Vector3d.h"
#include "CGLib/Graphics/ColorRGBA.h"
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace Phantom {
	namespace File {

		/// @brief Utility class for reading values from streams during file parsing.
		class Helper {
		public:
			/// @brief Reads a single value of type T from a stream.
			/// @tparam T The type to read.
			/// @param s Input stream.
			/// @return The value read from the stream.
			template<typename T>
			static T read(std::istream& s)
			{
				T val;
				s >> val;
				return val;
			}

			/// @brief Reads three values from a stream and returns them as a Vector3dd.
			/// @tparam T The component type to read.
			/// @param s Input stream.
			/// @return The resulting 3D vector.
			template<typename T>
			static Math::Vector3dd readVector(std::istream& s)
			{
				T x, y, z;
				s >> x >> y >> z;
				return Math::Vector3dd(x, y, z);
			}

			/// @brief Reads RGB values from a stream and returns a ColorRGBAf (alpha = 0).
			/// @param s Input stream.
			/// @return The resulting color with alpha set to 0.
			static Graphics::ColorRGBAf readColorRGB(std::istream& s)
			{
				float r, g, b;
				s >> r >> g >> b;
				return Graphics::ColorRGBAf(r, g, b, 0.0f);
			}

			/// @brief Reads RGBA values from a stream and returns a ColorRGBAf.
			/// @param s Input stream.
			/// @return The resulting color.
			static Graphics::ColorRGBAf readColorRGBA(std::istream& s)
			{
				float r, g, b, a;
				s >> r >> g >> b >> a;
				return Graphics::ColorRGBAf(r, g, b, a);
			}

			/// @brief Reads the next token from a stream without advancing the stream position.
			/// @param stream Input stream.
			/// @return The next token string.
			static std::string readNextString(std::istream& stream)
			{
				std::string str = read<std::string>(stream);
				const int size = -static_cast<int>(str.size());
				stream.seekg(size, std::ios_base::cur);
				return str;
			}

			/// @brief Splits a string by the given delimiter character.
			/// @param str The string to split.
			/// @param delim The delimiter character.
			/// @return A vector of substrings.
			static std::vector< std::string > split(const std::string& str, char delim) {
				/*std::istringstream iss(str);
				std::string tmp;
				std::vector< std::string > res;
				while( getline(iss, tmp, delim) ) {
				res.push_back(tmp);
				}
				return res;*/
				//int a = str.find( delim );
				std::vector< std::string > res;
				std::string tmp;
				for (std::string::const_iterator iter = str.begin(); iter != str.end(); ++iter) {
					if (*iter != delim) {
						tmp += *iter;
					}
					else {
						if (!tmp.empty()) {
							res.push_back(tmp);
						}
						tmp.clear();
					}
				}
				res.push_back(tmp);
				return res;
			}

		};

	}
}
