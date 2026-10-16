#pragma once

#include <string>
#include <filesystem>

#include "PLYFile.h"

namespace Phantom {
	namespace File {

/// @brief Writes PLY files in both ASCII and binary formats.
class PLYFileWriter
{
public:
	/// @brief Writes PLY data to a file in ASCII format.
	/// @param filename Output file path.
	/// @param pcd The PLY data to write.
	/// @return True on success, false on failure.
	bool writeASCII(const std::filesystem::path& filename, const PLYFile& pcd);

	/// @brief Writes PLY data to a stream in ASCII format.
	/// @param stream Output stream.
	/// @param pcd The PLY data to write.
	/// @return True on success, false on failure.
	bool writeASCII(std::ostream& stream, const PLYFile& pcd);

	/// @brief Writes PLY data to a file in binary format.
	/// @param filename Output file path.
	/// @param pcd The PLY data to write.
	/// @return True on success, false on failure.
	bool writeBinary(const std::filesystem::path& filename, const PLYFile& pcd);

	/// @brief Writes PLY data to a stream in binary format.
	/// @param stream Output stream.
	/// @param pcd The PLY data to write.
	/// @return True on success, false on failure.
	bool writeBinary(std::ostream& stream, const PLYFile& pcd);


private:
	//bool write(std::ostream& stream, const PCDFile::Header& header);

	//bool write(std::ostream& stream, const PCDFile::Data& data);

private:
};

	}
}
