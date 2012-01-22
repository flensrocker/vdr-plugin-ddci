#ifndef __DDDVBCI_H
#define __DDDVBCI_H

#include <vdr/device.h>
#include <vdr/dvbci.h>

class cTSTransferBuffer;

class cDdDvbCiAdapter : public cDvbCiAdapter
{
friend class cTSTransferBuffer;
private:
  static cDdDvbCiAdapter *ddCiAdapter[MAXDEVICES];
  static int numDdCiAdapter;

  cDevice *device;
  int     fdSecW;
  int     fdSecR;
  cString devNode;
  cString adapterNum;
  cString deviceNum;
  int     index;
  bool    idle;
  cTSTransferBuffer *transferBuffer;

  bool OpenSec(void);
  void CloseSec(void);
public:
  cDdDvbCiAdapter(cDevice *Device, int FdCa, int FdSecW, int FdSecR, const char *AdapterNum, const char *DeviceNum, const char *DevNode);
  virtual ~cDdDvbCiAdapter(void);
  virtual cTSBufferBase *GetTSBuffer(int FdDvr);
  virtual bool SetIdle(bool Idle, bool TestOnly);
  virtual bool IsIdle(void) const { return idle; }

  static void Stop(void);
};

class cDdDvbCiAdapterProbe : public cDvbCiAdapterProbe
{
friend class cDdDvbCiAdapter;
private:
  static cStringList  probedDevices;

public:
  virtual cDvbCiAdapter *Probe(cDevice *Device);
};

class cTSTransferBuffer : public cTSBuffer
{
private:
  cDdDvbCiAdapter *ciAdapter;
  cTSBuffer *dvrReader;
  int cardIndex;
  int fdSecW;

public:
  cTSTransferBuffer(cDdDvbCiAdapter *CiAdapter, int FdDvr, int FdSecW, int FdSecR, int CardIndex);
  virtual ~cTSTransferBuffer();
  virtual uchar *Get(void);
  void Stop(void);
};

#endif // __DDDVBCI_H
