#include "dddvbci.h"

#include <libudev.h>

#include <vdr/dvbdevice.h>

// --- helper ----------------------------------------------------------------

static cString DvbDeviceName(const char *Name, const char *Adapter, const char *Device)
{
  return cString::sprintf("%s%s/%s%s", DEV_DVB_ADAPTER, Adapter, Name, Device);
}

static bool DvbDeviceNodeExists(const char *Name, const char *Adapter, const char *Device)
{
  cString fileName = DvbDeviceName(Name, Adapter, Device);
  if (access(fileName, F_OK) == 0) {
     int f = open(fileName, O_RDONLY);
     if (f >= 0) {
        close(f);
        return true;
        }
     }
  return false;
}

// --- cDdDvbCiAdapter -------------------------------------------------------

ddci::cDdDvbCiAdapter::cDdDvbCiAdapter(cDevice *Device, int Fd, const char *AdapterNum, const char *DeviceNum, const char *DevNode)
 :cDvbCiAdapter(Device, Fd)
 ,device(Device)
 ,devNode(DevNode)
 ,adapterNum(AdapterNum)
 ,deviceNum(DeviceNum)
{
  cDdDvbCiAdapterProbe::probedDevices.Append((char*)*devNode);
}

ddci::cDdDvbCiAdapter::~cDdDvbCiAdapter(void)
{
  int i = cDdDvbCiAdapterProbe::probedDevices.Find(*devNode);
  if (i >= 0)
     cDdDvbCiAdapterProbe::probedDevices.Remove(i);
}

cTSBuffer *ddci::cDdDvbCiAdapter::GetTSBuffer(int FdDvr)
{
  int fd_sec = open(*DvbDeviceName("sec", *adapterNum, *deviceNum), O_RDWR);
  if (fd_sec >= 0) {
     cTSTransfer *transfer = new cTSTransfer(FdDvr, fd_sec, device->CardIndex() + 1);
     return new cTSTransferBuffer(fd_sec, MEGABYTE(2), device->CardIndex() + 1, transfer);
     }
  return cDvbCiAdapter::GetTSBuffer(FdDvr);
}

// --- cDdDvbCiAdapterProbe --------------------------------------------------

cStringList  ddci::cDdDvbCiAdapterProbe::probedDevices;

cDvbCiAdapter *ddci::cDdDvbCiAdapterProbe::Probe(cDevice *Device)
{
  cDvbCiAdapter *ci = NULL;
  struct udev  *udev = udev_new();
  if (udev == NULL) {
     esyslog("ddci: can't get udev instance");
     return NULL;
     }

  struct udev_enumerate *e = udev_enumerate_new(udev);
  struct udev_list_entry *l;
  udev_device *dev = NULL;
  const char *syspath;
  const char *devnode;
  const char *adapterNum;
  const char *deviceNum;
  int fd_ca = -1;
  if (e == NULL) {
     esyslog("ddci: can't enum devices");
     goto unref;
     }
  if (udev_enumerate_add_match_subsystem(e, "dvb") < 0) {
     esyslog("ddci: can't add dvb subsystem to enum-filter");
     goto unref;
     }
  if (udev_enumerate_add_match_property(e, "DVB_DEVICE_TYPE", "ca") < 0) {
     esyslog("ddci: can't add property DVB_DEVICE_TYPE with value ca to enum-filter");
     goto unref;
     }
  if (udev_enumerate_scan_devices(e) < 0) {
     esyslog("ddci: can't scan for ca-devices");
     goto unref;
     }
  l = udev_enumerate_get_list_entry(e);
  while (l != NULL) {
        syspath = udev_list_entry_get_name(l);
        if (syspath != NULL) {
           dev = udev_device_new_from_syspath(udev, syspath);
           if (dev != NULL) {
              devnode = udev_device_get_devnode(dev);
              if (probedDevices.Find(devnode) < 0) {
                 isyslog("ddci: found %s", devnode);
                 adapterNum = udev_device_get_property_value(dev, "DVB_ADAPTER_NUM");
                 deviceNum = udev_device_get_property_value(dev, "DVB_DEVICE_NUM");
                 if ((adapterNum != NULL) && (deviceNum != NULL)
                  && !DvbDeviceNodeExists(DEV_DVB_FRONTEND, adapterNum, deviceNum)
                  && DvbDeviceNodeExists("sec", adapterNum, deviceNum)) {
                    fd_ca = open(*DvbDeviceName(DEV_DVB_CA, adapterNum, deviceNum), O_RDWR);
                    if (fd_ca >= 0) {
                       ci = new cDdDvbCiAdapter(Device, fd_ca, adapterNum, deviceNum, devnode);
                       fd_ca = -1;
                       goto unref;
                       }
                    }
                 }
              udev_device_unref(dev);
              dev = NULL;
              }
           }
        l = udev_list_entry_get_next(l);
        }
unref:
  if (fd_ca >= 0)
     close(fd_ca);
  if (dev != NULL)
     udev_device_unref(dev);
  if (e != NULL)
     udev_enumerate_unref(e);
  if (udev != NULL)
     udev_unref(udev);
  return ci;
}

// --- cTSTransfer -----------------------------------------------------------

ddci::cTSTransfer::cTSTransfer(int ReadFd, int WriteFd, int CardIndex)
:readFd(ReadFd)
,writeFd(WriteFd)
,cardIndex(CardIndex)
{
  SetDescription("TS transfer on device %d", cardIndex);
  Start();
}

ddci::cTSTransfer::~cTSTransfer()
{
  Cancel(3);
}

void ddci::cTSTransfer::Action(void)
{
  int bufSize = 50 * TS_SIZE; // MEGABYTE(2);
  uint8_t *buffer = new uint8_t[bufSize];
  ssize_t r;
  ssize_t w;
  if (buffer) {
     bool firstRead = false;
     cPoller Poller(readFd);
     while (Running()) {
           if (firstRead || Poller.Poll(100)) {
              if (!Running())
                 break;
              firstRead = false;
              r = safe_read(readFd, buffer, bufSize);
              if (!Running())
                 break;
              if (r < 0) {
                 if (FATALERRNO)
                    esyslog("ddci: read error on transfer buffer on device %d", cardIndex);
                 }
              else {
                 if ((r % TS_SIZE) != 0)
                    isyslog("ddci: read only %d bytes instead of %d", r, bufSize);
                 w = safe_write(writeFd, buffer, r);
                 if (w != r)
                    esyslog("ddci: write error on transfer buffer on device %d", cardIndex);
                 }
              }
           }
     delete [] buffer;
     }
}

// --- cTSTransferBuffer -----------------------------------------------------

ddci::cTSTransferBuffer::cTSTransferBuffer(int File, int Size, int CardIndex, cTSTransfer *Transfer)
:cTSBuffer(File, Size, CardIndex)
,file(File)
,transfer(Transfer)
{
}

ddci::cTSTransferBuffer::~cTSTransferBuffer()
{
  Cancel(3);
  if (transfer)
     delete transfer;
  close(file);
}
