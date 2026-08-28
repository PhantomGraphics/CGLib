#include "FileOpenView.h"

#include "tinyfiledialogs.h"

using namespace Phantom::UI;

FileOpenView::FileOpenView(const std::string& name) :
	IView(name),
	selectButton("Select"),
	fileNameView("FileName")
{
	selectButton.setFunction([=]() { onSelect(); });
	add(&selectButton);
	add(&fileNameView);
}

void FileOpenView::addFilter(const char* filter)
{
	filters.push_back(filter);
}

void FileOpenView::onSelect()
{
	const auto filterCount = static_cast<int>(filters.size());
	auto fileName = tinyfd_openFileDialog("Open", "", filterCount, filters.data(), nullptr, 0);
	if (fileName == nullptr) {
		return;
	}
	fileNameView.setValue(fileName);
};