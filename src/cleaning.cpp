#include "cleaning.h"

#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

namespace
{
constexpr uint8_t MAX_SEND_ATTEMPTS = 3;
constexpr uint32_t ACK_TIMEOUT_MS = 750;
constexpr uint16_t ACK_SIGNATURE = 0xCA1E;

uint8_t cleaningPeerMac[6] = {};
bool espNowReady = false;
volatile bool acknowledgementReceived = false;
volatile uint16_t pendingMessageId = 0;
uint16_t nextMessageId = 0;
CleaningCommandHandler commandHandler = nullptr;
bool hasHandledMessage = false;
uint16_t lastHandledMessageId = 0;

void onEspNowDataReceived(
    const uint8_t *senderMac,
    const uint8_t *data,
    int dataLength)
{
  if (senderMac == nullptr ||
      data == nullptr ||
      memcmp(senderMac, cleaningPeerMac, sizeof(cleaningPeerMac)) != 0)
  {
    return;
  }

  if (dataLength == sizeof(CleaningAcknowledgement))
  {
    CleaningAcknowledgement acknowledgement = {};
    memcpy(&acknowledgement, data, sizeof(acknowledgement));
    if (acknowledgement.signature == ACK_SIGNATURE &&
        acknowledgement.messageId == pendingMessageId)
    {
      acknowledgementReceived = true;
    }
    return;
  }

  if (dataLength == sizeof(CleaningCommand) && commandHandler != nullptr)
  {
    CleaningCommand command = {};
    memcpy(&command, data, sizeof(command));
    const bool directionTerminated =
        memchr(command.direction, '\0', sizeof(command.direction)) != nullptr;
    const bool stateTerminated =
        memchr(command.state, '\0', sizeof(command.state)) != nullptr;
    if (!directionTerminated || !stateTerminated)
    {
      return;
    }

    if (hasHandledMessage &&
        command.messageId == lastHandledMessageId)
    {
      CleaningAcknowledgement acknowledgement = {};
      createCleaningAcknowledgement(command.messageId, &acknowledgement);
      esp_now_send(
          senderMac,
          reinterpret_cast<const uint8_t *>(&acknowledgement),
          sizeof(acknowledgement));
      return;
    }

    if (commandHandler(command))
    {
      hasHandledMessage = true;
      lastHandledMessageId = command.messageId;
      CleaningAcknowledgement acknowledgement = {};
      createCleaningAcknowledgement(command.messageId, &acknowledgement);
      esp_now_send(
          senderMac,
          reinterpret_cast<const uint8_t *>(&acknowledgement),
          sizeof(acknowledgement));
    }
  }
}
}

esp_err_t beginCleaningEspNow(const uint8_t peerMac[6])
{
  if (peerMac == nullptr)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!WiFi.mode(WIFI_STA))
  {
    return ESP_FAIL;
  }

  if (!espNowReady)
  {
    const esp_err_t initResult = esp_now_init();
    if (initResult != ESP_OK)
    {
      return initResult;
    }
    espNowReady = true;

    const esp_err_t callbackResult =
        esp_now_register_recv_cb(onEspNowDataReceived);
    if (callbackResult != ESP_OK)
    {
      esp_now_deinit();
      espNowReady = false;
      return callbackResult;
    }
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerMac, sizeof(peerInfo.peer_addr));
  peerInfo.channel = 0;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;

  const esp_err_t peerResult = esp_now_is_peer_exist(peerMac)
      ? esp_now_mod_peer(&peerInfo)
      : esp_now_add_peer(&peerInfo);
  if (peerResult != ESP_OK)
  {
    return peerResult;
  }

  memcpy(cleaningPeerMac, peerMac, sizeof(cleaningPeerMac));
  return ESP_OK;
}

void setCleaningCommandHandler(CleaningCommandHandler handler)
{
  commandHandler = handler;
}

esp_err_t sendCleaningCommand(
    uint8_t fan1,
    uint8_t fan2,
    uint8_t fan3,
    uint8_t fan4,
    const char *direction,
    const char *state)
{
  if (!espNowReady)
  {
    return ESP_ERR_INVALID_STATE;
  }

  CleaningCommand command = {};
  if (nextMessageId == UINT16_MAX)
  {
    nextMessageId = 0;
  }
  const uint16_t messageId = ++nextMessageId;

  if (!createCleaningCommand(
          fan1, fan2, fan3, fan4, direction, state, &command, messageId))
  {
    return ESP_ERR_INVALID_ARG;
  }

  bool sendAccepted = false;
  esp_err_t lastSendResult = ESP_FAIL;
  for (uint8_t attempt = 0; attempt < MAX_SEND_ATTEMPTS; ++attempt)
  {
    acknowledgementReceived = false;
    pendingMessageId = messageId;
    lastSendResult = esp_now_send(
        cleaningPeerMac,
        reinterpret_cast<const uint8_t *>(&command),
        sizeof(command));
    if (lastSendResult != ESP_OK)
    {
      continue;
    }

    sendAccepted = true;
    const uint32_t waitStartedAt = millis();
    while (!acknowledgementReceived &&
           millis() - waitStartedAt < ACK_TIMEOUT_MS)
    {
      delay(1);
    }

    if (acknowledgementReceived)
    {
      return ESP_OK;
    }
  }

  return sendAccepted ? ESP_ERR_TIMEOUT : lastSendResult;
}

esp_err_t sendCleaningAcknowledgement(
    const uint8_t receiverMac[6],
    uint16_t messageId)
{
  if (!espNowReady)
  {
    return ESP_ERR_INVALID_STATE;
  }
  if (receiverMac == nullptr)
  {
    return ESP_ERR_INVALID_ARG;
  }

  CleaningAcknowledgement acknowledgement = {};
  createCleaningAcknowledgement(messageId, &acknowledgement);
  return esp_now_send(
      receiverMac,
      reinterpret_cast<const uint8_t *>(&acknowledgement),
      sizeof(acknowledgement));
}