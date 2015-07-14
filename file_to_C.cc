#include <iostream>
#include <iomanip>
#include <cctype>
using namespace std;

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		cerr << "Pleas supply string name\n";
		return 1;
	}
	
	cout << "#include <string>\n";
	cout << "const char* " << argv[1] << "_ = \"";
	
	int c;
	int n=0;
	const int w=30;
	do
	{
		c=cin.get();
		if(!cin.good())
			break;
		
		if(n % w == 0)
			cout << "\"\n\"";

		cout << "\\x" << hex  << c;
		n++;
	}
	while(true);
	cout << "\";\n" << dec << setw(0) << setfill(' ');
	cout << "const std::string " <<  argv[1] << "() { return std::string(" << argv[1] << "_, " << n << ");}\n";
}
