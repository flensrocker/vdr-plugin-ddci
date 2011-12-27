#ifndef __DDDVBCI_H
#define __DDDVBCI_H

#include <vdr/device.h>
#include <vdr/dvbci.h>

namespace ddci
{
  class cDdDvbCiAdapter : public cDvbCiAdapter
  {
  private:
    cDevice *device;
    cString devNode;
    cString adapterNum;
    cString deviceNum;

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

  class cTSTransfer : public cThread
  {
  private:
    int  readFd;
    int  writeFd;
    int  cardIndex;

    virtual void Action(void);
  public:
    cTSTransfer(int ReadFd, int WriteFd, int CardIndex);
    ~cTSTransfer();
  };

  class cTSTransferBuffer : public cTSBuffer
  {
  private:
    int file;
    cTSTransfer *transfer;

  public:
    cTSTransferBuffer(int File, int Size, int CardIndex, cTSTransfer *Transfer);
    ~cTSTransferBuffer();
  };
}

#endif // __DDDVBCI_H
