#include <iostream>
#include <iomanip>
#include <array>
#include <utility>
#include <tuple>
#include <sstream>
#include <cstring>
#include <cerrno>

#include <cvd/image_io.h>
#include <cvd/gl_helpers.h>
#include <cvd/videodisplay.h>


#include <FL/Fl.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>


#include "render.h"

using namespace std;
using namespace CVD;

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

	Fl_Menu_Item menus[14]=
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
	  {"Blink",  FL_F+3, menu_toggle_callback_s, this, FL_MENU_TOGGLE                , 0,0,0,0},
	  {0,0,0,0,0,0,0,0,0},
	};


	Renderer ren;
	const ImageRef screen_size;
	Fl_Menu_Bar* menu;
	Fl_Group* group_B;
	const Fl_Menu_Item* codes_toggle, *grid_toggle,*blink_toggle;
	VDUDisplay* vdu;

	static const int menu_height=30;
	static const int initial_pad=10;
	Image<byte> buffer;
	int cursor_x_sixel=4;
	int cursor_y_sixel=6;

	Mode mode = Mode::Character;
	bool cursor_blink_on=true;
	double cursor_blink_time=.2;
	double text_blink_time=.5;
	bool text_blink_on=true;
	bool show_control=1;
	bool checkpoint_issued=0;

	string save_name;
	string err;

	vector<Image<byte>> history, redo_buffer;
	
	void checkpoint()
	{
		history.push_back(buffer);
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
			blink_toggle=menu->find_item("Blink");

			assert(codes_toggle != NULL);
			assert(grid_toggle != NULL);
			assert(blink_toggle != NULL);

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
		Fl::add_timeout(text_blink_time, text_flash_callback, this);

		callback(my_callback_s);
	}

	const Image<Rgb<byte>> get_rendered_text(int)
	{
		return ren.render(buffer, codes_toggle->value(), text_blink_on || !blink_toggle->value());
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


	static void text_flash_callback(void* d)
	{
		MainUI* m = static_cast<MainUI*>(d);
		m->text_blink_on ^= true;
		m->vdu->redraw();
		Fl::repeat_timeout(m->text_blink_time, text_flash_callback, d);
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
	
	static void my_callback_s(Fl_Widget*, void*) 
	{   
		if (Fl::event()==FL_SHORTCUT && Fl::event_key()==FL_Escape)     
			return; // ignore Escape  
		exit(0);
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
				if(Fl::event_state(FL_SHIFT))
				{
					if(isalpha(k))
						k = toupper(k);
				}
				else
					crnt() = k;
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
			else if(k == 'f' && Fl::event_state(FL_SHIFT)) //Fill block
			{
				if(mode == Mode::Graphics)
				{
					checkpoint();
					crnt() = 32;
				}
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
			else if(k == 'z' && ( Fl::event_state()& (FL_ALT|FL_CTRL))==0)
			{
				crnt()=0;
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
			else if( k == 'b' && Fl::event_state(FL_SHIFT))
			{
				checkpoint();
				crnt() = 8;
			}
			else if( k == 's' && Fl::event_state(FL_SHIFT))
			{
				checkpoint();
				crnt() = 9;
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
	
	Image<Rgb<byte> > j = i;
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
	try{

		MainUI m;
			
		if(argc >= 2)
			m.load(argv[1]);
		
		Fl::run();
	}
	catch(Exceptions::All e)
	{
		cerr << "Error: " << e.what() << endl;
		return 	1;
	}

	//img_save(ren.render(text), cout,ImageType::PNM);

}

