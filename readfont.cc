#include <iostream>
#include <array>
#include <utility>
#include <tuple>
#include <cvd/image_io.h>
#include <cvd/gl_helpers.h>
#include <cvd/videodisplay.h>
#include <tag/printf.h>
#include <FL/Fl.H>
#include <Fl/Fl_Menu_Bar.H>
#include <Fl/Fl_Menu_Item.H>
#include <Fl/Fl_Window.H>
#include <Fl/Fl_Pack.H>
#include <Fl/Fl_Box.H>
#include <Fl/Fl_Tile.H>
#include <Fl/Fl_Text_Editor.H>
#include <Fl/fl_draw.H>

using namespace std;
using namespace tag;
using namespace CVD;

class FontSet
{
	//very inefficient use of memory.
	Image<bool> blank;
	vector<vector<vector<Image<bool>>>> glyphs;
	
	Image<bool> get_upper(const Image<bool>& in)
	{
		Image<bool> upper(size());

		for(int r=0; r < size().y; r++)
			for(int c=0; c < size().x; c++)
				upper[r][c] = in[r/2][c];

		return upper;
	}

	Image<bool> get_lower(const Image<bool>& in)
	{
		Image<bool> lower(size());

		for(int r=0; r < size().y; r++)
			for(int c=0; c < size().x; c++)
				lower[r][c] = in[size().y/2 + r/2][c];

		return lower;
	}

	public:
		
	ImageRef size()
	{
		return ImageRef(12,18);
	}

	enum Mode
	{
		Normal,
		Graphics,
		ThinGraphics
	};
	
	enum Height
	{
		Standard,
		Upper,
		Lower
	};


	FontSet(string filename);
	const Image<bool>& get_glyph(int i, Mode m, Height h)
	{
		return glyphs[i][m][h];
	}

	const Image<bool>& get_blank()
	{
		return blank;
	}
};

FontSet::FontSet(string name)
{
	blank.resize(size());
	blank.zero();
	
		
	ifstream fi(name);
	
	//Teletext has 5 character sets:
	//Normal
	//Double height upper
	//Double height lower
	//Contig Block graphics
	//Noncontig block graphics

	//0 to 31 are control codes
	//32 to 127 are graphics codes

	//Teletext is 7 bit, so these are repeated.

	//The BBC terminal driver intercepts 0-31 and 127 as
	//terminal driver control codes, but not if the high bit is
	//set. This is not the case if screen memory is written directly.
		
	//In fact, the BBC terminal driver gently mangles a number of
	//things below 128, including the britigh pound and # signs.
	
	//This class reflects true telext (i.e. what the SAA chip does
	//if youwrite to screen memory).

	glyphs.resize(128, vector<vector<Image<bool>>>(5, vector<Image<bool>>(3, blank)));

	//Teletext font in file is is 10x18 (?)
	//Packed as 16x18

	for(int i=32; i <= 127; i++)
	{
		Image<bool> out(size());
		out.zero();

		for(int y=0; y < size().y; y++)
		{
			unsigned char c = fi.get(); //Second bits
			unsigned char d = fi.get(); //First bits

			out[y][0] = (bool)(d&2);
			out[y][1] = (bool)(d&1);
			out[y][2] = (bool)(c&128);
			out[y][3] = (bool)(c&64);
			out[y][4] = (bool)(c&32);
			out[y][5] = (bool)(c&16);
			out[y][6] = (bool)(c&8);
			out[y][7] = (bool)(c&4);
			out[y][8] = (bool)(c&2);
			out[y][9] = (bool)(c&1);
		}
		
		//Graphics blocks have bit 5 set...
		//Without bit 5, it reverts to normal characters.
		if(i & 32 )
		{
			Image<bool> graphics, thingraphics;
			graphics.resize(size());
			thingraphics.resize(size());
			thingraphics.fill(0);
			
			//2x3 subpixel sizes
			int sx = size().x/2;
			int sy = size().y/3;
			ImageRef ps(sx, sy);

			//Fill in the 6 chunks
			graphics.sub_image(ImageRef(  0,   0),ps).fill(i&1);
			graphics.sub_image(ImageRef( sx,   0),ps).fill(i&2);
			graphics.sub_image(ImageRef(  0,  sy),ps).fill(i&4);
			graphics.sub_image(ImageRef( sx,  sy),ps).fill(i&8);
			graphics.sub_image(ImageRef(  0,2*sy),ps).fill(i&16);
			graphics.sub_image(ImageRef( sx,2*sy),ps).fill(i&64);
			
			//Cut out the lines to thin down the graphics
			for(int y=0; y < size().y; y++)
				for(int x=0; x < size().x; x++)
					if(x!=0 && x != sx && y != 0 && y != sy && y != 2*sy)
						thingraphics[y][x] = graphics[y][x];
			
			glyphs[i][Normal][Standard] = out;
			glyphs[i][Graphics][Standard] = graphics;
			glyphs[i][ThinGraphics][Standard] = thingraphics;

			glyphs[i][Normal      ][Upper] = get_upper(glyphs[i][Normal      ][Standard]);
			glyphs[i][Graphics    ][Upper] = get_upper(glyphs[i][Graphics    ][Standard]);
			glyphs[i][ThinGraphics][Upper] = get_upper(glyphs[i][ThinGraphics][Standard]);

			glyphs[i][Normal      ][Lower] = get_lower(glyphs[i][Normal      ][Standard]);
			glyphs[i][Graphics    ][Lower] = get_lower(glyphs[i][Graphics    ][Standard]);
			glyphs[i][ThinGraphics][Lower] = get_lower(glyphs[i][ThinGraphics][Standard]);
		}
		else
		{
			glyphs[i][Normal][Standard] = out;
			glyphs[i][Graphics][Standard] = out;
			glyphs[i][ThinGraphics][Standard] = out;

			glyphs[i][Normal      ][Upper] = get_upper(out);
			glyphs[i][Graphics    ][Upper] = glyphs[i][Normal][Upper];
			glyphs[i][ThinGraphics][Upper] = glyphs[i][Normal][Upper];

			glyphs[i][Normal      ][Lower] = get_lower(out);
			glyphs[i][Graphics    ][Lower] = glyphs[i][Normal][Lower];
			glyphs[i][ThinGraphics][Lower] = glyphs[i][Normal][Lower];
		}

	}
}


