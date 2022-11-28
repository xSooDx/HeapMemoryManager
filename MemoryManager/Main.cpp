#include "HeapManagerProxy.h"
#include <crtdbg.h>

#include <stdlib.h>
#include <stdio.h>

int main()
{
	using namespace	HeapManagerProxy;

	HeapManager_UnitTest();

	_CrtDumpMemoryLeaks();

}