#include <iostream>
#include <array>
#include <cvd/image_io.h>
#include <cvd/gl_helpers.h>
#include <cvd/videodisplay.h>
#include <tag/printf.h>
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
			graphics.sub_image(ImageRef(  0,   0),ps).fill((bool)i&1);
			graphics.sub_image(ImageRef( sx,   0),ps).fill((bool)i&2);
			graphics.sub_image(ImageRef(  0,  sy),ps).fill((bool)i&4);
			graphics.sub_image(ImageRef( sx,  sy),ps).fill((bool)i&8);
			graphics.sub_image(ImageRef(  0,2*sy),ps).fill((bool)i&16);
			graphics.sub_image(ImageRef( sx,2*sy),ps).fill((bool)i&64);
			
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


int main()
{
	FontSet f("teletext.fnt");
	int w=40;
	int h=25;

	Image<byte> text(ImageRef(w,h));

	//Read every second character except control ones,
	//to that it renders OK in vim with ^M for a <13>
	text.fill(0);
	for(int r=0; r < w*h; r++)
	{
		int c = cin.get();
		if(r % w == 0 && c == '\n')
			c=cin.get();
			
		if(c>= 32)
			c=cin.get();
		
		if(!cin.good())
			break;
		text.data()[r] = c;

		cerr << "c = " << c << endl;
	}


	Image<Rgb<byte> > screen(text.size().dot_times(f.size()));

	Rgb<byte> fg, bg;

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
		const Image<bool>* last_graphic = &f.get_blank();

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
					cerr << "double height ftw!\n";
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
					glyph = last_graphic;
				else
					glyph = &f.get_blank();
			}
			else
			{
				
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
						c = 0; //A blank character
				}
				
				FontSet::Mode m = FontSet::Normal;
				if(graphics_on)
				{
					if(separated_graphics)
						m = FontSet::ThinGraphics;
					else
						m = FontSet::Graphics;
				}
				cerr << "Glyph: " << c << " " << m << " " << h << endl;	
				glyph = &f.get_glyph(c, m, h);
			}

			
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


	img_save(screen, cout, ImageType::PNM);
}


