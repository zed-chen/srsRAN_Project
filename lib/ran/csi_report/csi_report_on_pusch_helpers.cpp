/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "srsran/ran/csi_report/csi_report_on_pusch_helpers.h"
#include "csi_report_on_puxch_helpers.h"
#include "srsran/adt/interval.h"
#include "srsran/support/error_handling.h"

using namespace srsran;

static csi_report_data unpack_pusch_csi_cri_ri_li_pmi_cqi(const csi_report_packed&        csi1_packed,
                                                          const csi_report_packed&        csi2_packed,
                                                          const csi_report_configuration& config)
{
  csi_report_data data;

  // Compute the CSI Part 1 size and the CSI Part 2 size information.
  csi_report_pusch_size csi_size_info = get_csi_report_pusch_size(config);

  if (csi1_packed.size() != csi_size_info.part1_size.value()) {
    srslog::fetch_basic_logger("MAC").warning(
        "The number of packed bits for CSI Part 1 (i.e., {}) is not equal to the CSI Part 1 size (i.e., {})",
        units::bits(csi1_packed.size()),
        csi_size_info.part1_size);
    // Return empty data.
    return data;
  }

  // Gets RI, LI, CQI and CRI field sizes as if the rank was one.
  ri_li_cqi_cri_sizes sizes =
      get_ri_li_cqi_cri_sizes(config.pmi_codebook, config.ri_restriction, 1U, config.nof_csi_rs_resources);

  // Unpack CSI Part 1 and Part 2 bits following the order in TS38.212 Tables 6.3.2.1.2-3 and 6.3.2.1.2-4.
  unsigned csi1_count = 0;
  unsigned csi2_count = 0;

  // CRI.
  unsigned cri = 0;
  if (sizes.cri > 0) {
    cri = csi1_packed.extract(csi1_count, sizes.cri);
  }
  data.cri.emplace(cri);
  csi1_count += sizes.cri;

  // RI.
  unsigned ri = 1;
  if (sizes.ri > 0) {
    ri = csi1_packed.extract(csi1_count, sizes.ri) + 1;
  }
  data.ri.emplace(ri);
  csi1_count += sizes.ri;

  if ((config.quantities == csi_report_quantities::cri_ri_pmi_cqi) ||
      (config.quantities == csi_report_quantities::cri_ri_cqi) ||
      (config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi)) {
    // Wideband CQI for the first TB.
    data.first_tb_wideband_cqi.emplace(
        csi_report_unpack_wideband_cqi(csi1_packed.slice(csi1_count, csi1_count + sizes.wideband_cqi_first_tb)));
    csi1_count += sizes.wideband_cqi_first_tb;

    // Subband differential CQI for the first TB
    // ... Not supported.
  }

  srsran_assert(csi1_count == csi1_packed.size(),
                "The number of read bits (i.e., {}) is not equal to the CSI Part 1 report size (i.e., {})",
                units::bits(csi1_count),
                csi1_packed.size());

  // Verify CSI Part 2 size.
  interval<unsigned, true> csi2_size_range(csi_size_info.part2_min_size.value(),
                                           csi_size_info.part2_max_size.value());
  if (!csi2_size_range.contains(csi2_packed.size())) {
    srslog::fetch_basic_logger("MAC").warning(
        "The number of packed bits for CSI Part 2 (i.e., {}) is outside the valid range (i.e., {})",
        units::bits(csi2_packed.size()),
        csi2_size_range);
    // Return CSI Part 1 data only.
    return data;
  }

  if (!csi2_packed.empty()) {
    if ((config.quantities == csi_report_quantities::cri_ri_pmi_cqi) ||
        (config.quantities == csi_report_quantities::cri_ri_cqi) ||
        (config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi)) {
      // Wideband CQI for the second TB.
      if (sizes.wideband_cqi_second_tb != 0) {
        data.second_tb_wideband_cqi.emplace(
            csi_report_unpack_wideband_cqi(csi2_packed.slice(csi2_count, csi2_count + sizes.wideband_cqi_second_tb)));
        csi2_count += sizes.wideband_cqi_second_tb;
      }
    }

    // Layer Indicator.
    if (config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi) {
      // Gets RI, LI, CQI and CRI field sizes.
      ri_li_cqi_cri_sizes sizes_ri =
          get_ri_li_cqi_cri_sizes(config.pmi_codebook, config.ri_restriction, ri, config.nof_csi_rs_resources);

      unsigned li = 0;
      if (sizes_ri.li != 0) {
        li = csi2_packed.extract(csi2_count, sizes_ri.li);
      }

      data.li.emplace(li);
      csi2_count += sizes_ri.li;
    }

    // PMI wideband information fields X1 and X2, or codebook index for 2 antenna ports.
    if ((config.quantities == csi_report_quantities::cri_ri_pmi_cqi) ||
        (config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi)) {
      unsigned pmi_size = csi_report_get_size_pmi(config.pmi_codebook, ri);

      if (pmi_size != 0) {
        data.pmi.emplace(
            csi_report_unpack_pmi(csi2_packed.slice(csi2_count, csi2_count + pmi_size), config.pmi_codebook, ri));
        csi2_count += pmi_size;
      }
    }

    srsran_assert(csi2_count == csi2_packed.size(),
                  "The number of read bits (i.e., {}) is not equal to the CSI Part 2 report size (i.e., {})",
                  units::bits(csi2_count),
                  csi2_packed.size());
  }

  return data;
}

