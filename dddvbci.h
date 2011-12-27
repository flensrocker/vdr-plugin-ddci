#ifndef __DDDVBCI_H
#define __DDDVBCI_H

#include <vdr/dvbci.h>

namespace ddci
{
  class cDdDvbCiAdapter : public cDvbCiAdapter
  {
  private:
    cString devNode;

  public:
    cDdDvbCiAdapter(cDevice *Device, int Fd, const char *AdapterNum, const char *DeviceNum, const char *DevNode);
    virtual ~cDdDvbCiAdapter(void);
    virtual cTSBuffer *GetTSBuffer(int FdDvr);
  };

  class cDdDvbCiAdapterProbe : public cDvbCiAdapterProbe
  {
    friend class cDdDvbCiAdapter;

  private:
    static cStringList  probedDevices;

  public:
    virtual cDvbCiAdapter *Probe(cDevice *Device);
  };
}

#endif // __DDDVBCI_H
