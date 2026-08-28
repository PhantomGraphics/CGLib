#include "WireFrameBuilder.h"

#include "CGLib/Math/ICurve3d.h"
#include "CGLib/Math/ISurface3d.h"
#include "CGLib/Math/IVolume3d.h"

using namespace Phantom::Math;
using namespace Phantom::Shape;

void WireFrameBuilder::add(const ICurve3d<float>& curve, int unum)
{
	assert(1 <= unum);
	std::vector<int> indices;
	for (int i = 0; i <= unum; ++i) {
		const float u = i / static_cast<float>(unum);
		indices.push_back(createPosition(curve.getPosition(u)));
	}
	for (int i = 0; i < indices.size() - 1; ++i) {
		edges.push_back(WireFrame::Edge(indices[i], indices[i + 1]));
	}
}

void WireFrameBuilder::add(const ISurface3d<float>& surface, const int unum, const int vnum)
{
	assert(1 <= unum);
	assert(1 <= vnum);

	std::vector<std::vector<int>> grid;
	for (int i = 0; i <= unum; ++i) {
		const float u = i / static_cast<float>(unum);
		std::vector<int> g;
		for (int j = 0; j <= vnum; ++j) {
			const float v = j / static_cast<float>(vnum);
			g.push_back(createPosition(surface.getPosition(u, v)));
		}
		grid.push_back(g);
	}
	for (int i = 0; i < grid.size() - 1; ++i) {
		for (int j = 0; j < grid[i].size() - 1; ++j) {
			const auto v1 = grid[i][j];
			const auto v2 = grid[i + 1][j];
			const auto v3 = grid[i + 1][j + 1];
			const auto v4 = grid[i][j + 1];
			edges.push_back(WireFrame::Edge(v1, v2));
			edges.push_back(WireFrame::Edge(v2, v3));
			edges.push_back(WireFrame::Edge(v3, v4));
			edges.push_back(WireFrame::Edge(v4, v1));
		}
	}
}

void WireFrameBuilder::add(const IVolume3d<float>& volume, const int unum, const int vnum, const int wnum)
{
	assert(1 <= unum);
	assert(1 <= vnum);
	assert(1 <= wnum);

	std::vector<std::vector<std::vector<int>>> grid;
	grid.resize(unum + 1);
	for (int i = 0; i <= unum; ++i) {
		grid[i].resize(vnum + 1);
		const float u = i / static_cast<float>(unum);
		for (int j = 0; j <= vnum; ++j) {
			grid[i][j].resize(wnum + 1);
			const float v = j / static_cast<float>(vnum);
			for (int k = 0; k <= wnum; ++k) {
				const float w = k / static_cast<float>(wnum);
				const auto index = createPosition(volume.getPosition(u, v, w));
				grid[i][j][k] = index;
			}
		}
	}
	for (int i = 0; i < grid.size() - 1; ++i) {
		for (int j = 0; j < grid[i].size() - 1; ++j) {
			for (int k = 0; k < grid[i][j].size() - 1; ++k) {
				const auto v1 = grid[i][j][k];
				const auto v2 = grid[i + 1][j][k];
				const auto v3 = grid[i + 1][j + 1][k];
				const auto v4 = grid[i][j + 1][k];

				const auto v5 = grid[i][j][k + 1];
				const auto v6 = grid[i + 1][j][k + 1];
				const auto v7 = grid[i + 1][j + 1][k + 1];
				const auto v8 = grid[i][j + 1][k + 1];

				edges.push_back(WireFrame::Edge(v1, v2));
				edges.push_back(WireFrame::Edge(v2, v3));
				edges.push_back(WireFrame::Edge(v3, v4));
				edges.push_back(WireFrame::Edge(v4, v1));

				edges.push_back(WireFrame::Edge(v5, v6));
				edges.push_back(WireFrame::Edge(v6, v7));
				edges.push_back(WireFrame::Edge(v7, v8));
				edges.push_back(WireFrame::Edge(v8, v5));

				edges.push_back(WireFrame::Edge(v1, v5));
				edges.push_back(WireFrame::Edge(v2, v6));
				edges.push_back(WireFrame::Edge(v3, v7));
				edges.push_back(WireFrame::Edge(v4, v8));
			}
		}
	}
}

int WireFrameBuilder::createPosition(const Vector3df& v)
{
	positions.push_back(v);
	return static_cast<int>(positions.size() - 1);
}

std::unique_ptr<WireFrame> WireFrameBuilder::toWireFrame()
{
	auto wf = std::make_unique<WireFrame>();
	for (const auto& p : positions) {
		wf->add(std::make_unique<Vertex>(p));
	}
	for (const auto& e : edges) {
		wf->addEdge(e);
	}
	return wf;
}
