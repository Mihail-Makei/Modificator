#include "DispatchRoutines.h"

extern PDEVICE_OBJECT PointerDeviceObject;
extern ULONG PendingKey;
extern USHORT Counter;
extern BOOLEAN ReversedY;
extern const USHORT SecretCombo[5];

NTSTATUS AttachDevice(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING TargetDeviceName = RTL_CONSTANT_STRING(L"\\Device\\PointerClass0"); // Target device name

	NTSTATUS status = IoCreateDevice(										// Trying to create device
		DriverObject,
		sizeof(DEVICE_EXTENSION),
		NULL,
		FILE_DEVICE_KEYBOARD,
		0,
		FALSE,
		&PointerDeviceObject
	);

	if (!NT_SUCCESS(status)) {												// Check if device created
		DbgPrint("Device creation failed!\r\n");

		return status;
	}

	PointerDeviceObject->Flags |= DO_BUFFERED_IO;							// Setting all the necessary flags
	PointerDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	RtlZeroMemory(															// Allocating memory for device extension
		PointerDeviceObject->DeviceExtension, 
		sizeof(DEVICE_EXTENSION)
	);

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

	return STATUS_SUCCESS;
}

NTSTATUS DispatchPass(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	IoCopyCurrentIrpStackLocationToNext(Irp);								// Just ignore the request
	
	DbgPrint("DispatchPass\r\n");

	return IoCallDriver(
		((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerPointerDevice, 
		Irp
	);
}

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	IoCopyCurrentIrpStackLocationToNext(Irp);

	IoSetCompletionRoutine(													// Set ReadComplete routine
		Irp, 
		ReadComplete, 
		NULL, 
		TRUE, 
		TRUE, 
		TRUE
	);

	++PendingKey;															// Increase PendingKey

	return IoCallDriver(
		((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerPointerDevice, 
		Irp
	);
}

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);

	if (Irp->IoStatus.Status == STATUS_SUCCESS) {
		ULONG ReqNumber = (ULONG) Irp->IoStatus.Information / sizeof(MOUSE_INPUT_DATA);	// Count the number of IRPs
		PMOUSE_INPUT_DATA Keys = Irp->AssociatedIrp.SystemBuffer;			// Take all the IRP structures

		for (UINT32 i = 0; i < ReqNumber; ++i) {
			if (ReversedY) {												// If reversed
				DbgPrint("TRUE\r\n");

				if (Keys[i].Flags & MOUSE_MOVE_ABSOLUTE) {					// Reversing
					Keys[i].LastY = 0xFFFF - Keys[i].LastY;

					DbgPrint("Absolute! Last X %d\tLast Y %d\r\n", Keys[i].LastX, Keys[i].LastY);
				}
				else {
					Keys[i].LastY *= -1;
				}
			}

			switch (Keys[i].ButtonFlags) {
			case MOUSE_LEFT_BUTTON_DOWN:
				if (!ReversedY && SecretCombo[Counter] == LEFT) {
					++Counter;

					if (Counter == COMBO_LENGTH) {
						ReversedY = TRUE;
					}
				}
				else if (!ReversedY && SecretCombo[Counter] == RIGHT) {
					Counter = 0;
				}

				DbgPrint("Left button pressed Counter %d\r\n", Counter);

				break;

			case MOUSE_LEFT_BUTTON_UP:
				DbgPrint("Left button released\r\n");
				break;

			case MOUSE_RIGHT_BUTTON_DOWN:
				if (!ReversedY && SecretCombo[Counter] == RIGHT) {
					++Counter;

					if (Counter == COMBO_LENGTH) {
						ReversedY = TRUE;
					}
				} else if (!ReversedY && SecretCombo[Counter] == LEFT) {
					Counter = 0;
				}

				DbgPrint("Right button pressed\r\n");
				break;

			case MOUSE_RIGHT_BUTTON_UP:
				DbgPrint("Right button released\r\n");
				break;
			}
		}

		if (Irp->PendingReturned) {
			IoMarkIrpPending(Irp);
		}

		--PendingKey;

		return Irp->IoStatus.Status;
	}

	return Irp->IoStatus.Status;
}