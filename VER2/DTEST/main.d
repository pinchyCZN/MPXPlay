import core.stdc.stdlib;
import core.stdc.stdio;
import core.stdc.string;

import test;

@nogc
int main(string[] args)
{
	if(args.length<2)
		return 0;

	const char *fname=args[1].ptr;
	test_d(fname);

	return 0;		
}
