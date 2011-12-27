#include "dddvbci.h"

#include <libudev.h>

#include <vdr/dvbdevice.h>

ddci::cDdDvbCiAdapter::cDdDvbCiAdapter(cDevice *Device, int Fd, const char *AdapterNum, const char *DeviceNum, const char *DevNode)
 :cDvbCiAdapter(Device, Fd)
 ,devNode(DevNode)
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
  return cDvbCiAdapter::GetTSBuffer(FdDvr);
}

// --- cDdDvbCiAdapterProbe --------------------------------------------------

cStringList  ddci::cDdDvbCiAdapterProbe::probedDevices;

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
  int fd_ca;
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
  if (dev != NULL)
     udev_device_unref(dev);
  if (e != NULL)
     udev_enumerate_unref(e);
  if (udev != NULL)
     udev_unref(udev);
  return ci;
}
