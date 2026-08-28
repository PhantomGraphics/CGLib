#include "Matrix4dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Matrix4dView::Matrix4dView(const std::string& name) :
	Matrix4dView(name, Matrix4df())
{
}

Matrix4dView::Matrix4dView(const std::string& name, const Matrix4df& value) :
	IView(name),
	row1View("Row1"),
	row2View("Row2"),
	row3View("Row3"),
	row4View("Row4")
{
	add(&row1View);
	add(&row2View);
	add(&row3View);
	add(&row4View);

	setValue(value);
}

void Matrix4dView::setValue(const Matrix4df& value)
{
	row1View.setValue(value[0]);
	row2View.setValue(value[1]);
	row3View.setValue(value[2]);
	row4View.setValue(value[3]);
}

Matrix4df Matrix4dView::getValue() const
{
	return Matrix4df
	(
		row1View.getValue(),
		row2View.getValue(),
		row3View.getValue(),
		row4View.getValue()
	);
}