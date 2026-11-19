#include "FileUtility.h"

unsigned int FileUtility::ReadUnsignedIntegerFromFile(FILE* inFile)
{
	UINT nValue = 0;
	UINT nReads = (UINT)::fread(&nValue, sizeof(UINT), 1, inFile);
	return(nValue);
}

int FileUtility::ReadIntegerFromFile(FILE* inFile)
{
	int nValue = 0;
	UINT nReads = (UINT)::fread(&nValue, sizeof(int), 1, inFile);
	return(nValue);
}

float FileUtility::ReadFloatFromFile(FILE* inFile)
{
	float fValue = 0;
	UINT nReads = (UINT)::fread(&fValue, sizeof(float), 1, inFile);
	return(fValue);
}

int FileUtility::ReadStringFromFile(FILE* inFile, char* strToken)
{
	int nStrLength = 0;
	UINT nReads = 0;
	nReads = (UINT)::fread(&nStrLength, sizeof(int), 1, inFile);
	nReads = (UINT)::fread(strToken, sizeof(char), nStrLength, inFile);
	strToken[nStrLength] = '\0';

	return(nStrLength);
}
