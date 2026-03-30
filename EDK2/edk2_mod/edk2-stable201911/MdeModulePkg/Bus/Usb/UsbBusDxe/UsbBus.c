/** @file

    Usb Bus Driver Binding and Bus IO Protocol.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbBus.h"

EFI_USB_IO_PROTOCOL mUsbIoProtocol = {
  UsbIoControlTransfer,
  UsbIoBulkTransfer,
  UsbIoAsyncInterruptTransfer,
  UsbIoSyncInterruptTransfer,
  UsbIoIsochronousTransfer,
  UsbIoAsyncIsochronousTransfer,
  UsbIoGetDeviceDescriptor,
  UsbIoGetActiveConfigDescriptor,
  UsbIoGetInterfaceDescriptor,
  UsbIoGetEndpointDescriptor,
  UsbIoGetStringDescriptor,
  UsbIoGetSupportedLanguages,
  UsbIoPortReset
};

EFI_DRIVER_BINDING_PROTOCOL mUsbBusDriverBinding = {
  UsbBusControllerDriverSupported,
  UsbBusControllerDriverStart,
  UsbBusControllerDriverStop,
  0xa,
  NULL,
  NULL
};

/**
  USB_IO function to execute a control transfer. This
  function will execute the USB transfer. If transfer
  successes, it will sync the internal state of USB bus
  with device state.

  @param  This                   The USB_IO instance
  @param  Request                The control transfer request
  @param  Direction              Direction for data stage
  @param  Timeout                The time to wait before timeout
  @param  Data                   The buffer holding the data
  @param  DataLength             Then length of the data
  @param  UsbStatus              USB result

  @retval EFI_INVALID_PARAMETER  The parameters are invalid
  @retval EFI_SUCCESS            The control transfer succeeded.
  @retval Others                 Failed to execute the transfer

**/
EFI_STATUS
EFIAPI
UsbIoControlTransfer (
  IN  EFI_USB_IO_PROTOCOL     *This,
  IN  EFI_USB_DEVICE_REQUEST  *Request,
  IN  EFI_USB_DATA_DIRECTION  Direction,
  IN  UINT32                  Timeout,
  IN  OUT VOID                *Data,      OPTIONAL
  IN  UINTN                   DataLength, OPTIONAL
  OUT UINT32                  *UsbStatus
  )
{
  USB_DEVICE              *Dev;
  USB_INTERFACE           *UsbIf;
  USB_ENDPOINT_DESC       *EpDesc;
  EFI_TPL                 OldTpl;
  EFI_STATUS              Status;
  UINTN                   RequestedDataLength;

  if (UsbStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf  = USB_INTERFACE_FROM_USBIO (This);
  Dev    = UsbIf->Device;

  RequestedDataLength = DataLength;
  Status = UsbHcControlTransfer (
             Dev->Bus,
             Dev->Address,
             Dev->Speed,
             Dev->MaxPacket0,
             Request,
             Direction,
             Data,
             &DataLength,
             (UINTN) Timeout,
             &Dev->Translator,
             UsbStatus
             );
  //
  // If the request completed sucessfully and the Direction of the request is
  // EfiUsbDataIn or EfiUsbDataOut, then make sure the actual number of bytes
  // transfered is the same as the number of bytes requested.  If a different
  // number of bytes were transfered, then return EFI_DEVICE_ERROR.
  //
  if (!EFI_ERROR (Status)) {
    if (Direction != EfiUsbNoData && DataLength != RequestedDataLength) {
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
    }
  }

  if (EFI_ERROR (Status) || (*UsbStatus != EFI_USB_NOERROR)) {
    //
    // Clear TT buffer when CTRL/BULK split transaction failes
    // Clear the TRANSLATOR TT buffer, not parent's buffer
    //
    ASSERT (Dev->Translator.TranslatorHubAddress < Dev->Bus->MaxDevices);
    if (Dev->Translator.TranslatorHubAddress != 0) {
      UsbHubCtrlClearTTBuffer (
        Dev->Bus->Devices[Dev->Translator.TranslatorHubAddress],
        Dev->Translator.TranslatorPortNumber,
        Dev->Address,
        0,
        USB_ENDPOINT_CONTROL
        );
    }

    goto ON_EXIT;
  }

  //
  // Some control transfer will change the device's internal
  // status, such as Set_Configuration and Set_Interface.
  // We must synchronize the bus driver's status with that in
  // device. We ignore the Set_Descriptor request because it's
  // hardly used by any device, especially in pre-boot environment
  //

  //
  // Reset the endpoint toggle when endpoint stall is cleared
  //
  if ((Request->Request     == USB_REQ_CLEAR_FEATURE) &&
      (Request->RequestType == USB_REQUEST_TYPE (EfiUsbNoData, USB_REQ_TYPE_STANDARD,
                                                 USB_TARGET_ENDPOINT)) &&
      (Request->Value       == USB_FEATURE_ENDPOINT_HALT)) {

    EpDesc = UsbGetEndpointDesc (UsbIf, (UINT8) Request->Index);

    if (EpDesc != NULL) {
      EpDesc->Toggle = 0;
    }
  }

  //
  // Select a new configuration. This is a dangerous action. Upper driver
  // should stop use its current UsbIo after calling this driver. The old
  // UsbIo will be uninstalled and new UsbIo be installed. We can't use
  // ReinstallProtocol since interfaces in different configuration may be
  // completely irrelevant.
  //
  if ((Request->Request == USB_REQ_SET_CONFIG) &&
      (Request->RequestType == USB_REQUEST_TYPE (EfiUsbNoData, USB_REQ_TYPE_STANDARD,
                                                 USB_TARGET_DEVICE))) {
    //
    // Don't re-create the USB interfaces if configuration isn't changed.
    //
    if ((Dev->ActiveConfig != NULL) &&
        (Request->Value == Dev->ActiveConfig->Desc.ConfigurationValue)) {

      goto ON_EXIT;
    }
    DEBUG ((EFI_D_INFO, "UsbIoControlTransfer: configure changed!!! Do NOT use old UsbIo!!!\n"));

    if (Dev->ActiveConfig != NULL) {
      UsbRemoveConfig (Dev);
    }

    if (Request->Value != 0) {
      Status = UsbSelectConfig (Dev, (UINT8) Request->Value);
    }

    //
    // Exit now, Old USB_IO is invalid now
    //
    goto ON_EXIT;
  }

  //
  // A new alternative setting is selected for the interface.
  // No need to reinstall UsbIo in this case because only
  // underlying communication endpoints are changed. Functionality
  // should remains the same.
  //
  if ((Request->Request     == USB_REQ_SET_INTERFACE) &&
      (Request->RequestType == USB_REQUEST_TYPE (EfiUsbNoData, USB_REQ_TYPE_STANDARD,
                                                 USB_TARGET_INTERFACE)) &&
      (Request->Index       == UsbIf->IfSetting->Desc.InterfaceNumber)) {

    Status = UsbSelectSetting (UsbIf->IfDesc, (UINT8) Request->Value);

    if (!EFI_ERROR (Status)) {
      ASSERT (UsbIf->IfDesc->ActiveIndex < USB_MAX_INTERFACE_SETTING);
      UsbIf->IfSetting = UsbIf->IfDesc->Settings[UsbIf->IfDesc->ActiveIndex];
    }
  }

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


/**
  Execute a bulk transfer to the device endpoint.

  @param  This                   The USB IO instance.
  @param  Endpoint               The device endpoint.
  @param  Data                   The data to transfer.
  @param  DataLength             The length of the data to transfer.
  @param  Timeout                Time to wait before timeout.
  @param  UsbStatus              The result of USB transfer.

  @retval EFI_SUCCESS            The bulk transfer is OK.
  @retval EFI_INVALID_PARAMETER  Some parameters are invalid.
  @retval Others                 Failed to execute transfer, reason returned in
                                 UsbStatus.

**/
EFI_STATUS
EFIAPI
UsbIoBulkTransfer (
  IN  EFI_USB_IO_PROTOCOL *This,
  IN  UINT8               Endpoint,
  IN  OUT VOID            *Data,
  IN  OUT UINTN           *DataLength,
  IN  UINTN               Timeout,
  OUT UINT32              *UsbStatus
  )
{
  USB_DEVICE              *Dev;
  USB_INTERFACE           *UsbIf;
  USB_ENDPOINT_DESC       *EpDesc;
  UINT8                   BufNum;
  UINT8                   Toggle;
  VOID                    *DataMap[EFI_USB_MAX_BULK_BUFFER_NUM] = { NULL };
  EFI_TPL                 OldTpl;
  EFI_STATUS              Status;

  if ((USB_ENDPOINT_ADDR (Endpoint) == 0) || (USB_ENDPOINT_ADDR(Endpoint) > 15) ||
      (UsbStatus == NULL)) {

    return EFI_INVALID_PARAMETER;
  }

  OldTpl  = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf   = USB_INTERFACE_FROM_USBIO (This);
  Dev     = UsbIf->Device;

  EpDesc  = UsbGetEndpointDesc (UsbIf, Endpoint);

  if ((EpDesc == NULL) || (USB_ENDPOINT_TYPE (&EpDesc->Desc) != USB_ENDPOINT_BULK)) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  BufNum  = 1;
  Toggle  = EpDesc->Toggle;
  DataMap[0] = Data;
  Status  = UsbHcBulkTransfer (
              Dev->Bus,
              Dev->Address,
              Endpoint,
              Dev->Speed,
              EpDesc->Desc.MaxPacketSize,
              BufNum,
              DataMap,
              DataLength,
              &Toggle,
              Timeout,
              &Dev->Translator,
              UsbStatus
              );

  EpDesc->Toggle = Toggle;

  if (EFI_ERROR (Status) || (*UsbStatus != EFI_USB_NOERROR)) {
    //
    // Clear TT buffer when CTRL/BULK split transaction failes.
    // Clear the TRANSLATOR TT buffer, not parent's buffer
    //
    ASSERT (Dev->Translator.TranslatorHubAddress < Dev->Bus->MaxDevices);
    if (Dev->Translator.TranslatorHubAddress != 0) {
      UsbHubCtrlClearTTBuffer (
        Dev->Bus->Devices[Dev->Translator.TranslatorHubAddress],
        Dev->Translator.TranslatorPortNumber,
        Dev->Address,
        0,
        USB_ENDPOINT_BULK
        );
    }
  }

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


/**
  Execute a synchronous interrupt transfer.

  @param  This                   The USB IO instance.
  @param  Endpoint               The device endpoint.
  @param  Data                   The data to transfer.
  @param  DataLength             The length of the data to transfer.
  @param  Timeout                Time to wait before timeout.
  @param  UsbStatus              The result of USB transfer.

  @retval EFI_SUCCESS            The synchronous interrupt transfer is OK.
  @retval EFI_INVALID_PARAMETER  Some parameters are invalid.
  @retval Others                 Failed to execute transfer, reason returned in
                                 UsbStatus.

**/
EFI_STATUS
EFIAPI
UsbIoSyncInterruptTransfer (
  IN  EFI_USB_IO_PROTOCOL *This,
  IN  UINT8               Endpoint,
  IN  OUT VOID            *Data,
  IN  OUT UINTN           *DataLength,
  IN  UINTN               Timeout,
  OUT UINT32              *UsbStatus
  )
{
  USB_DEVICE              *Dev;
  USB_INTERFACE           *UsbIf;
  USB_ENDPOINT_DESC       *EpDesc;
  EFI_TPL                 OldTpl;
  UINT8                   Toggle;
  EFI_STATUS              Status;

  if ((USB_ENDPOINT_ADDR (Endpoint) == 0) || (USB_ENDPOINT_ADDR(Endpoint) > 15) ||
      (UsbStatus == NULL)) {

    return EFI_INVALID_PARAMETER;
  }

  OldTpl  = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf   = USB_INTERFACE_FROM_USBIO (This);
  Dev     = UsbIf->Device;

  EpDesc  = UsbGetEndpointDesc (UsbIf, Endpoint);

  if ((EpDesc == NULL) || (USB_ENDPOINT_TYPE (&EpDesc->Desc) != USB_ENDPOINT_INTERRUPT)) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  Toggle = EpDesc->Toggle;
  Status = UsbHcSyncInterruptTransfer (
             Dev->Bus,
             Dev->Address,
             Endpoint,
             Dev->Speed,
             EpDesc->Desc.MaxPacketSize,
             Data,
             DataLength,
             &Toggle,
             Timeout,
             &Dev->Translator,
             UsbStatus
             );

  EpDesc->Toggle = Toggle;

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


/**
  Queue a new asynchronous interrupt transfer, or remove the old
  request if (IsNewTransfer == FALSE).

  @param  This                   The USB_IO instance.
  @param  Endpoint               The device endpoint.
  @param  IsNewTransfer          Whether this is a new request, if it's old, remove
                                 the request.
  @param  PollInterval           The interval to poll the transfer result, (in ms).
  @param  DataLength             The length of perodic data transfer.
  @param  Callback               The function to call periodicaly when transfer is
                                 ready.
  @param  Context                The context to the callback.

  @retval EFI_SUCCESS            New transfer is queued or old request is removed.
  @retval EFI_INVALID_PARAMETER  Some parameters are invalid.
  @retval Others                 Failed to queue the new request or remove the old
                                 request.

**/
EFI_STATUS
EFIAPI
UsbIoAsyncInterruptTransfer (
  IN EFI_USB_IO_PROTOCOL              *This,
  IN UINT8                            Endpoint,
  IN BOOLEAN                          IsNewTransfer,
  IN UINTN                            PollInterval,       OPTIONAL
  IN UINTN                            DataLength,         OPTIONAL
  IN EFI_ASYNC_USB_TRANSFER_CALLBACK  Callback,           OPTIONAL
  IN VOID                             *Context            OPTIONAL
  )
{
  USB_DEVICE              *Dev;
  USB_INTERFACE           *UsbIf;
  USB_ENDPOINT_DESC       *EpDesc;
  EFI_TPL                 OldTpl;
  UINT8                   Toggle;
  EFI_STATUS              Status;

  if ((USB_ENDPOINT_ADDR (Endpoint) == 0) || (USB_ENDPOINT_ADDR (Endpoint) > 15)) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl  = gBS->RaiseTPL (USB_BUS_TPL);
  UsbIf   = USB_INTERFACE_FROM_USBIO (This);
  Dev     = UsbIf->Device;

  EpDesc  = UsbGetEndpointDesc (UsbIf, Endpoint);

  if ((EpDesc == NULL) || (USB_ENDPOINT_TYPE (&EpDesc->Desc) != USB_ENDPOINT_INTERRUPT)) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  Toggle  = EpDesc->Toggle;
  Status  = UsbHcAsyncInterruptTransfer (
              Dev->Bus,
              Dev->Address,
              Endpoint,
              Dev->Speed,
              EpDesc->Desc.MaxPacketSize,
              IsNewTransfer,
              &Toggle,
              PollInterval,
              DataLength,
              &Dev->Translator,
              Callback,
              Context
              );

  EpDesc->Toggle = Toggle;

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


/**
  Execute a synchronous isochronous transfer.

  @param  This                   The USB IO instance.
  @param  DeviceEndpoint         The device endpoint.
  @param  Data                   The data to transfer.
  @param  DataLength             The length of the data to transfer.
  @param  UsbStatus              The result of USB transfer.

  @retval EFI_UNSUPPORTED        Currently isochronous transfer isn't supported.

**/
EFI_STATUS
EFIAPI
UsbIoIsochronousTransfer (
  IN  EFI_USB_IO_PROTOCOL *This,
  IN  UINT8               DeviceEndpoint,
  IN  OUT VOID            *Data,
  IN  UINTN               DataLength,
  OUT UINT32              *Status
  )
{
  return EFI_UNSUPPORTED;
}


/**
  Queue an asynchronous isochronous transfer.

  @param  This                   The USB_IO instance.
  @param  DeviceEndpoint         The device endpoint.
  @param  Data                   The data to transfer.
  @param  DataLength             The length of perodic data transfer.
  @param  IsochronousCallBack    The function to call periodicaly when transfer is
                                 ready.
  @param  Context                The context to the callback.

  @retval EFI_UNSUPPORTED        Currently isochronous transfer isn't supported.

**/
EFI_STATUS
EFIAPI
UsbIoAsyncIsochronousTransfer (
  IN EFI_USB_IO_PROTOCOL              *This,
  IN UINT8                            DeviceEndpoint,
  IN OUT VOID                         *Data,
  IN UINTN                            DataLength,
  IN EFI_ASYNC_USB_TRANSFER_CALLBACK  IsochronousCallBack,
  IN VOID                             *Context              OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}


/**
  Retrieve the device descriptor of the device.

  @param  This                   The USB IO instance.
  @param  Descriptor             The variable to receive the device descriptor.

  @retval EFI_SUCCESS            The device descriptor is returned.
  @retval EFI_INVALID_PARAMETER  The parameter is invalid.

**/
EFI_STATUS
EFIAPI
UsbIoGetDeviceDescriptor (
  IN  EFI_USB_IO_PROTOCOL       *This,
  OUT EFI_USB_DEVICE_DESCRIPTOR *Descriptor
  )
{
  USB_DEVICE              *Dev;
  USB_INTERFACE           *UsbIf;
  EFI_TPL                 OldTpl;

  if (Descriptor == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf  = USB_INTERFACE_FROM_USBIO (This);
  Dev    = UsbIf->Device;

  CopyMem (Descriptor, &Dev->DevDesc->Desc, sizeof (EFI_USB_DEVICE_DESCRIPTOR));

  gBS->RestoreTPL (OldTpl);
  return EFI_SUCCESS;
}


/**
  Return the configuration descriptor of the current active configuration.

  @param  This                   The USB IO instance.
  @param  Descriptor             The USB configuration descriptor.

  @retval EFI_SUCCESS            The active configuration descriptor is returned.
  @retval EFI_INVALID_PARAMETER  Some parameter is invalid.
  @retval EFI_NOT_FOUND          Currently no active configuration is selected.

**/
EFI_STATUS
EFIAPI
UsbIoGetActiveConfigDescriptor (
  IN  EFI_USB_IO_PROTOCOL       *This,
  OUT EFI_USB_CONFIG_DESCRIPTOR *Descriptor
  )
{
  USB_DEVICE              *Dev;
  USB_INTERFACE           *UsbIf;
  EFI_STATUS              Status;
  EFI_TPL                 OldTpl;

  if (Descriptor == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  OldTpl = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf  = USB_INTERFACE_FROM_USBIO (This);
  Dev    = UsbIf->Device;

  if (Dev->ActiveConfig == NULL) {
    Status = EFI_NOT_FOUND;
    goto ON_EXIT;
  }

  CopyMem (Descriptor, &(Dev->ActiveConfig->Desc), sizeof (EFI_USB_CONFIG_DESCRIPTOR));

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


/**
  Retrieve the active interface setting descriptor for this USB IO instance.

  @param  This                   The USB IO instance.
  @param  Descriptor             The variable to receive active interface setting.

  @retval EFI_SUCCESS            The active interface setting is returned.
  @retval EFI_INVALID_PARAMETER  Some parameter is invalid.

**/
EFI_STATUS
EFIAPI
UsbIoGetInterfaceDescriptor (
  IN  EFI_USB_IO_PROTOCOL           *This,
  OUT EFI_USB_INTERFACE_DESCRIPTOR  *Descriptor
  )
{
  USB_INTERFACE           *UsbIf;
  EFI_TPL                 OldTpl;

  if (Descriptor == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf  = USB_INTERFACE_FROM_USBIO (This);
  CopyMem (Descriptor, &(UsbIf->IfSetting->Desc), sizeof (EFI_USB_INTERFACE_DESCRIPTOR));

  gBS->RestoreTPL (OldTpl);
  return EFI_SUCCESS;
}


/**
  Retrieve the endpoint descriptor from this interface setting.

  @param  This                   The USB IO instance.
  @param  Index                  The index (start from zero) of the endpoint to
                                 retrieve.
  @param  Descriptor             The variable to receive the descriptor.

  @retval EFI_SUCCESS            The endpoint descriptor is returned.
  @retval EFI_INVALID_PARAMETER  Some parameter is invalid.

**/
EFI_STATUS
EFIAPI
UsbIoGetEndpointDescriptor (
  IN  EFI_USB_IO_PROTOCOL         *This,
  IN  UINT8                       Index,
  OUT EFI_USB_ENDPOINT_DESCRIPTOR *Descriptor
  )
{
  USB_INTERFACE           *UsbIf;
  EFI_TPL                 OldTpl;

  OldTpl = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf  = USB_INTERFACE_FROM_USBIO (This);

  if ((Descriptor == NULL) || (Index > 15)) {
    gBS->RestoreTPL (OldTpl);
    return EFI_INVALID_PARAMETER;
  }

  if (Index >= UsbIf->IfSetting->Desc.NumEndpoints) {
    gBS->RestoreTPL (OldTpl);
    return EFI_NOT_FOUND;
  }

  CopyMem (
    Descriptor,
    &(UsbIf->IfSetting->Endpoints[Index]->Desc),
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR)
    );

  gBS->RestoreTPL (OldTpl);
  return EFI_SUCCESS;
}


/**
  Retrieve the supported language ID table from the device.

  @param  This                   The USB IO instance.
  @param  LangIDTable            The table to return the language IDs.
  @param  TableSize              The size, in bytes, of the table LangIDTable.

  @retval EFI_SUCCESS            The language ID is return.

**/
EFI_STATUS
EFIAPI
UsbIoGetSupportedLanguages (
  IN  EFI_USB_IO_PROTOCOL *This,
  OUT UINT16              **LangIDTable,
  OUT UINT16              *TableSize
  )
{
  USB_DEVICE              *Dev;
  USB_INTERFACE           *UsbIf;
  EFI_TPL                 OldTpl;

  OldTpl        = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf         = USB_INTERFACE_FROM_USBIO (This);
  Dev           = UsbIf->Device;

  *LangIDTable  = Dev->LangId;
  *TableSize    = (UINT16) (Dev->TotalLangId * sizeof (UINT16));

  gBS->RestoreTPL (OldTpl);
  return EFI_SUCCESS;
}


/**
  Retrieve an indexed string in the language of LangID.

  @param  This                   The USB IO instance.
  @param  LangID                 The language ID of the string to retrieve.
  @param  StringIndex            The index of the string.
  @param  String                 The variable to receive the string.

  @retval EFI_SUCCESS            The string is returned.
  @retval EFI_NOT_FOUND          No such string existed.

**/
EFI_STATUS
EFIAPI
UsbIoGetStringDescriptor (
  IN  EFI_USB_IO_PROTOCOL   *This,
  IN  UINT16                LangID,
  IN  UINT8                 StringIndex,
  OUT CHAR16                **String
  )
{
  USB_DEVICE                *Dev;
  USB_INTERFACE             *UsbIf;
  EFI_USB_STRING_DESCRIPTOR *StrDesc;
  EFI_TPL                   OldTpl;
  UINT8                     *Buf;
  UINT8                     Index;
  EFI_STATUS                Status;

  if ((StringIndex == 0) || (LangID == 0)) {
    return EFI_NOT_FOUND;
  }

  OldTpl = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf  = USB_INTERFACE_FROM_USBIO (This);
  Dev    = UsbIf->Device;

  //
  // Check whether language ID is supported
  //
  Status = EFI_NOT_FOUND;

  for (Index = 0; Index < Dev->TotalLangId; Index++) {
    ASSERT (Index < USB_MAX_LANG_ID);
    if (Dev->LangId[Index] == LangID) {
      break;
    }
  }

  if (Index == Dev->TotalLangId) {
    goto ON_EXIT;
  }

  //
  // Retrieve the string descriptor then allocate a buffer
  // to hold the string itself.
  //
  StrDesc = UsbGetOneString (Dev, StringIndex, LangID);

  if (StrDesc == NULL) {
    goto ON_EXIT;
  }

  if (StrDesc->Length <= 2) {
    goto FREE_STR;
  }

  Buf = AllocateZeroPool (StrDesc->Length);

  if (Buf == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FREE_STR;
  }

  CopyMem (Buf, StrDesc->String, StrDesc->Length - 2);
  *String = (CHAR16 *) Buf;
  Status  = EFI_SUCCESS;

FREE_STR:
  gBS->FreePool (StrDesc);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


/**
  Reset the device, then if that succeeds, reconfigure the
  device with its address and current active configuration.

  @param  This                   The USB IO instance.

  @retval EFI_SUCCESS            The device is reset and configured.
  @retval Others                 Failed to reset the device.

**/
EFI_STATUS
EFIAPI
UsbIoPortReset (
  IN EFI_USB_IO_PROTOCOL  *This
  )
{
  USB_INTERFACE           *UsbIf;
  USB_INTERFACE           *HubIf;
  USB_DEVICE              *Dev;
  EFI_TPL                 OldTpl;
  EFI_STATUS              Status;
  UINT8                   DevAddress;

  OldTpl = gBS->RaiseTPL (USB_BUS_TPL);

  UsbIf  = USB_INTERFACE_FROM_USBIO (This);
  Dev    = UsbIf->Device;

  if (UsbIf->IsHub) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  HubIf  = Dev->ParentIf;
  Status = HubIf->HubApi->ResetPort (HubIf, Dev->ParentPort);

  if (EFI_ERROR (Status)) {
    DEBUG (( EFI_D_ERROR, "UsbIoPortReset: failed to reset hub port %d@hub  %d, %r \n",
                Dev->ParentPort, Dev->ParentAddr, Status));

    goto ON_EXIT;
  }

  HubIf->HubApi->ClearPortChange (HubIf, Dev->ParentPort);

  //
  // Reset the device to its current address. The device now has an address
  // of ZERO after port reset, so need to set Dev->Address to the device again for
  // host to communicate with it.
  //
  DevAddress   = Dev->Address;
  Dev->Address = 0;
  Status  = UsbSetAddress (Dev, DevAddress);
  Dev->Address = DevAddress;

  gBS->Stall (USB_SET_DEVICE_ADDRESS_STALL);

  if (EFI_ERROR (Status)) {
    //
    // It may fail due to device disconnection or other reasons.
    //
    DEBUG (( EFI_D_ERROR, "UsbIoPortReset: failed to set address for device %d - %r\n",
                Dev->Address, Status));

    goto ON_EXIT;
  }

  DEBUG (( EFI_D_INFO, "UsbIoPortReset: device is now ADDRESSED at %d\n", Dev->Address));

  //
  // Reset the current active configure, after this device
  // is in CONFIGURED state.
  //
  if (Dev->ActiveConfig != NULL) {
    Status = UsbSetConfig (Dev, Dev->ActiveConfig->Desc.ConfigurationValue);

    if (EFI_ERROR (Status)) {
      DEBUG (( EFI_D_ERROR, "UsbIoPortReset: failed to set configure for device %d - %r\n",
                  Dev->Address, Status));
    }
  }

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


/**
  Install Usb Bus Protocol on host controller, and start the Usb bus.

  @param This                    The USB bus driver binding instance.
  @param Controller              The controller to check.
  @param RemainingDevicePath     The remaining device patch.

  @retval EFI_SUCCESS            The controller is controlled by the usb bus.
  @retval EFI_ALREADY_STARTED    The controller is already controlled by the usb bus.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate resources.

**/
EFI_STATUS
EFIAPI
UsbBusBuildProtocol (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  USB_BUS                 *UsbBus;
  USB_DEVICE              *RootHub;
  USB_INTERFACE           *RootIf;
  EFI_STATUS              Status;
  EFI_STATUS              Status2;

  UsbBus = AllocateZeroPool (sizeof (USB_BUS));

  if (UsbBus == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  UsbBus->Signature  = USB_BUS_SIGNATURE;
  UsbBus->HostHandle = Controller;
  UsbBus->MaxDevices = USB_MAX_DEVICES;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &UsbBus->DevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "UsbBusStart: Failed to open device path %r\n", Status));

    FreePool (UsbBus);
    return Status;
  }

  //
  // Get USB_HC2/USB_HC host controller protocol (EHCI/UHCI).
  // This is for backward compatibility with EFI 1.x. In UEFI
  // 2.x, USB_HC2 replaces USB_HC. We will open both USB_HC2
  // and USB_HC because EHCI driver will install both protocols
  // (for the same reason). If we don't consume both of them,
  // the unconsumed one may be opened by others.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsb2HcProtocolGuid,
                  (VOID **) &(UsbBus->Usb2Hc),
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  Status2 = gBS->OpenProtocol (
                   Controller,
                   &gEfiUsbHcProtocolGuid,
                   (VOID **) &(UsbBus->UsbHc),
                   This->DriverBindingHandle,
                   Controller,
                   EFI_OPEN_PROTOCOL_BY_DRIVER
                   );

  if (EFI_ERROR (Status) && EFI_ERROR (Status2)) {
    DEBUG ((EFI_D_ERROR, "UsbBusStart: Failed to open USB_HC/USB2_HC %r\n", Status));

    Status = EFI_DEVICE_ERROR;
    goto CLOSE_HC;
  }

  if (!EFI_ERROR (Status)) {
    //
    // The EFI_USB2_HC_PROTOCOL is produced for XHCI support.
    // Then its max supported devices are 256. Otherwise it's 128.
    //
    ASSERT (UsbBus->Usb2Hc != NULL);
    if (UsbBus->Usb2Hc->MajorRevision == 0x3) {
      UsbBus->MaxDevices = 256;
    }
  }

  //
  // Install an EFI_USB_BUS_PROTOCOL to host controller to identify it.
  //
  Status = gBS->InstallProtocolInterface (
                  &Controller,
                  &gEfiCallerIdGuid,
                  EFI_NATIVE_INTERFACE,
                  &UsbBus->BusId
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "UsbBusStart: Failed to install bus protocol %r\n", Status));
    goto CLOSE_HC;
  }

  //
  // Initial the wanted child device path list, and add first RemainingDevicePath
  //
  InitializeListHead (&UsbBus->WantedUsbIoDPList);
  Status = UsbBusAddWantedUsbIoDP (&UsbBus->BusId, RemainingDevicePath);
  ASSERT (!EFI_ERROR (Status));
  //
  // Create a fake usb device for root hub
  //
  RootHub = AllocateZeroPool (sizeof (USB_DEVICE));

  if (RootHub == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto UNINSTALL_USBBUS;
  }

  RootIf = AllocateZeroPool (sizeof (USB_INTERFACE));

  if (RootIf == NULL) {
    FreePool (RootHub);
    Status = EFI_OUT_OF_RESOURCES;
    goto FREE_ROOTHUB;
  }

  RootHub->Bus            = UsbBus;
  RootHub->NumOfInterface = 1;
  RootHub->Interfaces[0]  = RootIf;
  RootHub->Tier           = 0;
  RootIf->Signature       = USB_INTERFACE_SIGNATURE;
  RootIf->Device          = RootHub;
  RootIf->DevicePath      = UsbBus->DevicePath;

  //
  // Report Status Code here since we will enumerate the USB devices
  //
  REPORT_STATUS_CODE_WITH_DEVICE_PATH (
    EFI_PROGRESS_CODE,
    (EFI_IO_BUS_USB | EFI_IOB_PC_DETECT),
    UsbBus->DevicePath
    );

  Status                  = mUsbRootHubApi.Init (RootIf);

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "UsbBusStart: Failed to init root hub %r\n", Status));
    goto FREE_ROOTHUB;
  }

  UsbBus->Devices[0] = RootHub;

  DEBUG ((EFI_D_INFO, "UsbBusStart: usb bus started on %p, root hub %p\n", Controller, RootIf));
  return EFI_SUCCESS;

FREE_ROOTHUB:
  if (RootIf != NULL) {
    FreePool (RootIf);
  }
  if (RootHub != NULL) {
    FreePool (RootHub);
  }

UNINSTALL_USBBUS:
  gBS->UninstallProtocolInterface (Controller, &gEfiCallerIdGuid, &UsbBus->BusId);

CLOSE_HC:
  if (UsbBus->Usb2Hc != NULL) {
    gBS->CloseProtocol (
          Controller,
          &gEfiUsb2HcProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );
  }
  if (UsbBus->UsbHc != NULL) {
    gBS->CloseProtocol (
          Controller,
          &gEfiUsbHcProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );
  }
  gBS->CloseProtocol (
         Controller,
         &gEfiDevicePathProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );
  FreePool (UsbBus);

  DEBUG ((EFI_D_ERROR, "UsbBusStart: Failed to start bus driver %r\n", Status));
  return Status;
}


/**
  The USB bus driver entry pointer.

  @param ImageHandle       The driver image handle.
  @param SystemTable       The system table.

  @return EFI_SUCCESS      The component name protocol is installed.
  @return Others           Failed to init the usb driver.

**/
EFI_STATUS
EFIAPI
UsbBusDriverEntryPoint (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  return EfiLibInstallDriverBindingComponentName2 (
           ImageHandle,
           SystemTable,
           &mUsbBusDriverBinding,
           ImageHandle,
           &mUsbBusComponentName,
           &mUsbBusComponentName2
           );
}


/**
  Check whether USB bus driver support this device.

  @param  This                   The USB bus driver binding protocol.
  @param  Controller             The controller handle to check.
  @param  RemainingDevicePath    The remaining device path.

  @retval EFI_SUCCESS            The bus supports this controller.
  @retval EFI_UNSUPPORTED        This device isn't supported.

**/
EFI_STATUS
EFIAPI
UsbBusControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_DEV_PATH_PTR          DevicePathNode;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_USB2_HC_PROTOCOL      *Usb2Hc;
  EFI_USB_HC_PROTOCOL       *UsbHc;
  EFI_STATUS                Status;

  //
  // Check whether device path is valid
  //
  if (RemainingDevicePath != NULL) {
    //
    // Check if RemainingDevicePath is the End of Device Path Node,
    // if yes, go on checking other conditions
    //
    if (!IsDevicePathEnd (RemainingDevicePath)) {
      //
      // If RemainingDevicePath isn't the End of Device Path Node,
      // check its validation
      //
      DevicePathNode.DevPath = RemainingDevicePath;

      if ((DevicePathNode.DevPath->Type    != MESSAGING_DEVICE_PATH) ||
          (DevicePathNode.DevPath->SubType != MSG_USB_DP &&
           DevicePathNode.DevPath->SubType != MSG_USB_CLASS_DP
           && DevicePathNode.DevPath->SubType != MSG_USB_WWID_DP
           )) {

        return EFI_UNSUPPORTED;
      }
    }
  }

  //
  // Check whether USB_HC2 protocol is installed
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsb2HcProtocolGuid,
                  (VOID **) &Usb2Hc,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    //
    // If failed to open USB_HC2, fall back to USB_HC
    //
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiUsbHcProtocolGuid,
                    (VOID **) &UsbHc,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (Status == EFI_ALREADY_STARTED) {
      return EFI_SUCCESS;
    }

    if (EFI_ERROR (Status)) {
      return Status;
    }

    //
    // Close the USB_HC used to perform the supported test
    //
    gBS->CloseProtocol (
          Controller,
          &gEfiUsbHcProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );

  } else {

    //
    // Close the USB_HC2 used to perform the supported test
    //
    gBS->CloseProtocol (
           Controller,
           &gEfiUsb2HcProtocolGuid,
           This->DriverBindingHandle,
           Controller
           );
  }

  //
  // Open the EFI Device Path protocol needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (!EFI_ERROR (Status)) {
    //
    // Close protocol, don't use device path protocol in the Support() function
    //
    gBS->CloseProtocol (
          Controller,
          &gEfiDevicePathProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );

    return EFI_SUCCESS;
  }

  return Status;
}


