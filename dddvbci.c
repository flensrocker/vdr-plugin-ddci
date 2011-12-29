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

cDdDvbCiAdapter *cDdDvbCiAdapter::ddCiAdapter[MAXDEVICES] = { NULL };
int cDdDvbCiAdapter::numDdCiAdapter = 0;

cDdDvbCiAdapter::cDdDvbCiAdapter(cDevice *Device, int FdCa, int FdSec, const char *AdapterNum, const char *DeviceNum, const char *DevNode)
 :cDvbCiAdapter(Device, FdCa)
 ,device(Device)
 ,fdSec(FdSec)
 ,devNode(DevNode)
 ,adapterNum(AdapterNum)
 ,deviceNum(DeviceNum)
 ,idle(false)
 ,transferBuffer(NULL)
{
  index = 0;
  while ((index < numDdCiAdapter) && (ddCiAdapter[index] != NULL))
        index++;
  if (index >= MAXDEVICES)
     esyslog("ddci: too many ci-adapters, redefine array size and recompile!");
  else {
     ddCiAdapter[index] = this;
     if (index == numDdCiAdapter)
        numDdCiAdapter++;
     }
  cDdDvbCiAdapterProbe::probedDevices.Append((char*)*devNode);
}

cDdDvbCiAdapter::~cDdDvbCiAdapter(void)
{
  if ((index >= 0) && (index < MAXDEVICES) && (ddCiAdapter[index] == this))
     ddCiAdapter[index] = NULL;
  int i = cDdDvbCiAdapterProbe::probedDevices.Find(*devNode);
  if (i >= 0)
     cDdDvbCiAdapterProbe::probedDevices.Remove(i);
  CloseSec();
}

bool cDdDvbCiAdapter::OpenSec(void)
{
  if (fdSec >= 0)
     return true;
  fdSec = open(*DvbDeviceName("sec", adapterNum, deviceNum), O_RDWR);
  return (fdSec >= 0);
}

void cDdDvbCiAdapter::CloseSec(void)
{
  if (fdSec < 0)
     return;
  if (transferBuffer)
     transferBuffer->Stop();
  close(fdSec);
  fdSec = -1;
}

cTSBuffer *cDdDvbCiAdapter::GetTSBuffer(int FdDvr)
{
  if (fdSec >= 0) {
     transferBuffer = new cTSTransferBuffer(this, FdDvr, fdSec, device->CardIndex() + 1);
     return transferBuffer;
     }
  return cDvbCiAdapter::GetTSBuffer(FdDvr);
}

bool cDdDvbCiAdapter::SetIdle(bool Idle, bool TestOnly)
{
  if (TestOnly || (idle == Idle))
     return true;
  if (Idle)
     CloseSec();
  else
     OpenSec();
  idle = Idle;
  return true;
}

void cDdDvbCiAdapter::Stop(void)
{
  for (int i = 0; i < numDdCiAdapter; i++) {
      if ((ddCiAdapter[i] != NULL) && (ddCiAdapter[i]->transferBuffer != NULL))
         ddCiAdapter[i]->transferBuffer->Stop();
      }
}

// --- cDdDvbCiAdapterProbe --------------------------------------------------

cStringList  cDdDvbCiAdapterProbe::probedDevices;

cDvbCiAdapter *cDdDvbCiAdapterProbe::Probe(cDevice *Device)
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
  int fd_sec = -1;
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
                       int numSlots = cDvbCiAdapter::GetNumCamSlots(Device, fd_ca, NULL);
                       isyslog("ddci: with %d cam slots", numSlots);
                       if (numSlots > 0) {
                          fd_sec = open(*DvbDeviceName("sec", adapterNum, deviceNum), O_RDWR);
                          if (fd_sec >= 0) {
                             ci = new cDdDvbCiAdapter(Device, fd_ca, fd_sec, adapterNum, deviceNum, devnode);
                             fd_ca = -1;
                             fd_sec = -1;
                             goto unref;
                             }
                          }
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
  if (fd_sec >= 0)
     close(fd_sec);
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

cTSTransfer::cTSTransfer(int ReadFd, int WriteFd, int CardIndex)
 :writeFd(WriteFd)
 ,cardIndex(CardIndex)
{
  SetDescription("TS transfer on device %d", CardIndex);
  reader = new cTSBuffer(ReadFd, MEGABYTE(2), CardIndex);
  if (reader != NULL)
     Start();
}

cTSTransfer::~cTSTransfer()
{
  Cancel(3);
  if (reader)
     delete reader;
}

void cTSTransfer::Action(void)
{
  uchar *buffer;
  while (Running()) {
        buffer = reader->Get();
        if (buffer != NULL) {
           if (safe_write(writeFd, buffer, TS_SIZE) != TS_SIZE)
              esyslog("ddci: write error on transfer buffer on device %d", cardIndex);
           }
        }
}

// --- cTSTransferBuffer -----------------------------------------------------

cTSTransferBuffer::cTSTransferBuffer(cDdDvbCiAdapter *CiAdapter, int FdDvr, int FdSec, int CardIndex)
 :cTSBuffer(FdSec, MEGABYTE(2), CardIndex)
 ,ciAdapter(CiAdapter)
{
  SetDescription("TS transfer buffer on device %d", CardIndex);
  transfer = new cTSTransfer(FdDvr, FdSec, CardIndex);
  if (transfer != NULL)
     Start();
}

cTSTransferBuffer::~cTSTransferBuffer()
{
  if ((ciAdapter != NULL) && (ciAdapter->transferBuffer == this))
     ciAdapter->transferBuffer = NULL;
  Cancel(3);
  if (transfer)
     delete transfer;
}

void cTSTransferBuffer::Stop(void)
{
  Cancel(3);
  if (transfer) {
     delete transfer;
     transfer = NULL;
     }
}
