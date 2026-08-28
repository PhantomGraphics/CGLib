#pragma once

#include "FileOpenDialog.h"

#include "tinyfiledialogs.h"

using namespace Phantom::UI;

FileOpenDialog::FileOpenDialog(const std::string& name) :
	name(name.c_str())
{}

void FileOpenDialog::addFilter(char const* filter)
{
	filters.push_back(filter);
}

void FileOpenDialog::show()
{
	//char const * lFilterPatterns[2] = { "*.stl", "*.obj" };
	const auto filerCount = static_cast<int>(filters.size());
	filename = tinyfd_openFileDialog(name, "", filerCount, filters.data(), nullptr, 0);
}

std::string FileOpenDialog::getFileName() const
{
	if (filename == nullptr) {
		return "";
	}
	return filename;
}

std::filesystem::path FileOpenDialog::getFilePath() const
{
	return filename;
}
