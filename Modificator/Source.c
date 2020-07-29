#include <ntddk.h>

VOID UnloadDriver(PDRIVER_OBJECT DriverObject) {
	DbgPrint("Unloaded!\r\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	DbgPrint("Loaded!\r\n");

	DriverObject->DriverUnload = UnloadDriver;
	
	return STATUS_SUCCESS;
}