csi_report_pusch_size srsran::get_csi_report_pusch_size(const csi_report_configuration& config)
{
  csi_report_pusch_size result                = {};
  unsigned              nof_csi_antenna_ports = csi_report_get_nof_csi_rs_antenna_ports(config.pmi_codebook);

  // Get CSI Part 1 field sizes which do not depend on the number of layers.
  ri_li_cqi_cri_sizes part1_sizes =
      get_ri_li_cqi_cri_sizes(config.pmi_codebook, config.ri_restriction, 1, config.nof_csi_rs_resources);

  // Calculate CSI Part 1 size following TS38.212 Table 6.3.2.1.2-3.
  unsigned part1_size = 0;

  // CRI, RI and wideband CQI for the first TB are always present and their size does not depend on the RI.
  if ((config.quantities == csi_report_quantities::cri_ri_pmi_cqi) ||
      (config.quantities == csi_report_quantities::cri_ri_cqi) ||
      (config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi)) {
    part1_size += part1_sizes.cri;
    part1_size += part1_sizes.ri;
    part1_size += part1_sizes.wideband_cqi_first_tb;
  }

  result.part1_size = units::bits{part1_size};

  // Skip CSI Part 2 if there is one transmit port and no quantity is reported in CSI Part 2.
  if ((nof_csi_antenna_ports == 1) || ((config.quantities != csi_report_quantities::cri_ri_li_pmi_cqi) &&
                                       (config.quantities != csi_report_quantities::cri_ri_pmi_cqi))) {
    return result;
  }

  // Prepare CSI Part 2 entry.
  uci_part2_size_description::entry& entry = result.part2_correspondence.entries.emplace_back();

  // Prepare RI field as parameter.
  uci_part2_size_description::parameter& parameter = entry.parameters.emplace_back();
  parameter.offset                                 = part1_sizes.cri;
  parameter.width                                  = part1_sizes.ri;

  // Fill the entry table in function of the RI.
  for (unsigned i_nof_layers = 0; i_nof_layers != nof_csi_antenna_ports; ++i_nof_layers) {
    // Get CSI Part2 field sizes which depend on the number of layers.
    ri_li_cqi_cri_sizes part2_sizes = get_ri_li_cqi_cri_sizes(
        config.pmi_codebook, config.ri_restriction, i_nof_layers + 1, config.nof_csi_rs_resources);

    // Calculate CSI Part 2 size following TS38.212 Table 6.3.2.1.2-4.
    unsigned part2_size = 0;

    // Wideband CQI for the second TB.
    if ((config.quantities == csi_report_quantities::cri_ri_pmi_cqi) ||
        (config.quantities == csi_report_quantities::cri_ri_cqi) ||
        (config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi)) {
      part2_size += part2_sizes.wideband_cqi_second_tb;
    }

    // LI.
    if (config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi) {
      part2_size += part2_sizes.li;
    }

    // PMI.
    if ((config.quantities == csi_report_quantities::cri_ri_li_pmi_cqi) ||
        (config.quantities == csi_report_quantities::cri_ri_pmi_cqi)) {
      part2_size += csi_report_get_size_pmi(config.pmi_codebook, i_nof_layers + 1);
    }

    entry.map.emplace_back(part2_size);
  }

  result.part2_min_size = units::bits(*std::min_element(entry.map.begin(), entry.map.end()));
  result.part2_max_size = units::bits(*std::max_element(entry.map.begin(), entry.map.end()));

  return result;
}

csi_report_data srsran::csi_report_unpack_pusch(const csi_report_packed&        csi1_packed,
                                                const csi_report_packed&        csi2_packed,
                                                const csi_report_configuration& config)
{
  // Assert that CSI Part 2 payload is present if it is required.
  srsran_assert(((config.pmi_codebook == pmi_codebook_type::one) ||
                 ((config.quantities != csi_report_quantities::cri_ri_li_pmi_cqi) &&
                  (config.quantities != csi_report_quantities::cri_ri_pmi_cqi))) ||
                    !csi2_packed.empty(),
                "PUSCH CSI Part 2 is required for more than one CSI-RS port when PMI is reported.");

  // Select unpacking depending on the CSI report quantities.
  csi_report_data data;
  switch (config.quantities) {
    case csi_report_quantities::cri_ri_pmi_cqi:
    case csi_report_quantities::cri_ri_cqi:
    case csi_report_quantities::cri_ri_li_pmi_cqi:
      return unpack_pusch_csi_cri_ri_li_pmi_cqi(csi1_packed, csi2_packed, config);
    case csi_report_quantities::other:
    default:
      report_error("Invalid CSI report quantities.");
  }
}

csi_report_data srsran::csi_report_unpack_pusch(const csi_report_packed&        csi1_packed,
                                                const csi_report_configuration& config)
{
  return csi_report_unpack_pusch(csi1_packed, csi_report_packed(0), config);
}