/*
PV Drivers for Windows Xen HVM Domains
Copyright (C) 2007 James Harper
Copyright (C) 2007 Andrew Grover <andy.grover@oracle.com>

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

#pragma warning(disable: 4201)
#pragma warning(disable: 4214)

#include <wdm.h>
#include <wdf.h>
#include <wdfminiport.h>
#include <initguid.h>
#define NDIS_MINIPORT_DRIVER
#if NTDDI_VERSION < NTDDI_WINXP
# define NDIS50_MINIPORT 1
#else
# define NDIS51_MINIPORT 1
#endif
#include <ndis.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#define __DRIVER_NAME "XenNet"

#include <xen_windows.h>
#include <memory.h>
#include <grant_table.h>
#include <event_channel.h>
#include <hvm/params.h>
#include <hvm/hvm_op.h>
#include <xen_public.h>
#include <io/ring.h>
#include <io/netif.h>
#include <io/xenbus.h>
#include <stdlib.h>
#define XENNET_POOL_TAG (ULONG) 'XenN'

/* Xen macros use these, so they need to be redefined to Win equivs */
#define wmb() KeMemoryBarrier()
#define mb() KeMemoryBarrier()

#define GRANT_INVALID_REF 0

#define NAME_SIZE 64

#define ETH_ALEN 6

