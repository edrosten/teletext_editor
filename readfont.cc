#include <iostream>
#include <array>
#include <cvd/image_io.h>
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
		Upper,
		Lower,
		Graphics,
		ThinGraphics
	};

	Font(string filename);
	const Image<bool>& get_glyph(int i, Mode m)
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
		Image<bool> upper(ImageRef(10,18));
		Image<bool> lower(ImageRef(10,18));
		
		for(int y=0; y < h; y++)
			for(int x=0; x < w; x++)
			{	
				upper[y][x] = out[y/2][x];
				upper[y][x] = out[y/2][x];
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
		glyphs[i][Upper] = upper;
		glyphs[i][Lower] = upper;
		glyphs[i][Graphics] = graphics;
		glyphs[i][ThinGraphics] = thingraphics;
	}
}
