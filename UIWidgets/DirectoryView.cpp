#pragma once

#include "DirectoryView.h"

#include "tinyfiledialogs.h"

using namespace Phantom::UI;

DirectoryView::DirectoryView(const std::string& name) :
	IView(name),
	selectButton("Select" + name),
	pathView("Path" + name)
{
	selectButton.setFunction([=]() { onSelect(); });
	add(&selectButton);
	add(&pathView);
}

DirectoryView::DirectoryView(const std::string& name, const std::string& path) :
	IView(name),
	selectButton("Select" + name),
	pathView("Path" + name, path)
{
	selectButton.setFunction([=]() { onSelect(); });
	add(&selectButton);
	add(&pathView);
}

void DirectoryView::onSelect()
{
	auto fileName = tinyfd_selectFolderDialog("Open", "");
	if (fileName == nullptr) {
		return;
	}
	pathView.setValue(fileName);
};