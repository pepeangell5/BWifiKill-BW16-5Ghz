#include "packet-injection.h"


static const size_t MAX_RAW_MGMT_FRAME_BYTES = 0x68;

bool wifi_tx_raw_frame(const void* frame, size_t length) {
  if (rltk_wlan_info == NULL || frame == NULL || length == 0 || length > MAX_RAW_MGMT_FRAME_BYTES) {
    return false;
  }

  uint32_t **wlan_info_ptr = (uint32_t **)(rltk_wlan_info + 0x10);
  if (wlan_info_ptr == NULL || *wlan_info_ptr == NULL || **wlan_info_ptr == 0) {
    return false;
  }

  uint8_t *ptr = (uint8_t *)**wlan_info_ptr;
  uint8_t *frame_control = (uint8_t *)alloc_mgtxmitframe(ptr + 0xa80);

  if (frame_control != 0) {
    update_mgntframe_attrib(ptr, frame_control + 8);
    uint32_t frame_buffer = *(uint32_t *)(frame_control + 0x80);
    if (frame_buffer == 0) {
      return false;
    }

    memset((void *)frame_buffer, 0, 0x68);
    uint8_t *frame_data = (uint8_t *)frame_buffer + 0x28;
    memcpy(frame_data, frame, length);
    *(uint32_t *)(frame_control + 0x14) = length;
    *(uint32_t *)(frame_control + 0x18) = length;
    dump_mgntframe(ptr, frame_control);
    return true;
  }

  return false;
}

/*

*/
bool wifi_tx_deauth_frame(void* src_mac, void* dst_mac, uint16_t reason) {
  DeauthFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.reason = reason;
  return wifi_tx_raw_frame(&frame, sizeof(DeauthFrame));
}

/*

*/
bool wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid) {
  BeaconFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  for (int i = 0; ssid[i] != '\0'; i++) {
    frame.ssid[i] = ssid[i];
    frame.ssid_length++;
  }
  return wifi_tx_raw_frame(&frame, 38 + frame.ssid_length);
}
