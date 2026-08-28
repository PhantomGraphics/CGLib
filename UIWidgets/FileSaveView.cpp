#include "FileSaveView.h"

#include "tinyfiledialogs.h"

using namespace Phantom::UI;

FileSaveView::FileSaveView(const std::string& name) :
	IView(name),
	selectButton("Select"),
	fileNameView("FileName")
{
	selectButton.setFunction([=]() { onSelect(); });
	add(&selectButton);
	add(&fileNameView);
}

void FileSaveView::addFilter(const char* filter)
{
	filters.push_back(filter);
}

void FileSaveView::onSelect()
{
	const auto filterCount = static_cast<int>(filters.size());
	auto fileName = tinyfd_saveFileDialog("Save", "", filterCount, filters.data(), nullptr);
	if (fileName == nullptr) {
		return;
	}
	fileNameView.setValue(fileName);
};