/* couldn't get regular xen ring macros to work...*/
#define __NET_RING_SIZE(type, _sz) \
    (__RD32( \
    (_sz - sizeof(struct type##_sring) + sizeof(union type##_sring_entry)) \
    / sizeof(union type##_sring_entry)))

#define NET_TX_RING_SIZE __NET_RING_SIZE(netif_tx, PAGE_SIZE)
#define NET_RX_RING_SIZE __NET_RING_SIZE(netif_rx, PAGE_SIZE)

#pragma warning(disable: 4127) // conditional expression is constant

//#define XEN_PROFILE

/* TODO: crank this up if we support higher mtus? */
#define XN_DATA_SIZE 1500
#define XN_HDR_SIZE 14
#define XN_MIN_PKT_SIZE 60
#define XN_MAX_PKT_SIZE (XN_HDR_SIZE + XN_DATA_SIZE)

#define XN_MAX_SEND_PKTS 16

#define XN_RX_QUEUE_LEN 256
#define XENSOURCE_MAC_HDR 0x00163E
#define XN_VENDOR_DESC "Xensource"
#define MAX_XENBUS_STR_LEN 128

struct xennet_info
{
  /* Base device vars */
  PDEVICE_OBJECT pdo;
  PDEVICE_OBJECT fdo;
  PDEVICE_OBJECT lower_do;
  WDFDEVICE wdf_device;
  WCHAR dev_desc[NAME_SIZE];

  /* NDIS-related vars */
  NDIS_HANDLE adapter_handle;
  NDIS_HANDLE packet_pool;
  NDIS_HANDLE buffer_pool;
  ULONG packet_filter;
  int connected;
  UINT8 perm_mac_addr[ETH_ALEN];
  UINT8 curr_mac_addr[ETH_ALEN];

  /* Misc. Xen vars */
  XEN_IFACE XenInterface;
  PXENPCI_XEN_DEVICE_DATA pdo_data;
  evtchn_port_t event_channel;
  ULONG state;
  KEVENT backend_state_change_event;
  KEVENT shutdown_event;
  char backend_path[MAX_XENBUS_STR_LEN];
  ULONG backend_state;

  /* Xen ring-related vars */
  KSPIN_LOCK rx_lock;

  /* tx related - protected by tx_lock */
  KSPIN_LOCK tx_lock;
  LIST_ENTRY tx_waiting_pkt_list;
  struct netif_tx_front_ring tx;
  grant_ref_t tx_ring_ref;
  struct netif_tx_sring *tx_pgs;
  PMDL tx_mdl;
  ULONG tx_id_free;
  USHORT tx_id_list[NET_TX_RING_SIZE];
  ULONG tx_gref_free;
  grant_ref_t tx_gref_list[NET_TX_RING_SIZE];
  PNDIS_PACKET tx_pkts[NET_TX_RING_SIZE];
  grant_ref_t tx_grefs[NET_TX_RING_SIZE];

  /* rx_related - protected by rx_lock */
  struct netif_rx_front_ring rx;
  grant_ref_t rx_ring_ref;
  struct netif_rx_sring *rx_pgs;
  PMDL rx_mdl;
  USHORT rx_id_list[NET_RX_RING_SIZE];
  ULONG rx_id_free;
  PNDIS_BUFFER rx_buffers[NET_RX_RING_SIZE];
  PMDL page_list[NET_RX_RING_SIZE];
  ULONG page_free;

  /* Receive-ring batched refills. */
#define RX_MIN_TARGET 8
#define RX_DFL_MIN_TARGET 64
#define RX_MAX_TARGET min(NET_RX_RING_SIZE, 256)
  ULONG rx_target;
  ULONG rx_max_target;
  ULONG rx_min_target;

  /* how many packets are in the net stack atm */
  LONG rx_outstanding;
  LONG tx_outstanding;

  /* stats */
  ULONG64 stat_tx_ok;
  ULONG64 stat_rx_ok;
  ULONG64 stat_tx_error;
  ULONG64 stat_rx_error;
  ULONG64 stat_rx_no_buffer;
} typedef xennet_info_t;


extern LARGE_INTEGER ProfTime_TxBufferGC;
extern LARGE_INTEGER ProfTime_TxBufferFree;
extern LARGE_INTEGER ProfTime_RxBufferAlloc;
extern LARGE_INTEGER ProfTime_RxBufferFree;
extern LARGE_INTEGER ProfTime_ReturnPacket;
extern LARGE_INTEGER ProfTime_RxBufferCheck;
extern LARGE_INTEGER ProfTime_Linearize;
extern LARGE_INTEGER ProfTime_SendPackets;
extern LARGE_INTEGER ProfTime_SendQueuedPackets;

extern int ProfCount_TxBufferGC;
extern int ProfCount_TxBufferFree;
extern int ProfCount_RxBufferAlloc;
extern int ProfCount_RxBufferFree;
extern int ProfCount_ReturnPacket;
extern int ProfCount_RxBufferCheck;
extern int ProfCount_Linearize;
extern int ProfCount_SendPackets;
extern int ProfCount_PacketsPerSendPackets;
extern int ProfCount_SendQueuedPackets;

extern int ProfCount_TxPacketsTotal;
extern int ProfCount_TxPacketsOffload;
extern int ProfCount_RxPacketsTotal;
extern int ProfCount_RxPacketsOffload;

NDIS_STATUS
XenNet_RxBufferCheck(struct xennet_info *xi);

VOID
XenNet_ReturnPacket(
  IN NDIS_HANDLE MiniportAdapterContext,
  IN PNDIS_PACKET Packet
  );

BOOLEAN
XenNet_RxInit(xennet_info_t *xi);

BOOLEAN
XenNet_RxShutdown(xennet_info_t *xi);

NDIS_STATUS
XenNet_TxBufferGC(struct xennet_info *xi);

VOID
XenNet_SendPackets(
  IN NDIS_HANDLE MiniportAdapterContext,
  IN PPNDIS_PACKET PacketArray,
  IN UINT NumberOfPackets
  );

BOOLEAN
XenNet_TxInit(xennet_info_t *xi);

BOOLEAN
XenNet_TxShutdown(xennet_info_t *xi);

NDIS_STATUS 
XenNet_QueryInformation(
  IN NDIS_HANDLE MiniportAdapterContext,
  IN NDIS_OID Oid,
  IN PVOID InformationBuffer,
  IN ULONG InformationBufferLength,
  OUT PULONG BytesWritten,
  OUT PULONG BytesNeeded);

NDIS_STATUS 
XenNet_SetInformation(
  IN NDIS_HANDLE MiniportAdapterContext,
  IN NDIS_OID Oid,
  IN PVOID InformationBuffer,
  IN ULONG InformationBufferLength,
  OUT PULONG BytesRead,
  OUT PULONG BytesNeeded
  );
