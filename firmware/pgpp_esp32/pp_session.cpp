#include "pp_session.h"

PPSession gSessions[PP_MAX_CONNECTIONS];

static PPSession* gCurrentSessionByCore[2] = { nullptr, nullptr };

void ppSessionInitAll()
{
  memset(gSessions, 0, sizeof(gSessions));
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    gSessions[i].connHandle = BLE_CONN_HANDLE_INVALID;
    gSessions[i].recordIndex = -1;
  }
}

int ppSessionSlotByHandle(uint16_t connHandle)
{
  if (connHandle == BLE_CONN_HANDLE_INVALID) {
    return -1;
  }
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (gSessions[i].used && gSessions[i].connHandle == connHandle) {
      return i;
    }
  }
  return -1;
}

int ppSessionClaim(uint16_t connHandle)
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) {
      memset(&gSessions[i], 0, sizeof(PPSession));
      gSessions[i].used = true;
      gSessions[i].connHandle = connHandle;
      gSessions[i].recordIndex = -1;
      return i;
    }
  }
  return -1;
}

int ppSessionRelease(uint16_t connHandle)
{
  int slot = ppSessionSlotByHandle(connHandle);
  if (slot < 0) {
    return -1;
  }
  gSessions[slot].used = false;
  gSessions[slot].connHandle = BLE_CONN_HANDLE_INVALID;
  return slot;
}

int ppSessionActiveCount()
{
  int n = 0;
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (gSessions[i].used) {
      ++n;
    }
  }
  return n;
}

int ppSessionAnyPaired()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (gSessions[i].used && gSessions[i].persistentReconnectReady) {
      return i;
    }
  }
  return -1;
}

void ppSetCurrentSession(PPSession* s)
{
  gCurrentSessionByCore[xPortGetCoreID() & 1] = s;
}

PPSession* ppCurrentSession()
{
  PPSession* s = gCurrentSessionByCore[xPortGetCoreID() & 1];
  if (s == nullptr) {
    // Fallback: the lowest active slot (boot paths before any connect).
    for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
      if (gSessions[i].used) {
        return &gSessions[i];
      }
    }
  }
  return s;
}

void ppSetCurrentSessionByHandle(uint16_t connHandle)
{
  int slot = ppSessionSlotByHandle(connHandle);
  ppSetCurrentSession(slot >= 0 ? &gSessions[slot] : nullptr);
}

uint8_t ppSessionAggregateManufacturerState()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (gSessions[i].persistentReconnectReady || gSessions[i].pairingStage == PAIR_STAGE_AUTHENTICATED) {
      return MANUFACTURER_STATE_RECONNECT;
    }
  }
  return MANUFACTURER_STATE_PAIRABLE;
}
