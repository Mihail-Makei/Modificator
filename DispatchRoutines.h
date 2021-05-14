#pragma once
#include "Config.h"

NTSTATUS DispatchPass(PDEVICE_OBJECT DeviceObject, PIRP Irp);				// Passing IRPs we're not interested in

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);				// Handling IRP_MJ_READ

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context); // Completion routine

NTSTATUS AttachDevice(PDRIVER_OBJECT DriverObject);							// Attaching device routine