class Renderer
{
	FontSet f;
	Image<Rgb<byte> > screen;

	public:

	Renderer()
	:f("teletext.fnt")
	{
		screen.resize(ImageRef(w,h).dot_times(f.size()));
	}

	static const int w=40;
	static const int h=25;

	const Image<Rgb<byte>>& render(const Image<byte> text);
	const Image<Rgb<byte>>& get_rendered()
	{
		return screen;
	}
	
	pair<ImageRef,ImageRef> char_area_under_sixel(int x, int y)
	{
		int xx = x/2;
		int yy = y/3;
		return make_pair(ImageRef(xx,yy).dot_times(f.size()), f.size());
	}
	pair<ImageRef,ImageRef> sixel_area(int x, int y)
	{
		int xx = x/2;
		int yy = y/3;
		int sx = f.size().x/2;
		int sy = f.size().y/3;
		ImageRef s(sx, sy);
		return make_pair(ImageRef(xx,yy).dot_times(f.size()) + s.dot_times(ImageRef(x%2, y%3)),s);
	}
};

const Image<Rgb<byte>>& Renderer::render(const Image<byte> text)
{
	if(text.size() != ImageRef(w, h))
	{
		cerr << "huh.\n";
		throw "oe noe";
	}

	screen.resize(text.size().dot_times(f.size()));

	bool double_height_bottom=false;
	for(int y=0; y < h; y++)
	{
		bool separated_graphics=false;
		bool hold_graphics=false;
		bool graphics_on=false;
		bool double_height=false;
		bool next_is_double_height=false;
		Rgb<byte> fg(255,255,255);
		Rgb<byte> bg(0,0,0);
		int last_graphic=0;

		for(int x=0; x < w; x++)
		{
			//Teletext is 7 bit.
			int c = text[y][x] & 0x7f;
			const Image<bool> *glyph;

			
			if(c < 32)
			{
				if(c>=1 && c <=7) //Enable colour text
				{
					fg.red   = (bool)(c&1) * 255;
					fg.green = (bool)(c&2) * 255;
					fg.blue  = (bool)(c&4) * 255;
					graphics_on=false;
				}
				else if(c == 12)
					double_height=false;
				else if(c == 13)
				{
					double_height=true;
					if(!double_height_bottom)
						next_is_double_height=true;
				}
				else if(c >=17 && c <= 23) //Enable colour graphics
				{
					fg.red   = (bool)(c&1) * 255;
					fg.green = (bool)(c&2) * 255;
					fg.blue  = (bool)(c&4) * 255;
					graphics_on=true;
				}
				else if(c == 25) //Switch to contiguous graphics if graphics are on
					separated_graphics=false;
				else if(c == 26) //Switch to separated graphics if graphics are on
					separated_graphics=true;
				else if(c == 27) //no-op
				{}
				else if(c == 28) //Black bg
					bg = Rgb<byte>(0,0,0);
				else if(c == 29) //New background (ie. copy fg colour)
					bg = fg;
				else if(c == 30)
					hold_graphics=true;
				else if(c == 31)
					hold_graphics=false;
				
				//Blank glyph, or not
				if(hold_graphics && graphics_on)
				{
					c = last_graphic;
				}
				else
					c=0;
			}

			bool no_render=0;
			//Double height text on row 1 maked row 2
			//a bottom row. Non double height chars on 
			//row 2 are blank
			FontSet::Height h=FontSet::Standard;
			if(double_height)
			{
				if(double_height_bottom)
					h = FontSet::Lower;
				else
					h = FontSet::Upper;
			}
			else
			{
				if(double_height_bottom)
					no_render=true;
			}
			
			FontSet::Mode m = FontSet::Normal;
			if(graphics_on)
			{
				if(separated_graphics)
					m = FontSet::ThinGraphics;
				else
					m = FontSet::Graphics;
			}
			
			//The last graphic drawn counts even if it isn't displayed, apparently.
			if(graphics_on && (c & 32))
				last_graphic=c;

			if(no_render)
				glyph = &f.get_blank();
			else
				glyph = &f.get_glyph(c, m, h);

			
			SubImage<Rgb<byte> > s = screen.sub_image(ImageRef(x,y).dot_times(f.size()), f.size());

			
			ImageRef p(0,0);
			do
			{
				if((*glyph)[p])
					s[p] = fg;
				else
					s[p] = bg;
			}
			while(p.next(f.size()));

		}
		double_height_bottom = next_is_double_height;
	}
	
	return screen;
}


