#ifndef RENDER_H_Hwixjaqgh8vMTA
#define RENDER_H_Hwixjaqgh8vMTA
#include <cvd/image.h>
#include <cvd/rgb.h>
#include <cvd/byte.h>
#include <memory>
#include <utility>

class FontSet;

class Renderer
{
	std::unique_ptr<FontSet> f;
	CVD::Image<CVD::Rgb<CVD::byte> > screen;

	public:

	Renderer();
	~Renderer();

	static const int w=40;
	static const int h=25;

	const CVD::Image<CVD::Rgb<CVD::byte>>& render(const CVD::Image<CVD::byte> text, bool control, bool flash_on);
	const CVD::Image<CVD::Rgb<CVD::byte>>& get_rendered()
	{
		return screen;
	}

	CVD::ImageRef glyph_size() const;
	
	//The bounding box in pixels of the character under the current sixel in the image
	std::pair<CVD::ImageRef,CVD::ImageRef> char_area_under_sixel(int x, int y) const;

	//The bounding box in pixels of the sixel under the current sixel in the image
	std::pair<CVD::ImageRef,CVD::ImageRef> sixel_area(int x, int y) const;
};

#endif
