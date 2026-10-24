// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: HARDWARE.C

#include "u8port.h"
#include "plat.h"

#include "error.h"
#include "cprotmem.h"
#include "hardware.h"

static char *hardwareErrors[5] =
{
	"\n\r\n\rERROR:  You must have at least %ldK bytes of hard drive space.\n\r"
	"ERREUR: Il vous faut au moins %ld octets disponibles sur le disque dur.\n\r"
	"FEHLER: Sie ben\x94tigen mindestens %ldK Bytes Festplattenspeicher.\n\r",
	"\n\r\n\rERROR:  You must have at least 3580K bytes of free memory.\n\r"
	"ERREUR: Il vous faut au moins 3580K d'octets do m\x82moire disponbile.\n\r"
	"FEHLER: Sie ben\x94tigen mindestens 3580K Bytes Speicherkapazit\x84t.\n\r",
	"\n\r\n\rERROR:  The existing memory is too fragmented.\n\r"
	"ERREUR: La m\x82moire existante est trop fragment\x82" "e.\n\r"
	"FEHLER: Die verf\x81gbare Speicherkapazit\x84t ist zu fragmentiert.\n\r",
	"\n\r\n\rERROR:  Pagan will not run on a 8088 or 80286 processor.\n\r"
	"ERREUR: Pagan ne peut \x88tre jou\x82 sur un microprocessor 8088 ou 80286.\n\r"
	"FEHLER: Pagan l\x84\xe1t sich nicht mit 8088 oder 80286 Prozessoren betreiben.\n\r",
	"\n\r\n\rERROR:  You must have a Microsoft(tm) compatible mouse driver installed.\n\r"
	"ERREUR: Il faut vous installer un gestionnaire souris.\n\r"
	"FEHLER: Kein Microsoft(tm) kompatibler Maustreiber gefunden.\n\r"
};

HardwareChecker::HardwareChecker(int32_t diskSpace, int32_t memory, int32_t largestFragment, CPU cpu)
{
	minDiskSpace = diskSpace;
	minMemory = memory;
	minLargestFragment = largestFragment;
	minCpu = cpu;
}

// Free bytes on a drive (0 = current).
int32_t HardwareChecker::getHardDriveSpace(int16_t drive)
{
	// DOS get free disk space (int 21h function 36h).
	return (int32_t)plat_disk_free();
}

int32_t HardwareChecker::getTotalMemory(void)
{
	uint32_t total, largest;
	uint16_t selectors;

	ProtMemoryManager::getMemStatus(total, largest, selectors);
	return total;
}

int32_t HardwareChecker::getLargestMemoryFragment(void)
{
	uint32_t total, largest;
	uint16_t selectors;

	ProtMemoryManager::getMemStatus(total, largest, selectors);
	return largest;
}

// An 8086 pushes sp after decrementing it; a 286 cannot set flag bits 12-13. Every host is
// at least a 386.
int16_t HardwareChecker::getCpu(void)
{
	return CPU_386;
}

// int 33h function 0: the platform always has a pointer.
uint8_t HardwareChecker::mouseInstalled(void)
{
	return 1;
}

uint8_t HardwareChecker::verifyAll(uint8_t verbose)
{
	uint8_t ok = 1;

	if (getHardDriveSpace(0) < minDiskSpace)
	{
		if (verbose)
			trace(TRACE_ALWAYS, hardwareErrors[0], minDiskSpace / 1024);
		ok = 0;
	}
	if (getTotalMemory() < minMemory)
	{
		if (verbose)
			trace(TRACE_ALWAYS, hardwareErrors[1]);
		ok = 0;
	}
	if (getLargestMemoryFragment() < minLargestFragment)
	{
		if (verbose)
			trace(TRACE_ALWAYS, hardwareErrors[2]);
		ok = 0;
	}
	if (getCpu() < minCpu)
	{
		if (verbose)
			trace(TRACE_ALWAYS, hardwareErrors[3]);
		ok = 0;
	}
	if (!mouseInstalled())
	{
		if (verbose)
			trace(TRACE_ALWAYS, hardwareErrors[4]);
		ok = 0;
	}
	return ok;
}
