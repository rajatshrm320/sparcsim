#include <loader.h>



/*
 * Initializes loader by returning a handle
 * to the ELF binary after opening.
 */
int initializeLoader(char *elfBinary)
{
	static int fileDescriptor = RET_FAILURE;
	if(elfBinary == NULL)
	{
		close(fileDescriptor);
		return -1;
	}
	else
	{
		if((fileDescriptor = open(elfBinary, O_RDWR)) == RET_FAILURE)
			return ELF_FILE_DOES_NOT_EXIST_ERROR;
		else
			return fileDescriptor;
	}
}



/*
 * Extracts out relevant sections (e.g. .text, .data, .bss, .rodata)
 * from ELF binary and loads to appropriate memory locations starting
 * from section entry point address as specified by sh_addr member of
 * GElf_Shdr structure. Returns an array of structure, each structure
 * carrying the information, viz. section name, entry point address,
 * size, type, number of instructions etc. The method makes use of
 * LIBELF API (licensed under LGPL) developed by Michael Riepe. LIBELF
 * follows BSD 3.0 specification for ELF manipulation.
 */
struct loadedSections* load_sparc_instructions(char *elfBinary)
{
	GElf_Ehdr elf_header;		// ELF header
	GElf_Shdr shdr;                 // Section Header
	Elf_Scn* scn = NULL;            // Section Descriptor
	Elf_Data* sectionData;          // Data Descriptor
	unsigned long instructionCount;
	int fileDescriptor;
	struct loadedSections* elfSections, *elfSectionsPrevPtr, *elfSectionCurPtr;

	instructionCount = 0;
	elfSections = (struct loadedSections*)malloc(sizeof(struct loadedSections));
	if(elfSections == NULL)
	{
		elfSections->sectionType = ELF_SECTION_LOAD_ERROR;
		return elfSections;
	}
	elfSectionCurPtr = elfSections;
	fileDescriptor = initializeLoader(elfBinary);
	if(fileDescriptor == ELF_FILE_DOES_NOT_EXIST_ERROR)
	{
		elfSections->sectionType = ELF_FILE_DOES_NOT_EXIST_ERROR;
		return elfSections;
	}
	
	// Check libelf version
	if(elf_version(EV_CURRENT) == EV_NONE)
	{
		elfSections->sectionType = ELF_BINARY_OUT_OF_DATE;
		return elfSections;
	}

	Elf* elf = elf_begin(fileDescriptor, ELF_C_READ, NULL);
	if(elf == NULL)
	{
		elfSections->sectionType = ELF_POINTER_INITIALIZATION_ERROR;
		return elfSections;
	}

	gelf_getehdr(elf, &elf_header);

	// Iterate over section headers
	while((scn = elf_nextscn(elf, scn)) != 0)
	{
		gelf_getshdr(scn, &shdr);
		size_t sectionDataByteCounter;

		if(((shdr.sh_flags & SHF_EXECINSTR) && (shdr.sh_flags & SHF_ALLOC)) ||  // .text section
			((shdr.sh_flags & SHF_WRITE) && (shdr.sh_flags & SHF_ALLOC)))		// .data & .bss sections
		{
			sectionData = NULL;
			sectionDataByteCounter = 0;
			unsigned long sectionLoadAddress = shdr.sh_addr;
			while((sectionDataByteCounter < shdr.sh_size) && ((sectionData = elf_getdata (scn, sectionData)) != NULL))
			{
				char* sectionDataBuffer = (char*)sectionData -> d_buf;
				if(sectionDataBuffer == NULL)
					break;
				while (sectionDataBuffer < (char*)sectionData-> d_buf + sectionData -> d_size )
				{
					writeByte(sectionLoadAddress, *sectionDataBuffer); sectionLoadAddress++; sectionDataBuffer++;
					writeByte(sectionLoadAddress, *sectionDataBuffer); sectionLoadAddress++; sectionDataBuffer++;
					writeByte(sectionLoadAddress, *sectionDataBuffer); sectionLoadAddress++; sectionDataBuffer++;
					writeByte(sectionLoadAddress, *sectionDataBuffer); sectionLoadAddress++; sectionDataBuffer++;
					sectionDataByteCounter += 4;
					instructionCount++;
				}
			}

			// Store section information
			strcpy(elfSectionCurPtr->sectionName, elf_strptr(elf, elf_header.e_shstrndx, shdr.sh_name));
			elfSectionCurPtr->sectionLoadAddress = shdr.sh_addr;
			if(sectionData != NULL)
				elfSectionCurPtr->sectionSize = sectionData->d_size;
			else
				elfSectionCurPtr->sectionSize = 0;
			elfSectionCurPtr->instructionCount = instructionCount;
			if((shdr.sh_flags & SHF_EXECINSTR) && (shdr.sh_flags & SHF_ALLOC))
				elfSectionCurPtr->sectionType = CODE_SECTION;
			else
				elfSectionCurPtr->sectionType = DATA_SECTION;
			elfSectionCurPtr->nextSection = (struct loadedSections*)malloc(sizeof(struct loadedSections));
			if(elfSectionCurPtr->nextSection == NULL)
			{
				elfSections->sectionType = ELF_SECTION_LOAD_ERROR;
				return elfSections;
			}
			elfSectionsPrevPtr = elfSectionCurPtr;
			elfSectionCurPtr = elfSectionCurPtr->nextSection;
		}
	}

	free(elfSectionCurPtr);
	elfSectionsPrevPtr->nextSection = NULL;
	return elfSections;
}
