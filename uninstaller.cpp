#include <cstdio>
__declspec(dllimport) void UnInstall();

int main()
{
	UnInstall();
	puts("press enter to exit");
	(void)getchar();
}
