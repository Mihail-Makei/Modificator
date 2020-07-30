#include <ntddk.h>
#include <ntddmou.h>

PDEVICE_OBJECT PointerDeviceObject = NULL;
ULONG PendingKey = 0;

typedef struct {
	PDEVICE_OBJECT LowerPointerDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

/**typedef struct _MOUSE_INPUT_DATA {
	USHORT UnitId;
	USHORT Flags;
	union {
		ULONG Buttons;
		struct {
			USHORT ButtonFlags;
			USHORT ButtonData;
		};
	};
	ULONG  RawButtons;
	LONG   LastX;
	LONG   LastY;
	ULONG  ExtraInformation;
} MOUSE_INPUT_DATA, * PMOUSE_INPUT_DATA;*/

NTSTATUS DispatchPass(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

NTSTATUS AttachDevice(PDRIVER_OBJECT DriverObject);

VOID UnloadDriver(PDRIVER_OBJECT DriverObject);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	DbgPrint("Loaded!\r\n");

	DriverObject->DriverUnload = UnloadDriver;

	for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
		DriverObject->MajorFunction[i] = DispatchPass;

	DriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;

	NTSTATUS status = AttachDevice(DriverObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Device not attached!\r\n");

		return status;
	}

	DbgPrint("Attached!\r\n");

	return STATUS_SUCCESS;
}

VOID UnloadDriver(PDRIVER_OBJECT DriverObject) {
	IoDetachDevice(((PDEVICE_EXTENSION)DriverObject->DeviceObject->DeviceExtension)->LowerPointerDevice);

	LARGE_INTEGER interval = { 0 };
	interval.QuadPart = -10 * 1000 * 1000;
	while (PendingKey) {
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}

	IoDeleteDevice(PointerDeviceObject);

	DbgPrint("Unloaded!\r\n");
}

NTSTATUS AttachDevice(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING TargetDeviceName = RTL_CONSTANT_STRING(L"\\Device\\PointerClass0");

	NTSTATUS status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), NULL, FILE_DEVICE_KEYBOARD, NULL, FALSE, &PointerDeviceObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Device creation failed!\r\n");

		return status;
	}

	PointerDeviceObject->Flags |= DO_BUFFERED_IO;
	PointerDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	RtlZeroMemory(PointerDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION));

	status = IoAttachDevice(PointerDeviceObject, &TargetDeviceName, &((PDEVICE_EXTENSION)PointerDeviceObject->DeviceExtension)->LowerPointerDevice);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Not attached!\r\n");
		
		IoDeleteDevice(PointerDeviceObject);
		
		return status;
	}

	DbgPrint("WOW!\r\n");

	return STATUS_SUCCESS;
}

NTSTATUS DispatchPass(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	IoCopyCurrentIrpStackLocationToNext(Irp);
	DbgPrint("DispatchPass\r\n");
	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerPointerDevice, Irp);
}

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	IoCopyCurrentIrpStackLocationToNext(Irp);
	
	IoSetCompletionRoutine(Irp, ReadComplete, NULL, TRUE, TRUE, TRUE);

	++PendingKey;

	DbgPrint("DispatchRead\r\n");
	
	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerPointerDevice, Irp);
}

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
	if (Irp->IoStatus.Status == STATUS_SUCCESS) {
		ULONG ReqNumber = Irp->IoStatus.Information / sizeof(MOUSE_INPUT_DATA);
		PMOUSE_INPUT_DATA Keys = Irp->AssociatedIrp.SystemBuffer;

		for (int i = 0; i < ReqNumber; ++i) {
			switch (Keys[i].ButtonFlags) {
			case MOUSE_LEFT_BUTTON_DOWN:
				DbgPrint("Left button pressed\r\n");
				break;

			case MOUSE_LEFT_BUTTON_UP:
				DbgPrint("Left button released\r\n");
				break;

			case MOUSE_RIGHT_BUTTON_DOWN:
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