/**
  Start to process the controller.

  @param  This                   The USB bus driver binding instance.
  @param  Controller             The controller to check.
  @param  RemainingDevicePath    The remaining device patch.

  @retval EFI_SUCCESS            The controller is controlled by the usb bus.
  @retval EFI_ALREADY_STARTED    The controller is already controlled by the usb
                                 bus.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate resources.

**/
EFI_STATUS
EFIAPI
UsbBusControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_USB_BUS_PROTOCOL          *UsbBusId;
  EFI_STATUS                    Status;
  EFI_DEVICE_PATH_PROTOCOL      *ParentDevicePath;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Report Status Code here since we will initialize the host controller
  //
  REPORT_STATUS_CODE_WITH_DEVICE_PATH (
    EFI_PROGRESS_CODE,
    (EFI_IO_BUS_USB | EFI_IOB_PC_INIT),
    ParentDevicePath
    );

  //
  // Locate the USB bus protocol, if it is found, USB bus
  // is already started on this controller.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiCallerIdGuid,
                  (VOID **) &UsbBusId,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    //
    // If first start, build the bus execute environment and install bus protocol
    //
    REPORT_STATUS_CODE (EFI_PROGRESS_CODE, (EFI_IO_BUS_USB | EFI_P_PC_ENABLE));
    Status = UsbBusBuildProtocol (This, Controller, RemainingDevicePath);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    //
    // Try get the Usb Bus protocol interface again
    //
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    (VOID **) &UsbBusId,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    ASSERT (!EFI_ERROR (Status));
  } else {
    //
    // USB Bus driver need to control the recursive connect policy of the bus, only those wanted
    // usb child device will be recursively connected.
    // The RemainingDevicePath indicate the child usb device which user want to fully recursively connecte this time.
    // All wanted usb child devices will be remembered by the usb bus driver itself.
    // If RemainingDevicePath == NULL, all the usb child devices in the usb bus are wanted devices.
    //
    // Save the passed in RemainingDevicePath this time
    //
    if (RemainingDevicePath != NULL) {
      if (IsDevicePathEnd (RemainingDevicePath)) {
        //
        // If RemainingDevicePath is the End of Device Path Node,
        // skip enumerate any device and return EFI_SUCESSS
        //
        return EFI_SUCCESS;
      }
    }

    Status = UsbBusAddWantedUsbIoDP (UsbBusId, RemainingDevicePath);
    ASSERT (!EFI_ERROR (Status));
    //
    // Ensure all wanted child usb devices are fully recursively connected
    //
    Status = UsbBusRecursivelyConnectWantedUsbIo (UsbBusId);
    ASSERT (!EFI_ERROR (Status));
  }


  return EFI_SUCCESS;
}


