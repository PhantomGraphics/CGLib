#pragma once

#include "FileSaveDialog.h"
#include "tinyfiledialogs.h"

using namespace Phantom::UI;

FileSaveDialog::FileSaveDialog(const std::string& name) :
	name(name.c_str())
{}

void FileSaveDialog::addFilter(char const* filter)
{
	filters.push_back(filter);
}

void FileSaveDialog::show()
{
	//char const * lFilterPatterns[2] = { "*.stl", "*.obj" };
	const auto filterCount = static_cast<int>(filters.size());
	const auto str = tinyfd_saveFileDialog("Save", "", filterCount, filters.data(), nullptr);
	this->filename = str;
}

std::string FileSaveDialog::getFileName() const
{
	if (filename == nullptr) {
		return "";
	}
	return filename;
}

std::filesystem::path FileSaveDialog::getFilePath() const
{
	return filename;
}