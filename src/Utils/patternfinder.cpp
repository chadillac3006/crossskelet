#include "patternfinder.h"
#include "../hooker.h"

// original code by dom1n1k and Patrick at GameDeception
inline bool Compare(const unsigned char* pData, const unsigned char* bMask, const char* szMask)
{
	for (; *szMask; ++szMask, ++pData, ++bMask)
		if (*szMask == 'x' && *pData != *bMask)
			return false;

	return (*szMask) == 0;
}

uintptr_t PatternFinder::FindPattern(uintptr_t dwAddress, uintptr_t dwLen, unsigned char* bMask, const char* szMask)
{
	for (uintptr_t i = 0; i < dwLen; i++)
		if (Compare((unsigned char*)(dwAddress + i), bMask, szMask))
			return (uintptr_t)(dwAddress + i);

	return 0;
}

uintptr_t PatternFinder::FindPatternInModule(const char* moduleName, unsigned char* bMask, const char* szMask)
{
	uintptr_t baseAddress;
	size_t memSize;

	if (!Hooker::GetLibraryInformation(moduleName, &baseAddress, &memSize))
		return 0;
	return FindPattern(baseAddress, memSize, bMask, szMask);
}


/*
     * @brief Scan for a given byte pattern on a module
     *
     * @param module    Base of the module to search
     * @param signature IDA-style byte array pattern
     *
     * @returns Address of the first occurence
     */
std::uint8_t* PatternFinder::PatternScan(char* module, const char* signature)
{
    static auto pattern_to_byte = [](const char* pattern) {
	auto bytes = std::vector<int>{};
	auto start = const_cast<char*>(pattern);
	auto end = const_cast<char*>(pattern) + strlen(pattern);

	for (auto current = start; current < end; ++current)
	{
	    if (*current == '?')
	    {
		++current;
		if (*current == '?')
		    ++current;
		bytes.push_back(-1);
	    }
	    else
	    {
		bytes.push_back(strtoul(current, &current, 16));
	    }
	}
	return bytes;
    };

	unsigned int moduleptr = 0;
    size_t sizeOfImage = 0;

    Hooker::GetLibraryInformation(module, &moduleptr, &sizeOfImage);

    auto patternBytes = pattern_to_byte(signature);
    auto scanBytes = reinterpret_cast<std::uint8_t*>(moduleptr);

    auto s = patternBytes.size();
    auto d = patternBytes.data();

    for (auto i = 0ul; i < sizeOfImage - s; ++i)
    {
	bool found = true;
	for (auto j = 0ul; j < s; ++j)
	{
	    if (scanBytes[i + j] != d[j] && d[j] != -1)
	    {
		found = false;
		break;
	    }
	}
	if (found)
	{
	    return &scanBytes[i];
	}
    }
    return nullptr;
}
