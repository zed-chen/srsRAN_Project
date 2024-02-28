/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "e1ap_cause.h"
#include "ngap_cause.h"

namespace srsran {

/// \brief Converts an E1AP cause to an NGAP cause.
ngap_cause_t e1ap_to_ngap_cause(e1ap_cause_t e1ap_cause);

} // namespace srsran
