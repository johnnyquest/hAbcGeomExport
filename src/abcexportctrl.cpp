/**
		@file		abcexportctrl.cpp
		@author		Imre Tuske
		@since		2011-12-13

		@brief		Wrapper cmd for abc export calls can't be made in python (==all :)).

*/

#include <UT/UT_DSOVersion.h>
#include <CMD/CMD_Args.h>
#include <CMD/CMD_Manager.h>

#include <string.h>
#include <malloc.h>
#include <math.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>


using namespace std;





/**		Callback for abcexportctrl.
*/
static void cmd_abcexportctrl( CMD_Args & args )
{
	if ( args.found('e') )
	{
		args.out() << "found -e option with: " << args.argp('e') << "...\n";

		args.err() << "this is an error (?)\n";
		args.showUsage();
	}

	args.out() << "arguments:\n";
	for(int i=0, m=args.argc();  i<m;  ++i)
	{
		args.out()
			<< i << ".: "
			<< args(i)
			<< "\n";
	}
}



/**		Install cmd.
*/
void CMDextendLibrary( CMD_Manager *cmgr )
{
	cmgr->installCommand("abcexportctrl", "e:", cmd_abcexportctrl);
}


