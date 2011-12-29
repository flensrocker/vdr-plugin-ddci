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
  int     fdSec;
  cString devNode;
  cString adapterNum;
  cString deviceNum;
  int     index;
  bool    idle;
  cTSTransferBuffer *transferBuffer;

  bool OpenSec(void);
  void CloseSec(void);
public:
  cDdDvbCiAdapter(cDevice *Device, int FdCa, int FdSec, const char *AdapterNum, const char *DeviceNum, const char *DevNode);
  virtual ~cDdDvbCiAdapter(void);
  virtual cTSBuffer *GetTSBuffer(int FdDvr);
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

class cTSTransfer : public cThread
{
private:
  int  writeFd;
  int  cardIndex;
  cTSBuffer *reader;

  virtual void Action(void);
public:
  cTSTransfer(int ReadFd, int WriteFd, int CardIndex);
  ~cTSTransfer();
};

class cTSTransferBuffer : public cTSBuffer
{
private:
  cDdDvbCiAdapter *ciAdapter;
  cTSTransfer *transfer;

public:
  cTSTransferBuffer(cDdDvbCiAdapter *CiAdapter, int FdDvr, int FdSec, int CardIndex);
  virtual ~cTSTransferBuffer();
  void Stop(void);
};

#endif // __DDDVBCI_H
