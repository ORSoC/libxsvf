
#include <stdio.h>

int main(int argc, char **argv)
{
	char *id = "vfile";
	int ch;

	if (argc > 1)
		id = argv[1];

	printf("#ifndef VFILE_%s\n", id);
	printf("#define VFILE_%s\n", id);
	printf("#include <stdio.h>\n");

	printf("unsigned char vfile_data_%s[] = {", id);
	ch = getchar();
	if (ch != EOF) {
		printf(" 0x%02x", ch);
		while ((ch = getchar()) != EOF)
			printf(", 0x%02x", ch);
	}
	printf(" };\n");

	printf("FILE *vfile_open_%s() {\n  return fmemopen(vfile_data_%s, "
		"sizeof(vfile_data_%s), \"r\");\n}\n", id, id, id);

	printf("#endif /* VFILE_%s */\n", id);

	return 0;
}

