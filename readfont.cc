#include <iostream>
#include <array>
#include <cvd/image_io.h>
#include <cvd/gl_helpers.h>
#include <cvd/videodisplay.h>
#include <tag/printf.h>
using namespace std;
using namespace tag;
using namespace CVD;

class Font
{
	//very inefficient use of memory.
	vector<vector<Image<bool>>> glyphs;
	
	public:
		
	static const int w=10;
	static const int h=18;

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


	Font(string filename);
	const Image<bool>& get_glyph(int i, Mode m, Height h)
	{
		return glyphs[i][m];
	}
};

Font::Font(string name)
{
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

	glyphs.resize(127, vector<Image<bool>>(5));

	//Teletext font in file is is 10x18 (?)
	//Packed as 16x18

	for(int i=32; i <= 127; i++)
	{
		Image<bool> out(ImageRef(10,18));

		for(int y=0; y < h; y++)
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
		
		Image<bool> graphics, thingraphics;
		//Graphics blocks have bit 5 set...
		if(i & 32 )
		{
			Image<bool> graphics(ImageRef(10,18));
			Image<bool> thingraphics(ImageRef(10,18),false);
			
			ImageRef ps(w/2, h/3);

			//Fill in the 6 chunks
			graphics.sub_image(ImageRef(  0,    0),ps).fill((bool)i&1);
			graphics.sub_image(ImageRef(w/2,    0),ps).fill((bool)i&2);
			graphics.sub_image(ImageRef(  0,  h/3),ps).fill((bool)i&4);
			graphics.sub_image(ImageRef(w/2,  h/3),ps).fill((bool)i&8);
			graphics.sub_image(ImageRef(  0,2*h/3),ps).fill((bool)i&16);
			graphics.sub_image(ImageRef(w/2,2*h/3),ps).fill((bool)i&64);
			
			//Cut out the lines to thin down the graphics
			for(int y=0; y < h; y++)
				for(int x=0; x < w; x++)
					if(x!=0 && x != w/2 && y != 0 && y != h/3 && y != 2*h/3)
						thingraphics[y][x] = graphics[y][x];
			
		}
		else
		{
			graphics=out;
			thingraphics=out;
		}

		glyphs[i][Normal] = out;
		glyphs[i][Graphics] = graphics;
		glyphs[i][ThinGraphics] = thingraphics;
	}
}


int main()
{
	Font f("teletext.fnt");
	int w=40;
	int h=25;

	Image<byte> text(ImageRef(w,h));
	Image<byte> screen(text.size().dotstar(ImageRef(f.w, f.h)));

	Rgb<byte> fg, bg;
	
	for(int r=0; r < h; r++)
	{
		Font::Mode m=Font::Normal;
		Rgb<byte> fg(255,255,255);
		Rgb<byte> bg(0,0,0);


		for(int c=0; c < w; c++)
		{
			int c = text[r][c];
			const Image<bool> *g;

			if(c < 32)
			{
				//Blank glyph

				g = f.get_glyph(32,Normal,Normal);
				if(c>=1 && c <=7)
				{
					fg.red   = (bool)(c&1) * 255;
					fg.green = (bool)(c&2) * 255;
					fg.blue  = (bool)(c&4) * 255;
				}
				else if(c >=17 && c <= 23)
				{
					fg.red   = (bool)(c&1) * 255;
					fg.green = (bool)(c&2) * 255;
					fg.blue  = (bool)(c&4) * 255;
					m = Font::Graphics;
				}
				else if(c == 25)
				{
					//Switch graphics type
					if(m != Font::Normal)
						m = Fone::Graphics;
				}
				else if(c == 26)
				{
					//Switch graphics type
					if(m != Font::Normal)
						m = Fone::ThinGraphics;
				}







		}
	}



}