Fl_Menu_Item menus[]=
{
  {"&File",0,0,0,FL_SUBMENU},
    {"&Open",	FL_ALT+'o', 0, 0, FL_MENU_INACTIVE},
    {"&Close",	0,	0},
    {"&Quit",	FL_ALT+'q', NULL, 0, FL_MENU_DIVIDER},
  {0},
  {"&Edit",0,0,0,FL_SUBMENU},
    {"&Nothing",	0,	0},
  {0},
  {0}
};

class MainUI;

class VDUDisplay: public Fl_Window
{
	public:
	MainUI& ui;
	VDUDisplay(MainUI& u)
	:Fl_Window(0,0,0,0,""),
	 ui(u)
	{
		
	}

	virtual void draw();
};

class MainUI: public Fl_Window
{
	public:

	Renderer ren;
	const ImageRef screen_size;
	Fl_Menu_Bar* menu;
	Fl_Group* group_B;
	VDUDisplay* vdu;

	static const int menu_height=30;
	static const int initial_pad=10;
	Image<byte> buffer;
	int cursor_x_sixel=4;
	int cursor_y_sixel=6;
	bool graphics_cursor=true;
	bool cursor_blink_on=true;
	double cursor_blink_time=.2;

	friend class VDUDisplay;


	/* OK, so I don't know FLTK very well... 

	+------------------------------------------------------+
	|   A                                                  |
    |                                                      |
    +-------------------------+----------------------------+
    |   B                                                  |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
    |                                                      |
	+-------------------------+----------------------------+

	The Window group will have a fully resizable group B
	And a menu bar A. The placement of A will make it resize horizontally

	Within B




	*/


	static const int pad=5;