/**
  Stop handle the controller by this USB bus driver.

  @param  This                   The USB bus driver binding protocol.
  @param  Controller             The controller to release.
  @param  NumberOfChildren       The child of USB bus that opened controller
                                 BY_CHILD.
  @param  ChildHandleBuffer      The array of child handle.

  @retval EFI_SUCCESS            The controller or children are stopped.
  @retval EFI_DEVICE_ERROR       Failed to stop the driver.

**/
EFI_STATUS
EFIAPI
UsbBusControllerDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  )
{
  USB_BUS               *Bus;
  USB_DEVICE            *RootHub;
  USB_DEVICE            *UsbDev;
  USB_INTERFACE         *RootIf;
  USB_INTERFACE         *UsbIf;
  EFI_USB_BUS_PROTOCOL  *BusId;
  EFI_USB_IO_PROTOCOL   *UsbIo;
  EFI_TPL               OldTpl;
  UINTN                 Index;
  EFI_STATUS            Status;
  EFI_STATUS            ReturnStatus;

  Status  = EFI_SUCCESS;

  if (NumberOfChildren > 0) {
    //
    // BugBug: Raise TPL to callback level instead of USB_BUS_TPL to avoid TPL conflict
    //
    OldTpl   = gBS->RaiseTPL (TPL_CALLBACK);

    ReturnStatus = EFI_SUCCESS;
    for (Index = 0; Index < NumberOfChildren; Index++) {
      Status = gBS->OpenProtocol (
                      ChildHandleBuffer[Index],
                      &gEfiUsbIoProtocolGuid,
                      (VOID **) &UsbIo,
                      This->DriverBindingHandle,
                      Controller,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );

      if (EFI_ERROR (Status)) {
        //
        // It is possible that the child has already been released:
        // 1. For combo device, free one device will release others.
        // 2. If a hub is released, all devices on its down facing
        //    ports are released also.
        //
        continue;
      }

      UsbIf   = USB_INTERFACE_FROM_USBIO (UsbIo);
      UsbDev  = UsbIf->Device;

      ReturnStatus = UsbRemoveDevice (UsbDev);
    }

    gBS->RestoreTPL (OldTpl);
    return ReturnStatus;
  }

  DEBUG (( EFI_D_INFO, "UsbBusStop: usb bus stopped on %p\n", Controller));

  //
  // Locate USB_BUS for the current host controller
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiCallerIdGuid,
                  (VOID **) &BusId,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Bus = USB_BUS_FROM_THIS (BusId);

  //
  // Stop the root hub, then free all the devices
  //
  // BugBug: Raise TPL to callback level instead of USB_BUS_TPL to avoid TPL conflict
  //
  OldTpl  = gBS->RaiseTPL (TPL_CALLBACK);

  RootHub = Bus->Devices[0];
  RootIf  = RootHub->Interfaces[0];

  ASSERT (Bus->MaxDevices <= 256);
  ReturnStatus = EFI_SUCCESS;
  for (Index = 1; Index < Bus->MaxDevices; Index++) {
    if (Bus->Devices[Index] != NULL) {
      Status = UsbRemoveDevice (Bus->Devices[Index]);
      if (EFI_ERROR (Status)) {
        ReturnStatus = Status;
      }
    }
  }

  gBS->RestoreTPL (OldTpl);

  if (!EFI_ERROR (ReturnStatus)) {
    mUsbRootHubApi.Release (RootIf);
    gBS->FreePool   (RootIf);
    gBS->FreePool   (RootHub);

    Status = UsbBusFreeUsbDPList (&Bus->WantedUsbIoDPList);
    ASSERT (!EFI_ERROR (Status));

    //
    // Uninstall the bus identifier and close USB_HC/USB2_HC protocols
    //
    gBS->UninstallProtocolInterface (Controller, &gEfiCallerIdGuid, &Bus->BusId);

    if (Bus->Usb2Hc != NULL) {
      Status = gBS->CloseProtocol (
                      Controller,
                      &gEfiUsb2HcProtocolGuid,
                      This->DriverBindingHandle,
                      Controller
                      );
    }

    if (Bus->UsbHc != NULL) {
      Status = gBS->CloseProtocol (
                      Controller,
                      &gEfiUsbHcProtocolGuid,
                      This->DriverBindingHandle,
                      Controller
                      );
    }

    if (!EFI_ERROR (Status)) {
      gBS->CloseProtocol (
             Controller,
             &gEfiDevicePathProtocolGuid,
             This->DriverBindingHandle,
             Controller
             );

      gBS->FreePool (Bus);
    }
  }
  return Status;
}
