import core.stdc.stdlib;
import core.stdc.stdio;
import core.stdc.string;

@nogc
int main(string[] args)
{
	if(args.length<2)
		return 0;

	const char *fname=args[1].ptr;
	FILE *f=cast(FILE*)fopen(fname,"rb");
	if(f is null)
		return 0;

	return 0;		
}
