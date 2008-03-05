/*
PV Drivers for Windows Xen HVM Domains
Copyright (C) 2007 James Harper

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#if !defined(_XEN_PUBLIC_H_)
#define _XEN_PUBLIC_H_

//{5C568AC5-9DDF-4FA5-A94A-39D67077819C}
DEFINE_GUID(GUID_XEN_IFACE, 0x5C568AC5, 0x9DDF, 0x4FA5, 0xA9, 0x4A, 0x39, 0xD6, 0x70, 0x77, 0x81, 0x9C);

//{CD433FE7-954F-4D51-BE29-D8A38DFA1108}
DEFINE_GUID(GUID_XENHIDE_IFACE, 0xCD433FE7, 0x954F, 0x4D51, 0xBE, 0x29, 0xD8, 0xA3, 0x8D, 0xFA, 0x11, 0x08);

typedef PHYSICAL_ADDRESS
(*PXEN_ALLOCMMIO)(PVOID Context, ULONG Length);

typedef void
(*PXEN_FREEMEM)(PVOID Ptr);

typedef NTSTATUS
(*PXEN_EVTCHN_BIND)(PVOID Context, evtchn_port_t Port, PKSERVICE_ROUTINE ServiceRoutine, PVOID ServiceContext);

typedef NTSTATUS
(*PXEN_EVTCHN_UNBIND)(PVOID Context, evtchn_port_t Port);

typedef NTSTATUS
(*PXEN_EVTCHN_MASK)(PVOID Context, evtchn_port_t Port);

typedef NTSTATUS
(*PXEN_EVTCHN_UNMASK)(PVOID Context, evtchn_port_t Port);

typedef NTSTATUS
(*PXEN_EVTCHN_NOTIFY)(PVOID Context, evtchn_port_t Port);

typedef evtchn_port_t
(*PXEN_EVTCHN_ALLOCUNBOUND)(PVOID Context, domid_t Domain);

typedef grant_ref_t
(*PXEN_GNTTBL_GRANTACCESS)(WDFDEVICE Device, domid_t domid, uint32_t frame, int readonly, grant_ref_t ref);

typedef BOOLEAN
(*PXEN_GNTTBL_ENDACCESS)(WDFDEVICE Device, grant_ref_t ref, BOOLEAN keepref);

typedef VOID
(*PXEN_GNTTBL_PUTREF)(WDFDEVICE Device, grant_ref_t ref);

typedef grant_ref_t
(*PXEN_GNTTBL_GETREF)(WDFDEVICE Device);


typedef VOID
(*PXENBUS_WATCH_CALLBACK)(char *Path, PVOID ServiceContext);

typedef char *
(*PXEN_XENBUS_READ)(PVOID Context, xenbus_transaction_t xbt, const char *path, char **value);

typedef char *
(*PXEN_XENBUS_WRITE)(PVOID Context, xenbus_transaction_t xbt, const char *path, const char *value);

typedef char *
(*PXEN_XENBUS_PRINTF)(PVOID Context, xenbus_transaction_t xbt, const char *path, const char *fmt, ...);

typedef char *
(*PXEN_XENBUS_STARTTRANSACTION)(PVOID Context, xenbus_transaction_t *xbt);

typedef char *
(*PXEN_XENBUS_ENDTRANSACTION)(PVOID Context, xenbus_transaction_t t, int abort, int *retry);

typedef char *
(*PXEN_XENBUS_LIST)(PVOID Context, xenbus_transaction_t xbt, const char *prefix, char ***contents);

typedef char *
(*PXEN_XENBUS_ADDWATCH)(PVOID Context, xenbus_transaction_t xbt, const char *Path, PXENBUS_WATCH_CALLBACK ServiceRoutine, PVOID ServiceContext);

typedef char *
(*PXEN_XENBUS_REMWATCH)(PVOID Context, xenbus_transaction_t xbt, const char *Path, PXENBUS_WATCH_CALLBACK ServiceRoutine, PVOID ServiceContext);

typedef struct _XEN_IFACE {
  INTERFACE InterfaceHeader;

  PXEN_ALLOCMMIO AllocMMIO;
  PXEN_FREEMEM FreeMem;

  PXEN_EVTCHN_BIND EvtChn_Bind;
  PXEN_EVTCHN_UNBIND EvtChn_Unbind;
  PXEN_EVTCHN_MASK EvtChn_Mask;
  PXEN_EVTCHN_UNMASK EvtChn_Unmask;
  PXEN_EVTCHN_NOTIFY EvtChn_Notify;
  PXEN_EVTCHN_ALLOCUNBOUND EvtChn_AllocUnbound;
  PXEN_EVTCHN_BIND EvtChn_BindDpc;

  PXEN_GNTTBL_GETREF GntTbl_GetRef;
  PXEN_GNTTBL_PUTREF GntTbl_PutRef;
  PXEN_GNTTBL_GRANTACCESS GntTbl_GrantAccess;
  PXEN_GNTTBL_ENDACCESS GntTbl_EndAccess;

  PXEN_XENBUS_READ XenBus_Read;
  PXEN_XENBUS_WRITE XenBus_Write;
  PXEN_XENBUS_PRINTF XenBus_Printf;
  PXEN_XENBUS_STARTTRANSACTION XenBus_StartTransaction;
  PXEN_XENBUS_ENDTRANSACTION XenBus_EndTransaction;
  PXEN_XENBUS_LIST XenBus_List;
  PXEN_XENBUS_ADDWATCH XenBus_AddWatch;
  PXEN_XENBUS_REMWATCH XenBus_RemWatch;
} XEN_IFACE, *PXEN_IFACE;

#define XEN_DATA_MAGIC 0x12345678

typedef struct _XENPCI_IDENTIFICATION_DESCRIPTION
{
  WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header;
  char DeviceType[128]; //UNICODE_STRING DeviceType;
  char Path[128];
  ULONG DeviceIndex;
} XENPCI_IDENTIFICATION_DESCRIPTION, *PXENPCI_IDENTIFICATION_DESCRIPTION;

typedef struct {
  ULONG Magic;
  char Path[128];
  ULONG DeviceIndex;
  PXENBUS_WATCH_CALLBACK WatchHandler;
  PVOID WatchContext;
  XEN_IFACE XenInterface;
  BOOLEAN AutoEnumerate;
  CM_PARTIAL_RESOURCE_DESCRIPTOR InterruptRaw;
  CM_PARTIAL_RESOURCE_DESCRIPTOR InterruptTranslated;
} XENPCI_XEN_DEVICE_DATA, *PXENPCI_XEN_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(XENPCI_XEN_DEVICE_DATA, GetXenDeviceData);

#endif
