/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "cu_cp_controller.h"
#include "../cu_up_processor/cu_up_processor_repository.h"
#include "../du_processor/du_processor_repository.h"
#include <thread>

using namespace srsran;
using namespace srs_cu_cp;

cu_cp_controller::cu_cp_controller(const cu_cp_configuration&  config_,
                                   common_task_scheduler&      common_task_sched_,
                                   ngap_connection_manager&    ngap_conn_mng_,
                                   cu_up_processor_repository& cu_ups_,
                                   du_processor_repository&    dus_,
                                   task_executor&              ctrl_exec_) :
  cfg(config_),
  common_task_sched(common_task_sched_),
  ctrl_exec(ctrl_exec_),
  logger(srslog::fetch_basic_logger("CU-CP")),
  amf_mng(common_task_sched_, ngap_conn_mng_),
  du_mng(cfg.admission.max_nof_dus, dus_, ctrl_exec, common_task_sched_),
  cu_up_mng(cfg.admission.max_nof_cu_ups, cu_ups_, ctrl_exec, common_task_sched_)
{
}

void cu_cp_controller::stop()
{
  // Note: Called from separate outer thread.
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (not running) {
      return;
    }
  }

  // Stop and delete DU connections.
  du_mng.stop();

  // Stop and delete CU-UP connections.
  cu_up_mng.stop();

  // Stop AMF connection.
  while (not ctrl_exec.defer([this]() { stop_impl(); })) {
    logger.warning("Failed to dispatch CU-CP stop task. Retrying...");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Wait for stop_impl() to signal completion.
  std::unique_lock<std::mutex> lock(mutex);
  cvar.wait(lock, [this]() { return not running; });
}

void cu_cp_controller::stop_impl()
{
  common_task_sched.schedule_async_task(launch_async([this](coro_context<async_task<void>>& ctx) {
    CORO_BEGIN(ctx);

    // Stop AMF connection.
    CORO_AWAIT(amf_mng.stop());

    // CU-CP stop successfully finished.
    // Dispatch main async task loop destruction via defer so that the current coroutine ends successfully.
    while (not ctrl_exec.defer([this]() {
      std::lock_guard<std::mutex> lock(mutex);
      running = false;
      cvar.notify_one();
    })) {
      logger.warning("Unable to stop DU Manager. Retrying...");
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CORO_RETURN();
  }));
}

bool cu_cp_controller::handle_du_setup_request(du_index_t du_idx, const du_setup_request& req)
{
  if (not amf_mng.is_amf_connected()) {
    // If AMF is not connected, it either means that the CU-CP is not operational state or there is a CU-CP failure.
    return false;
  }
  return true;
}

bool cu_cp_controller::request_ue_setup() const
{
  if (not amf_mng.is_amf_connected()) {
    return false;
  }

  if (cu_up_mng.nof_cu_ups() == 0) {
    return false;
  }

  return true;
}
