#include "Config.h"
#include "DispatchRoutines.h"
PDEVICE_OBJECT PointerDeviceObject = NULL;									// Pointer to device object created by our filter driver
ULONG PendingKey = 0;														// Special number so as to avoid IRP loss
USHORT Counter = 0;															// Counter of combination checker
BOOLEAN ReversedY = FALSE;													// See if Y axis is inverted
const USHORT SecretCombo[4] = { LEFT, RIGHT, RIGHT, LEFT };					// Combination

VOID UnloadDriver(PDRIVER_OBJECT DriverObject);								// Unloading driver

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) { // DriverEntry routine
	UNREFERENCED_PARAMETER(RegistryPath);

	DbgPrint("Loaded!\r\n");

	DriverObject->DriverUnload = UnloadDriver;								// Setting unload function

	for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
		DriverObject->MajorFunction[i] = DispatchPass;						// Setting major functions

	DriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;

	NTSTATUS status = AttachDevice(DriverObject);							// Attaching all the devices
	if (!NT_SUCCESS(status)) {
		DbgPrint("Device not attached!\r\n");

		return status;
	}

	DbgPrint("Attached!\r\n");

	return STATUS_SUCCESS;
}

VOID UnloadDriver(PDRIVER_OBJECT DriverObject) {
	IoDetachDevice(((PDEVICE_EXTENSION)DriverObject->DeviceObject->DeviceExtension)->LowerPointerDevice); // Detaching device

	LARGE_INTEGER interval = { 0 };
	interval.QuadPart = -10 * 1000 * 1000;
	while (PendingKey) {
		KeDelayExecutionThread(												// Delay until all IRPs are passed
			KernelMode, 
			FALSE, 
			&interval
		);				
	}

	IoDeleteDevice(PointerDeviceObject);

	DbgPrint("Unloaded!\r\n");
}