	MainUI()
	:Fl_Window(0,0,"Editor"),
	 screen_size(ren.get_rendered().size())
	{

		begin();
			resizable(this);
			size(screen_size.x + 2*pad, screen_size.y + menu_height + 2*pad);

			menu = new Fl_Menu_Bar(0,0,w(), menu_height, "Menu");
			menu->menu(menus);

			group_B = new Fl_Window(0, menu_height, w(), h()-menu_height, "");	
			group_B->begin();

				vdu = new VDUDisplay(*this);
				vdu->resizable(0);
				vdu->resize(pad,pad,screen_size.x, screen_size.y);
				vdu->end();
			group_B->end();

		end();
		resizable(group_B); //Make group B the fully resizable widget

		buffer.resize(ImageRef(ren.w, ren.h));
		buffer.fill('a');
		show();

		Fl::add_timeout(cursor_blink_time, cursor_callback, this);
	}

	const Image<Rgb<byte>> get_rendered_text(int)
	{
		return ren.render(buffer);
	}

	static void cursor_callback(void* d)
	{
		MainUI* m = static_cast<MainUI*>(d);
		m->cursor_blink_on ^= true;
		m->vdu->redraw();
		Fl::repeat_timeout(m->cursor_blink_time, cursor_callback, d);
	}
	
	void set_x(int x)
	{
		cursor_x_sixel = max(0, min(x, ren.w * 2));
		cursor_blink_on=true;
		Fl::remove_timeout(cursor_callback, this);
		Fl::add_timeout(cursor_blink_time, cursor_callback, this);
		vdu->redraw();
	}
	void set_y(int y)
	{
		cursor_y_sixel = max(0, min(y, ren.h * 3));
		cursor_blink_on=true;
		Fl::remove_timeout(cursor_callback, this);
		Fl::add_timeout(cursor_blink_time, cursor_callback, this);
		vdu->redraw();
	}

	int handle(int e) override
	{
		if(e == FL_KEYBOARD)
		{	
			int k = Fl::event_key();
			
			//Decide whether to move in sixels or characters
			int dx=1;
			int dy=1;
			if(!graphics_cursor)
			{
				dx=2;
				dy=3;
			}

			if(k == FL_Left)
				set_x(cursor_x_sixel-dx);
			else if(k == FL_Right)
				set_x(cursor_x_sixel+dx);
			else if(k == FL_Up)
				set_y(cursor_y_sixel-dy);
			else if(k == FL_Down)
				set_y(cursor_y_sixel+dy);
			else
				return Fl_Window::handle(e);
		}
		else
			return Fl_Window::handle(e);
		return 1;
	}


};

void VDUDisplay::draw()
{
	const Image<Rgb<byte>>& i = ui.get_rendered_text(0);
	
	Image<Rgb<byte> > j = i.copy_from_me();
	if(ui.cursor_blink_on)
	{
		ImageRef tl, size;
		if(ui.graphics_cursor)
			tie(tl,size) = ui.ren.sixel_area(ui.cursor_x_sixel, ui.cursor_y_sixel);
		else
			tie(tl,size) = ui.ren.char_area_under_sixel(ui.cursor_x_sixel, ui.cursor_y_sixel);
		
		
		ImageRef p = tl;

		do
		{
			if(p.x % 1 == 0)
			{
				j[p].red ^= 255;
				j[p].green ^= 255;
				j[p].blue ^= 255;
			}
		}
		while(p.next(tl,tl + size));

	}
	cout << "oink\n";

	fl_draw_image((byte*)j.data(), 0, 0, j.size().x, j.size().y);
}	


int main()
{
	MainUI m;
	Fl::run();

	Renderer ren;

	Image<byte> text(ImageRef(ren.w,ren.h));


	//Read every second character except control ones,
	//to that it renders OK in vim with ^M for a <13>
	text.fill(0);
	for(int r=0; r < ren.w*ren.h; r++)
	{
		int c = cin.get();
		if(r % ren.w == 0 && c == '\n')
			c=cin.get();
			
		if(c>= 32)
			c=cin.get();
		
		if(!cin.good())
			break;
		text.data()[r] = c;
	}


	img_save(ren.render(text), cout,ImageType::PNM);

}

