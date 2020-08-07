#include <ntddk.h>															// ntddk library for kernel development
#include <ntddmou.h>

#define LEFT MOUSE_LEFT_BUTTON_DOWN											// Just making define shorter
#define RIGHT MOUSE_RIGHT_BUTTON_DOWN										//
#define COMBO_LENGTH 5														// Length of combination

PDEVICE_OBJECT PointerDeviceObject = NULL;									// Pointer to device object created by our filter driver
ULONG PendingKey = 0;														// Special number so as to avoid IRP loss

const USHORT SecretCombo[5] = {LEFT, RIGHT, RIGHT, LEFT, LEFT};				// Combination
USHORT Counter = 0;															// Counter of combination checker
BOOLEAN ReversedY = FALSE;													// See if Y axis is inverted

typedef struct {															// Device extension containing only lower pointer device
	PDEVICE_OBJECT LowerPointerDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


NTSTATUS DispatchPass(PDEVICE_OBJECT DeviceObject, PIRP Irp);				// Passing IRPs we're not interested in

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);				// Handling IRP_MJ_READ

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context); // Completion routine

NTSTATUS AttachDevice(PDRIVER_OBJECT DriverObject);							// Attaching device routine

VOID UnloadDriver(PDRIVER_OBJECT DriverObject);								// Unloading driver

#pragma

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) { // DriverEntry routine
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
		KeDelayExecutionThread(KernelMode, FALSE, &interval);				// Delay until all IRPs are passed
	}

	IoDeleteDevice(PointerDeviceObject);

	DbgPrint("Unloaded!\r\n");
}

NTSTATUS AttachDevice(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING TargetDeviceName = RTL_CONSTANT_STRING(L"\\Device\\PointerClass0"); // Target device name

	NTSTATUS status = IoCreateDevice(										// Trying to create device
		DriverObject,
		sizeof(DEVICE_EXTENSION),
		NULL,
		FILE_DEVICE_KEYBOARD,
		NULL,
		FALSE,
		&PointerDeviceObject
	);
	
	if (!NT_SUCCESS(status)) {												// Check if device created
		DbgPrint("Device creation failed!\r\n");

		return status;
	}

	PointerDeviceObject->Flags |= DO_BUFFERED_IO;							// Setting all the necessary flags
	PointerDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	RtlZeroMemory(PointerDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION)); // Allocating memory for device extension

	status = IoAttachDevice(												// Trying to attach to target device
		PointerDeviceObject, 
		&TargetDeviceName, 
		&((PDEVICE_EXTENSION)PointerDeviceObject->DeviceExtension)->LowerPointerDevice
	);
	if (!NT_SUCCESS(status)) {												// Check if attached
		DbgPrint("Not attached!\r\n");
		
		IoDeleteDevice(PointerDeviceObject);
		
		return status;
	}

	DbgPrint("WOW!\r\n");

	return STATUS_SUCCESS;
}

NTSTATUS DispatchPass(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	IoCopyCurrentIrpStackLocationToNext(Irp);								// Just ignore the request
	DbgPrint("DispatchPass\r\n");
	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerPointerDevice, Irp);
}

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	IoCopyCurrentIrpStackLocationToNext(Irp);
	
	IoSetCompletionRoutine(Irp, ReadComplete, NULL, TRUE, TRUE, TRUE);		// Set ReadComplete routine

	++PendingKey;															// Increase PendingKey

	
	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerPointerDevice, Irp);
}

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
	if (Irp->IoStatus.Status == STATUS_SUCCESS) {
		ULONG ReqNumber = Irp->IoStatus.Information / sizeof(MOUSE_INPUT_DATA);	// Count the number of IRPs
		PMOUSE_INPUT_DATA Keys = Irp->AssociatedIrp.SystemBuffer;			// Take all the IRP structures

		for (int i = 0; i < ReqNumber; ++i) {
			if (ReversedY) {												// If reversed
				DbgPrint("TRUE\r\n");

				if (Keys[i].Flags & MOUSE_MOVE_ABSOLUTE) {					// 
					Keys[i].LastY = 0xFFFF - Keys[i].LastY;

					DbgPrint("Absolute! Last X %d\tLast Y %d\r\n", Keys[i].LastX, Keys[i].LastY);
				}
				else
					Keys[i].LastY *= -1;
			}
			
			switch (Keys[i].ButtonFlags) {
				case MOUSE_LEFT_BUTTON_DOWN:
					if (!ReversedY && SecretCombo[Counter] == LEFT) {
						++Counter;

					if (Counter == COMBO_LENGTH)
						ReversedY = TRUE;
					} else if (!ReversedY && SecretCombo[Counter] == RIGHT)
						Counter = 0;
				
					DbgPrint("Left button pressed Counter %d\r\n", Counter);

					break;

				case MOUSE_LEFT_BUTTON_UP:
					DbgPrint("Left button released\r\n");
					break;

				case MOUSE_RIGHT_BUTTON_DOWN:
					if (!ReversedY && SecretCombo[Counter] == RIGHT) {
						++Counter;

					if (Counter == COMBO_LENGTH)
						ReversedY = TRUE;
					}
					else if (!ReversedY && SecretCombo[Counter] == LEFT)
						Counter = 0;

					DbgPrint("Right button pressed\r\n");
					break;

				case MOUSE_RIGHT_BUTTON_UP:
					DbgPrint("Right button released\r\n");
					break;
			}
		}

		if (Irp->PendingReturned)
			IoMarkIrpPending(Irp);

		--PendingKey;

		return Irp->IoStatus.Status;
	}
}