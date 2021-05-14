#pragma once
#include <ntddk.h>															// ntddk library for kernel development
#include <ntddmou.h>

#define LEFT MOUSE_LEFT_BUTTON_DOWN											// Just making define shorter
#define RIGHT MOUSE_RIGHT_BUTTON_DOWN										//
#define COMBO_LENGTH 5														// Length of combination


typedef struct {															// Device extension containing only lower pointer device
	PDEVICE_OBJECT LowerPointerDevice;
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;