#pragma once

#include "STLFile.h"
#include <filesystem>

namespace Phantom {
	namespace File {

		/// @brief Writes STL files in both ASCII and binary formats.
		class STLFileWriter
		{
		public:
			/// @brief Writes STL data to a file in ASCII format.
			/// @param filePath Output file path.
			/// @param stl The STL data to write.
			/// @return True on success, false on failure.
			bool writeAscii(const std::filesystem::path& filePath, const STLFile& stl);

			/// @brief Writes STL data to a stream in ASCII format.
			/// @param stream Output stream.
			/// @param stl The STL data to write.
			/// @return True on success, false on failure.
			bool writeAscii(std::ostream& stream, const STLFile& stl);

			/// @brief Writes STL data to a file in binary format.
			/// @param filePath Output file path.
			/// @param stl The STL data to write.
			/// @return True on success, false on failure.
			bool writeBinary(const std::filesystem::path& filePath, const STLFile& stl);

			/// @brief Writes STL data to a stream in binary format.
			/// @param stream Output stream.
			/// @param stl The STL data to write.
			/// @return True on success, false on failure.
			bool writeBinary(std::ostream& stream, const STLFile& stl);

		private:
		};
	}
}
