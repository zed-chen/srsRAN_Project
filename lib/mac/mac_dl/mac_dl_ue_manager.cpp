/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "mac_dl_ue_manager.h"
#include "srsran/ran/pdsch/pdsch_constants.h"
#include "srsran/support/timers.h"

using namespace srsran;

mac_dl_ue_context::mac_dl_ue_context(const mac_ue_create_request& req) :
  ue_index(req.ue_index), rnti(req.crnti), harq_buffers(MAX_NOF_HARQS), rlf_notif(req.rlf_notifier)
{
  // Resize each HARQ buffer to maximum DL PDU size.
  // TODO: Account the maximum PDU length, given cell BW.
  for (std::vector<uint8_t>& harq : harq_buffers) {
    harq.resize(MAX_DL_PDU_LENGTH);
  }

  // Store DL logical channel notifiers.
  addmod_logical_channels(req.bearers);

  // Store UL-CCCH
  if (req.ul_ccch_msg != nullptr) {
    // If the Msg3 contained an UL-CCCH message, store it for Contention Resolution.
    srsran_assert(req.ul_ccch_msg->length() >= UE_CON_RES_ID_LEN,
                  "Invalid UL-CCCH message length ({} < 6)",
                  req.ul_ccch_msg->length());
    std::copy(req.ul_ccch_msg->begin(), req.ul_ccch_msg->begin() + UE_CON_RES_ID_LEN, msg3_subpdu.begin());
  }
}

void mac_dl_ue_context::addmod_logical_channels(span<const mac_logical_channel_config> dl_logical_channels)
{
  for (const mac_logical_channel_config& lc : dl_logical_channels) {
    dl_bearers.insert(lc.lcid, lc.dl_bearer);
  }
}

void mac_dl_ue_context::remove_logical_channels(span<const lcid_t> lcids_to_remove)
{
  for (lcid_t lcid : lcids_to_remove) {
    dl_bearers.erase(lcid);
  }
}

// ///////////////////////

mac_dl_ue_manager::mac_dl_ue_manager(du_rnti_table& rnti_table_, rlf_detector& rlf_handler_) :
  rnti_table(rnti_table_), rlf_handler(rlf_handler_)
{
}

bool mac_dl_ue_manager::add_ue(mac_dl_ue_context ue_to_add)
{
  const du_ue_index_t ue_index = ue_to_add.get_ue_index();

  // Register the UE in the repository.
  {
    std::lock_guard<std::mutex> lock(ue_mutex[ue_index]);
    if (ue_db.contains(ue_index)) {
      return false;
    }
    ue_db.emplace(ue_index, std::move(ue_to_add));
  }

  // Register UE in RLF detector.
  rlf_handler.add_ue(ue_index, ue_to_add.rlf_notifier());
  return true;
}

bool mac_dl_ue_manager::remove_ue(du_ue_index_t ue_index)
{
  // Remove UE from RLF detector.
  rlf_handler.rem_ue(ue_index);

  // Erase UE from the repository.
  std::lock_guard<std::mutex> lock(ue_mutex[ue_index]);
  if (not ue_db.contains(ue_index)) {
    return false;
  }
  ue_db.erase(ue_index);

  return true;
}

bool mac_dl_ue_manager::addmod_bearers(du_ue_index_t                          ue_index,
                                       span<const mac_logical_channel_config> dl_logical_channels)
{
  std::lock_guard<std::mutex> lock(ue_mutex[ue_index]);
  if (not ue_db.contains(ue_index)) {
    return false;
  }
  auto& u = ue_db[ue_index];

  u.addmod_logical_channels(dl_logical_channels);
  return true;
}

bool mac_dl_ue_manager::remove_bearers(du_ue_index_t ue_index, span<const lcid_t> lcids)
{
  std::lock_guard<std::mutex> lock(ue_mutex[ue_index]);
  if (not ue_db.contains(ue_index)) {
    return false;
  }
  auto& u = ue_db[ue_index];

  u.remove_logical_channels(lcids);
  return true;
}

ue_con_res_id_t mac_dl_ue_manager::get_con_res_id(rnti_t rnti)
{
  du_ue_index_t               ue_index = rnti_table[rnti];
  std::lock_guard<std::mutex> lock(ue_mutex[ue_index]);
  if (not ue_db.contains(ue_index)) {
    return {};
  }
  return ue_db[ue_index].get_con_res_id();
}
