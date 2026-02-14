#pragma once
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

namespace FileUtility
{
	int ReadStringFromFile(FILE* inFile, char* strToken);
	unsigned int ReadUnsignedIntegerFromFile(FILE* inFile);
	int ReadIntegerFromFile(FILE* inFile);
	float ReadFloatFromFile(FILE* inFile);
}
