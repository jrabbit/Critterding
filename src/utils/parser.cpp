#include "parser.h"

Parser::Parser()
{
}

string Parser::returnUntill(string stop, string &line)
{
	string result;
	size_t pos = line.find_first_of( stop, 0 );
	if ( pos != string::npos ) result = line.substr( 0, pos );

	return result;
}

string Parser::returnUntillStrip(string stop, string &line)
{
	string result;
	size_t pos = line.find_first_of( stop, 0 );
	if ( pos != string::npos ) result = line.substr( 0, pos );

	// strip result from line
	line = line.substr( pos+1, line.size() );
	return result;
}

bool Parser::beginMatchesStrip(string stop, string &line)
{
	if ( line.substr( 0, stop.size() ) == stop )
	{
		line = line.substr( stop.size(), line.size() );
		return true;
	}
	else return false;
}

Parser::~Parser()
{
}


