#ifndef WIN_LAYOUT_H
#define WIN_LAYOUT_H

#define NOMINMAX
#include <Windows.h>
#include <vector>
#include <memory>
#include <string>

using std::vector;
using std::unique_ptr;
using std::shared_ptr;
using std::string;

namespace winlayout {

	class LayoutNode;
	class BaseLayout;

	class LayoutNodeIterator {
	public:
		LayoutNodeIterator(vector<LayoutNode*>* p, size_t idx) noexcept
		{
			_container = p;
			index = idx;
		}
		LayoutNodeIterator(const LayoutNodeIterator&) = delete;
		LayoutNodeIterator(LayoutNodeIterator&& other) noexcept
		{
			_container = other._container;
			index = other.index;
			other._container = nullptr;
			other.index = 0;
		}
		LayoutNodeIterator& operator= (const LayoutNodeIterator&) = delete;
		LayoutNodeIterator& operator= (LayoutNodeIterator&&) = delete;
		LayoutNode& operator*() { return *(*_container)[index]; }
		LayoutNodeIterator& operator++()
		{
			++index;
			return *this;
		}
		bool operator!=(const LayoutNodeIterator& other) const
		{
			return index != other.index || _container != other._container;
		}
	private:
		vector<LayoutNode*>* _container;
		size_t index;
	};

	class LayoutNode {
	public:
		LayoutNode(HWND hwnd = nullptr) noexcept;
		LayoutNode(const LayoutNode&) = delete;
		LayoutNode(LayoutNode&&) = delete;
		LayoutNode& operator= (const LayoutNode&) = delete;
		LayoutNode& operator= (LayoutNode&&) = delete;
		void set_location(int x, int y);
		void get_box(int& x, int& y, int& w, int& h);
		void set_box(int x, int y, int w, int h);
		virtual void on_size(int w, int h);

		int get_fixed_h();
		void set_fixed_h(int h) { minH = maxH = h; }
		int minW, maxW, minH, maxH;

		HWND get_hwnd() { return hwnd; }
	protected:
		int x, y, w, h;
		HWND hwnd;
	};

	class Widget : public LayoutNode {
	public:
		Widget(HWND hwnd = nullptr) noexcept : LayoutNode(hwnd)
		{}
		Widget(const Widget&) = delete;
		Widget(Widget&&) = delete;
		Widget& operator= (const Widget&) = delete;
		Widget& operator= (Widget&&) = delete;
		virtual void on_size(int w, int h) override;
	public:
	};

	class WidgetsContainer : public LayoutNode {
	public:
		WidgetsContainer(HWND hwnd = nullptr) noexcept : LayoutNode(hwnd), _layout(nullptr)
		{}
		WidgetsContainer(const WidgetsContainer&) = delete;
		WidgetsContainer(WidgetsContainer&&) = delete;
		WidgetsContainer& operator= (const WidgetsContainer&) = delete;
		WidgetsContainer& operator= (const WidgetsContainer&&) = delete;
		virtual void on_size(int w, int h) override;
		int add(LayoutNode* node);
		void delete_node(int index);
		void clear();
		int get_children_size();
		void set_layout(BaseLayout* layout);

		LayoutNodeIterator begin() {
			return std::move(LayoutNodeIterator(&_children, 0));
		}

		LayoutNodeIterator end() {
			return std::move(LayoutNodeIterator(&_children, _children.size()));
		}
	private:
		vector<LayoutNode*> _children;
		BaseLayout* _layout;
	};

	class BaseLayout {
	public:
		BaseLayout() noexcept : leftPadding(0), bottomPadding(0), rightPadding(0), topPadding(0),
			hGap(0), vGap(0), useReletiveCoordinates(false)
		{}
		BaseLayout(const BaseLayout&) = delete;
		BaseLayout(BaseLayout&&) = delete;
		BaseLayout& operator= (const BaseLayout&) = delete;
		BaseLayout& operator= (BaseLayout&&) = delete;
		virtual void layout(int w, int h) = 0;
		void set_target(WidgetsContainer* target);
		void set_padding(int left, int bottom, int right, int top);
		void set_gap(int horizontal, int vertical);
		void use_reletive_coordinates(bool use) { useReletiveCoordinates = use; }
	protected:
		WidgetsContainer* target;
		int leftPadding, bottomPadding, rightPadding, topPadding;
		int hGap, vGap;
		bool useReletiveCoordinates;
	};

	class SequentialLayout : public BaseLayout {
	public:
		SequentialLayout(int direction = SBS_HORZ) noexcept : _direction(direction)
		{}
		SequentialLayout(const SequentialLayout&) = delete;
		SequentialLayout(SequentialLayout&&) = delete;
		SequentialLayout& operator= (const SequentialLayout&) = delete;
		SequentialLayout& operator= (SequentialLayout&&) = delete;
		virtual void layout(int w, int h) override;
	private:
		int _direction;
	};

	class EvenLayout : public BaseLayout {
	public:
		EvenLayout(int direction = SBS_HORZ) noexcept : _direction(direction)
		{}
		EvenLayout(const EvenLayout&) = delete;
		EvenLayout(EvenLayout&&) = delete;
		EvenLayout& operator= (const EvenLayout&) = delete;
		EvenLayout& operator= (EvenLayout&&) = delete;
		virtual void layout(int w, int h) override;
	private:
		int _direction;
	};

	class RatioLayout : public BaseLayout {
	public:
		RatioLayout(int direction = SBS_HORZ) noexcept : _direction(direction)
		{}
		RatioLayout(const RatioLayout&) = delete;
		RatioLayout(RatioLayout&&) = delete;
		RatioLayout& operator= (const RatioLayout&) = delete;
		RatioLayout& operator= (RatioLayout&&) = delete;
		virtual void layout(int w, int h) override;
		void set_percentages(vector<float>&& percentages);
	private:
		int _direction;
		vector<float> _percentages;
	};

};

#endif // WIN_LAYOUT_H
