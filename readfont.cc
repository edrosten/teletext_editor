#include <iostream>
#include <array>
#include <utility>
#include <tuple>
#include <sstream>
#include <cstring>
#include <cerrno>

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
#include <Fl/Fl_File_Chooser.H>
#include <Fl/Fl_Text_Editor.H>
#include <Fl/fl_draw.H>
#include <Fl/fl_ask.H>

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
		
	vector<Image<bool>> control_glyphs;
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
	
	for(int i=0; i < 32; i++)
	{
		ostringstream os;
		os << "resources/" << setfill('0') << setw(4) << i << ".png";
		
		cerr  << os.str() << endl;
		
		control_glyphs.push_back(img_load(os.str()));
	}

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

	const Image<Rgb<byte>>& render(const Image<byte> text, bool control);
	const Image<Rgb<byte>>& get_rendered()
	{
		return screen;
	}

	ImageRef glyph_size()
	{
		return f.size();
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

const Image<Rgb<byte>>& Renderer::render(const Image<byte> text, bool control)
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
			//Remeber c so we can reder control characters on top
			int actual_c = c;
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

			if(actual_c < 32 && control)
			{
				const Image<bool>& glyph = f.control_glyphs[actual_c];
				
				Rgb<byte> bg1 = bg;
				if(fg == bg)
					bg1 = Rgb<byte>(0,0,0);
					
				ImageRef p(0,0);
				do
				{
					if(!glyph[p])
					{
						if(s[p] == fg)
							s[p] = bg1;
						else
							s[p] = fg;
					}
				}
				while(p.next(f.size()));


			}

		}
		double_height_bottom = next_is_double_height;
	}
	
	return screen;
}

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
	enum class Mode
	{
		Character,
		Graphics,
		Text
	};

	Fl_Menu_Item menus[13]=
	{
	  {"&File",0,0,0,FL_SUBMENU,0,0,0,0},
		{"&Open",   FL_ALT+'o' ,   open_callback_s, this, 0,0,0,0,0},
		{"&Save",   FL_CTRL+'s',save_callback_s,    this, 0, 0, 0, 0, 0},
		{"Save &as",          0,save_as_callback_s, this, 0, 0, 0, 0, 0},
		{"Save a &copy",          0,save_a_copy_callback_s, this, 0, 0, 0, 0, 0},
		{"&Quit",	FL_ALT+'q' ,                    NULL, 0, FL_MENU_DIVIDER,0,0,0,0},
	  {0,0,0,0,0,0,0,0,0},
	  {"&Edit",0,0,0,FL_SUBMENU,0,0,0,0},
	  {0,0,0,0,0,0,0,0,0},
	  {"Codes", FL_F+1, menu_toggle_callback_s, this, FL_MENU_TOGGLE + FL_MENU_VALUE, 0,0,0,0},
	  {"Grid",  FL_F+2, menu_toggle_callback_s, this, FL_MENU_TOGGLE                , 0,0,0,0},
	  {0,0,0,0,0,0,0,0,0},
	};


	Renderer ren;
	const ImageRef screen_size;
	Fl_Menu_Bar* menu;
	Fl_Group* group_B;
	const Fl_Menu_Item* codes_toggle, *grid_toggle;
	VDUDisplay* vdu;

	static const int menu_height=30;
	static const int initial_pad=10;
	Image<byte> buffer;
	int cursor_x_sixel=4;
	int cursor_y_sixel=6;

	Mode mode = Mode::Character;
	bool cursor_blink_on=true;
	double cursor_blink_time=.2;
	bool show_control=1;
	bool checkpoint_issued=0;

	string save_name;
	string err;

	vector<Image<byte>> history, redo_buffer;
	
	void checkpoint()
	{
		history.push_back(buffer.copy_from_me());
		checkpoint_issued=1;
		vdu->redraw();
	}
	
	//Never duplicate checkpoint states. Explicitly checking states
	//makes logic with respect to complex sixel mangling easier.
	void process_checkpoint()
	{
		if(checkpoint_issued)
		{
			if(equal(buffer.begin(), buffer.end(), history.back().begin()))
			{
				history.pop_back();
			}
			else
			{
				redo_buffer.clear();
			}
		}
		checkpoint_issued=0;
	}

	void undo()
	{
		if(!history.empty())
		{
			redo_buffer.push_back(buffer);
			buffer=history.back();
			history.pop_back();
			vdu->redraw();
		}
	}

	void redo()
	{
		if(!redo_buffer.empty())
		{
			history.push_back(buffer);
			buffer=redo_buffer.back();
			redo_buffer.pop_back();
			vdu->redraw();
		}
	}

	static void menu_toggle_callback_s(Fl_Widget*, void * ui)
	{
		//Fl_Menu_ *m = static_cast<Fl_Menu_*>(w);
		//const Fl_Menu_Item *i = m->mvalue();
		static_cast<MainUI*>(ui)->vdu->redraw();
	}


	public:

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
			codes_toggle=menu->find_item("Codes");
			grid_toggle=menu->find_item("Grid");

			assert(codes_toggle != NULL);
			assert(grid_toggle != NULL);

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
		buffer.fill(' ');
		show();

		Fl::add_timeout(cursor_blink_time, cursor_callback, this);
	}

	const Image<Rgb<byte>> get_rendered_text(int)
	{
		return ren.render(buffer, codes_toggle->value());
	}

	static void cursor_callback(void* d)
	{
		MainUI* m = static_cast<MainUI*>(d);
		m->cursor_blink_on ^= true;
		m->vdu->redraw();
		Fl::repeat_timeout(m->cursor_blink_time, cursor_callback, d);
	}

	void cursor_change()
	{
		cursor_blink_on=true;
		Fl::remove_timeout(cursor_callback, this);
		Fl::add_timeout(cursor_blink_time, cursor_callback, this);
		vdu->redraw();
	}

	void set_x(int x)
	{
		cursor_x_sixel = max(0, min(x, ren.w * 2-1));
		cursor_change();
	}
	void set_y(int y)
	{
		cursor_y_sixel = max(0, min(y, ren.h * 3-1));
		cursor_change();
	}
	void advance()
	{
		set_x(cursor_x_sixel+2);
	}

	int xc()
	{
		return cursor_x_sixel/2;
	}
	int yc()
	{
		return cursor_y_sixel/3;
	}

	////////////////////////////////////////////////////////////////////////////////
	//
	// Stuff relating to getting and setting characters and sixels
	//

	enum class Set
	{
		Off = 0,
		On  = 1,
		Toggle = 2
	};

	byte& crnt()
	{
		return  buffer[yc()][xc()];
	}

	void set_sixel(Set way)
	{
		set_sixel(way, cursor_x_sixel, cursor_y_sixel);
	}
	
	void set_sixel(bool s, int x, int y)
	{
		if(s)
			set_sixel(Set::On, x, y);
		else
			set_sixel(Set::Off, x, y);
	}

	bool get_sixel(int x, int y)
	{
		int o = x%2 + 2 * (y%3);
		//But bits go 1,2,4,8,16, 64
		if(o ==5)
			o = 6;

		int mask = (1 << o);
		
		return static_cast<bool>(buffer[y/3][x/2] & mask);
	}
	
	bool is_graphic()
	{
		return crnt()&32;
	}	

	void set_sixel(Set way, int x, int y)
	{
		int o = x%2 + 2 *(y%3);
		//But bits go 1,2,4,8,16, 64
		if(o ==5)
			o = 6;

		int mask = (1 << o);

		if(way == Set::On)
			buffer[y/3][x/2] |= mask;
		else if(way == Set::Off)
			buffer[y/3][x/2] &= ~mask;
		else if(way == Set::Toggle)
			buffer[y/3][x/2] ^= mask;
	}

	int next_non_graphic_char()
	{
		//Search for next non-graphic
		int end=xc();
		for(; end < ren.w; end++)
			if(!( buffer[yc()][end] & 32))
				break;
		return end;
	}

	
	////////////////////////////////////////////////////////////////////////////////
	//
	// Functions relating to saving.
	//
	// this is surprisingly messy
	void actually_save(const string& name, bool remember)
	{
		ofstream out(name);
		out.write(reinterpret_cast<char*>(buffer.data()), buffer.size().area());
		
		if(!out.good())
		{
			err = "Error saving to \"" + name + "\": " + strerror(errno);	
			fl_choice(err.c_str(), "Horsefeathers!", "Gosh darn it!", ":(");	
		}
		else if(remember)
		{
			save_name = name;
			label(save_name.c_str());
		}
	}
	
	static void save_callback_s(Fl_Widget*, void * ui)
	{
		static_cast<MainUI*>(ui)->save_callback();
	}
	void save_callback()
	{
		if(save_name == "")
		{
			Fl_File_Chooser* file = new Fl_File_Chooser(".", "Text (*.txt)\tAll files (*)", Fl_File_Chooser::CREATE, "Save as...");
			file->callback(save_dialog_callback_s, this);
			file->show();
		}
		else
		{
			actually_save(save_name, true);
		}
	}

	static void save_as_callback_s(Fl_Widget*, void * ui)
	{
		static_cast<MainUI*>(ui)->save_as_callback();
	}
	void save_as_callback()
	{
		Fl_File_Chooser* file = new Fl_File_Chooser(".", "Text (*.txt)\tAll files (*)", Fl_File_Chooser::CREATE, "Save as...");
		file->callback(save_dialog_callback_s, this);
		file->show();
	}


	static void save_a_copy_callback_s(Fl_Widget*, void * ui)
	{
		static_cast<MainUI*>(ui)->save_a_copy_callback();
	}
	void save_a_copy_callback()
	{
		Fl_File_Chooser* file = new Fl_File_Chooser(".", "Text (*.txt)\tAll files (*)", Fl_File_Chooser::CREATE, "Save as...");
		file->callback(save_a_copy_dialog_callback_s, this);
		file->show();
	}

	static void save_dialog_callback_s(Fl_File_Chooser* w, void * ui)
	{
		if(!w->visible())
			((MainUI*)ui)->actually_save(w->value(), true);
	}

	static void save_a_copy_dialog_callback_s(Fl_File_Chooser* w, void * ui)
	{
		if(!w->visible())
			((MainUI*)ui)->actually_save(w->value(), false);
	}
	
	////////////////////////////////////////////////////////////////////////////////
	//
	// Loading (much cleaner)
	//
	//
	void load(const string& name)
	{
		ifstream in(name);

		Image<byte> tmp(buffer.size());
		in.read(reinterpret_cast<char*>(tmp.data()), buffer.size().area());
		
		if(!in.good())
		{
			err = "Error reading from\"" + name + "\": " + strerror(errno);	
			fl_choice(err.c_str(), "Horsefeathers!", "Gosh darn it!", ":(");	
		}
		else
		{
			save_name = name;
			label(save_name.c_str());
			checkpoint();
			buffer=tmp;
			process_checkpoint();
		}
	}

	static void open_callback_s(Fl_Widget*, void * ui)
	{
		static_cast<MainUI*>(ui)->open_callback();
	}
	void open_callback()
	{
		Fl_File_Chooser* file = new Fl_File_Chooser(".", "Text (*.txt)\tAll files (*)", Fl_File_Chooser::SINGLE, "Open");
		file->callback(open_dialog_callback_s, this);
		file->show();
	}
	static void open_dialog_callback_s(Fl_File_Chooser* w, void * ui)
	{
		if(!w->visible())
			((MainUI*)ui)->load(w->value());
	}
	



	////////////////////////////////////////////////////////////////////////////////
	//
	// Main event handler
	//
	
	//Should probably write this as a bunch of support functions and
	//calls to those functions, so it can be scripted easily.

	int handle(int e) override
	{

		if(e == FL_KEYBOARD)
		{	
			int k = Fl::event_key();
			cerr << "-->" << Fl::event_text() << "<--\n";
			
			//Decide whether to move in sixels or characters
			int dx=1;
			int dy=1;
			if(mode != Mode::Graphics)
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
			else if(k == FL_Home)
				set_x(0);
			else if(k == FL_End)
				set_x(80);
			else if(k >= 32 && k <= 127 && mode == Mode::Text && (Fl::event_state()& (FL_CTRL|FL_ALT))==0)
			{
				crnt() = Fl::event_text()[0];
				advance();
			}
			else if(k >= 32 && k <= 127 && Fl::event_state(FL_ALT) && Fl::event_state(FL_CTRL))
			{
				//Alt + key inserts a literal character
				checkpoint();
				crnt() = Fl::event_text()[0];
				advance();
			}
			else if(k == ' ') //Blank current element
			{

				if(mode == Mode::Graphics)
				{
					if(is_graphic())
					{
						checkpoint();
						set_sixel(Set::Off);
						set_x(cursor_x_sixel + 1);
					}
				}
				else
				{
					checkpoint();
					crnt() = 32;
					advance();
				}
			}
			else if(k == '.' && mode == Mode::Graphics) //Fill current sixel
			{
				if(is_graphic())
				{
					checkpoint();
					set_sixel(Set::On);
					set_x(cursor_x_sixel + 1);
				}
			}
			else if(k == 'f' && ( Fl::event_state()& (FL_SHIFT|FL_ALT|FL_CTRL))==0) //Fill block
			{
				checkpoint();
				crnt() = 127;
			}
			else if(k == 'n' && ( Fl::event_state()& (FL_ALT|FL_CTRL))==0)
			{
				//Set black/new background
				checkpoint();
				if(Fl::event_state(FL_SHIFT))
					crnt() = 28;
				else
					crnt() = 29;
			}
			else if(k == 'd' && ( Fl::event_state()& (FL_ALT|FL_CTRL))==0)
			{
				//Set single/double height
				checkpoint();
				if(Fl::event_state(FL_SHIFT))
					crnt() = 12;
				else
					crnt() = 13;
			}
			else if(k == 'h' && ( Fl::event_state()& (FL_ALT|FL_CTRL))==0)
			{
				//Release/hold graphics
				checkpoint();
				if(Fl::event_state(FL_SHIFT))
					crnt() = 31;
				else
					crnt() = 30;
			}
			else if(k == 'z' && Fl::event_state(FL_CTRL))
			{
				undo();
			}
			else if(k == 'y' && Fl::event_state(FL_CTRL))
			{
				redo();
			}
			else if(k == FL_Insert && !Fl::event_state(FL_SHIFT))
			{
				//Insert an element and shift thr row
				if(mode == Mode::Graphics)
				{
					
					if(is_graphic())
					{
						checkpoint();

						int end = next_non_graphic_char();

						for(int sx = end*2-2; sx >= cursor_x_sixel; sx--)
							set_sixel(get_sixel(sx,cursor_y_sixel), sx+1, cursor_y_sixel);
						set_sixel(Set::Off, cursor_x_sixel, cursor_y_sixel);
					}
				}
				else
				{
					checkpoint();
					for(int x=ren.w-2; x >= xc(); x--)
						buffer[yc()][x+1] = buffer[yc()][x];
					crnt() = 32;
				}
				
			}
			else if(k == FL_Insert && Fl::event_state(FL_SHIFT))
			{
				//Insert row
				checkpoint();
				
				for(int y=ren.h-1; y > yc(); y--)
					for(int x=0; x < ren.w; x++)
						buffer[y][x] = buffer[y-1][x];

				for(int x=0; x < ren.w; x++)
					buffer[yc()][x] = 32;
			}
			else if(k == FL_Delete && Fl::event_state(FL_SHIFT))
			{
				//Delete row
				checkpoint();
				
				for(int y=yc(); y < ren.h-1; y++)
					for(int x=0; x < ren.w; x++)
						buffer[y][x] = buffer[y+1][x];

				for(int x=0; x < ren.w; x++)
					buffer[ren.h-1][x] = 32;
			}
			else if(k == 'g' && Fl::event_state(FL_SHIFT))
			{
				//Switch between graphics mode and regular mode
				if(mode != Mode::Graphics)
					mode = Mode::Graphics;
				else
					mode = Mode::Character;
				cursor_change();
			}
			else if(k == 'g' && Fl::event_state(FL_CTRL))
			{
				mode = Mode::Graphics;
				cursor_change();
			}
			else if(k == 'c' && Fl::event_state(FL_CTRL))
			{
				mode = Mode::Character;
				cursor_change();
			}
			else if(k == 't' && Fl::event_state(FL_CTRL))
			{
				mode = Mode::Text;
				cursor_change();
			}
			else if( (k == 'x' || k == FL_Delete) && ( Fl::event_state()& (FL_SHIFT|FL_ALT|FL_CTRL))==0)
			{
				checkpoint();
				
				if(mode == Mode::Graphics)
				{
					if(is_graphic())
					{
						int end = next_non_graphic_char();
						for(int sx=cursor_x_sixel; sx < (end-1)*2+1; sx++)
							set_sixel(get_sixel(sx+1,cursor_y_sixel), sx, cursor_y_sixel);
						set_sixel(Set::Off, (end-1)*2+1, cursor_y_sixel);

					}
				}
				else
				{
					for(int x=xc(); x < ren.w-1; x++)
						buffer[yc()][x] = buffer[yc()][x+1];
					buffer[yc()][ren.w-1] = 32;
				}
			}
			else if((k == 'r' || k == 'g' || k == 'b' || k == 'c' || k == 'm' || k == 'y' || k == 'w') && !Fl::event_state(FL_ALT) && !Fl::event_state(FL_CTRL) &&!Fl::event_state(FL_SHIFT))
			{
				//Select colours.
				//RGB, one bit per colour...
				int c = 0;
				if(k == 'r')
					c = 1;
				else if(k == 'g')
				  	c = 2;	
				else if(k == 'y')
				  	c = 3;	
				else if(k == 'b')
				  	c = 4;	
				else if(k == 'm')
				  	c = 5;	
				else if(k == 'c')
				  	c = 6;	
				else if(k == 'w')
				  	c = 7;	
				
				if(mode == Mode::Graphics)
					c += 16;

				checkpoint();
				crnt()=c;

			}
			else if( (k == '9' || k == '0' || k == 'o' || k=='p' || k == 'l' || k == ';') && ( Fl::event_state()& (FL_SHIFT|FL_ALT|FL_CTRL))==0)
			{
				int c = crnt();
				if(c & 32)
				{
					checkpoint();
					if(k == '9')
						c ^= 1;
					else if(k == '0')
						c ^= 2;
					else if(k == 'o')
						c ^= 4;
					else if(k == 'p')
						c ^= 8;
					else if(k == 'l')
						c ^= 16;
					else if(k == ';')
						c ^= 64;

					crnt()=c;
				}


			}
			else
				return Fl_Window::handle(e);
		}
		else
			return Fl_Window::handle(e);

		process_checkpoint();
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
		if(ui.mode == MainUI::Mode::Graphics)
			tie(tl,size) = ui.ren.sixel_area(ui.cursor_x_sixel, ui.cursor_y_sixel);
		else if(ui.mode == MainUI::Mode::Text)
		{
			tie(tl,size) = ui.ren.char_area_under_sixel(ui.cursor_x_sixel, ui.cursor_y_sixel);
			size.x = 4;
		}	
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

	if(ui.grid_toggle->value())
	{
		
		ImageRef p(0,0);
		do
		{
			if(p.x % ui.ren.glyph_size().x == 0)
			{
				if(p.x%2 == 0)
					j[p] = Rgb<byte>(128,128,128);
				else
					j[p] = Rgb<byte>(0,0,0);
			}
			if(p.y % ui.ren.glyph_size().y == 0)
			{
				if(p.y%2 == 0)
					j[p] = Rgb<byte>(128,128,128);
				else
					j[p] = Rgb<byte>(0,0,0);
			}
		}
		while(p.next(j.size()));
	}

	fl_draw_image((byte*)j.data(), 0, 0, j.size().x, j.size().y);
}	


int main(int argc, char** argv)
{
	MainUI m;
		
	if(argc >= 2)
		m.load(argv[1]);
	
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


	//img_save(ren.render(text), cout,ImageType::PNM);

}

