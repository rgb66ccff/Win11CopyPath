#include <cstdio>
__declspec(dllimport) void Install();

int main()
{
	Install();
	puts("press enter to exit");
	(void)getchar();
}
