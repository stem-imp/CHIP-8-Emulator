#include "winlayout.h"
#include <algorithm>
#include <iostream>

using std::cout;
using std::cerr;
using std::endl;
using std::make_shared;

namespace winlayout {

	// Layout Node

	LayoutNode::LayoutNode(HWND hwnd) noexcept : x(0), y(0), w(0), h(0),
		minW(INT_MAX), minH(INT_MAX), maxW(INT_MIN), maxH(INT_MIN)
	{
		this->hwnd = hwnd;
	}

	void LayoutNode::set_location(int x, int y)
	{
		this->x = x;
		this->y = y;
	}

	void LayoutNode::get_box(int& x, int& y, int& w, int& h)
	{
		x = this->x;
		y = this->y;
		w = this->w;
		h = this->h;
	}

	void LayoutNode::set_box(int x, int y, int w, int h)
	{
		set_location(x, y);
		this->w = w;
		this->h = h;
		on_size(w, h);
	}

	void LayoutNode::on_size(int w, int h)
	{
	}

	int LayoutNode::get_fixed_h()
	{
		if (minH == maxH && minH != INT_MAX) return minH;
		return INT_MAX;
	}

	void Widget::on_size(int w, int h)
	{
		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
		}
	}

	void WidgetsContainer::on_size(int w, int h)
	{
		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
		}
		if (_layout) {
			_layout->layout(w, h);
		}
	}

	int WidgetsContainer::add(LayoutNode* node)
	{
		int index = _children.size();
		_children.push_back(node);
		return index;
	}

	void WidgetsContainer::delete_node(int index)
	{
		if (index >= _children.size()) return;
		_children.erase(_children.begin() + index);
	}

	void WidgetsContainer::clear()
	{
		_children.clear();
	}

	int WidgetsContainer::get_children_size()
	{
		return _children.size();
	}

	void WidgetsContainer::set_layout(BaseLayout* layout)
	{
		_layout = layout;
		if (_layout) _layout->set_target(this);
	}


	// Layout

	void BaseLayout::set_target(WidgetsContainer* target)
	{
		this->target = target;
	}

	void BaseLayout::set_padding(int left, int bottom, int right, int top)
	{
		leftPadding = left;
		bottomPadding = bottom;
		rightPadding = right;
		topPadding = top;
	}

	void BaseLayout::set_gap(int horizontal, int vertical)
	{
		hGap = horizontal;
		vGap = vertical;
	}

	void SequentialLayout::layout(int w, int h)
	{
		if (!target) return;
		int baseX, baseY, baseW, baseH;
		target->get_box(baseX, baseY, baseW, baseH);
		if (useReletiveCoordinates) baseX = baseY = 0;
		int n = target->get_children_size();
		int curX, curY;
		int eX, eY, eW, eH;
		if (_direction == SBS_HORZ) {
			curX = leftPadding;
			curY = topPadding;
			int remainingW = w - leftPadding - rightPadding;
			int remainingH = h - bottomPadding - topPadding;
			for (auto& e : (*target)) {
				e.get_box(eX, eY, eW, eH);
				if (eW > remainingW) break;
				int finalH = std::min(eH, remainingH);
				e.set_box(baseX+curX, baseY+curY, eW, finalH);
				curX += eW + hGap;
				remainingW -= eW + hGap;
			}
		}
		else if (_direction == SBS_VERT) {
			curX = leftPadding;
			curY = topPadding;
			int remainingW = w - leftPadding - rightPadding;
			int remainingH = h - bottomPadding - topPadding;
			for (auto& e : (*target)) {
				e.get_box(eX, eY, eW, eH);
				int fixedH = e.get_fixed_h();
				if (fixedH != INT_MAX) {
					eH = fixedH;
				}
				if (eH > remainingH) break;
				int finalW = std::min(eW, remainingW);
				e.set_box(baseX + curX, baseY + curY, finalW, eH);
				curY += eH + vGap;
				remainingH -= eH + vGap;
			}
		}
		
	}

	void EvenLayout::layout(int w, int h)
	{
		if (!target) return;
		int baseX, baseY, baseW, baseH;
		target->get_box(baseX, baseY, baseW, baseH);
		int n = target->get_children_size();
		if (_direction == SBS_HORZ) {
			int remainingW = w - leftPadding - rightPadding - (n - 1) * hGap;
			int entryW = remainingW / n;
			int extra = remainingW % n;
			int x = baseX + leftPadding;
			int y = baseY + topPadding;
			int entryH = h - bottomPadding - topPadding;
			for (auto& e : (*target)) {
				int extraW = extra-- > 0 ? 1 : 0;
				e.set_box(x, y, entryW + extraW, entryH);
				x += entryW + extraW + hGap;
			}
		}
		else if (_direction == SBS_VERT) {

		}
	}

	void RatioLayout::layout(int w, int h)
	{
		if (!target) return;
		int baseX, baseY, baseW, baseH;
		target->get_box(baseX, baseY, baseW, baseH);
		if (useReletiveCoordinates) baseX = baseY = 0;
		int n = target->get_children_size();
		if (_direction == SBS_HORZ) {
		}
		else if (_direction == SBS_VERT) {
			int idx = 0;
			int x = baseX + leftPadding;
			int y = baseY + topPadding;
			int entryW = w - leftPadding - rightPadding;
			int remainingH = h - bottomPadding - topPadding - (n - 1) * vGap;
			int totalFixedH = 0;
			for (auto& e : (*target)) {
				if (e.minH == e.maxH && e.minH != INT_MAX) {
					totalFixedH += e.minH;
				}
			}
			
			int clientH = remainingH;
			remainingH -= totalFixedH;
			int accumH = 0;
			for (auto& e : (*target)) {
				if (idx >= _percentages.size()) break;
				int entryH = static_cast<int>(remainingH * _percentages[idx] / 100.f);
				int fixedH = e.get_fixed_h();
				if (fixedH != INT_MAX) {
					entryH = fixedH;
				}
				if (accumH + entryH > remainingH + totalFixedH) break;
				accumH += entryH;
				if (idx++ == _percentages.size() - 1) {
					entryH += (clientH - accumH);
				}
				e.set_box(x, y, entryW, entryH);
				y += entryH + vGap;
			}
		}
	}

	void RatioLayout::set_percentages(vector<float>&& percentages)
	{
		_percentages = std::move(percentages);
	}

};
