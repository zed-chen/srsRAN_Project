/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "srsran/ran/csi_report/csi_report_config_helpers.h"
#include "csi_report_on_puxch_helpers.h"

using namespace srsran;

csi_report_configuration srsran::create_csi_report_configuration(const csi_meas_config& csi_meas)
{
  csi_report_configuration csi_rep = {};
  csi_rep.pmi_codebook             = pmi_codebook_type::one;

  // TODO: support more CSI reports.
  const csi_report_config& csi_rep_cfg = csi_meas.csi_report_cfg_list[0];

  // TODO: support more CSI resource sets.
  nzp_csi_rs_res_set_id_t nzp_csi_set_id =
      variant_get<csi_resource_config::nzp_csi_rs_ssb>(
          csi_meas.csi_res_cfg_list[csi_rep_cfg.res_for_channel_meas].csi_rs_res_set_list)
          .nzp_csi_rs_res_set_list[0];
  csi_rep.nof_csi_rs_resources = csi_meas.nzp_csi_rs_res_set_list[nzp_csi_set_id].nzp_csi_rs_res.size();

  // Enable indicators
  switch (csi_rep_cfg.report_qty_type) {
    case csi_report_config::report_quantity_type_t::none:
      break;
    case csi_report_config::report_quantity_type_t::cri_ri_li_pmi_cqi:
      csi_rep.quantities = csi_report_quantities::cri_ri_li_pmi_cqi;
      break;
    case csi_report_config::report_quantity_type_t::cri_ri_pmi_cqi:
      csi_rep.quantities = csi_report_quantities::cri_ri_pmi_cqi;
      break;
    case csi_report_config::report_quantity_type_t::cri_ri_cqi:
      csi_rep.quantities = csi_report_quantities::cri_ri_cqi;
      break;
    default:
      csi_rep.quantities = csi_report_quantities::other;
      break;
  }

  if (csi_rep_cfg.codebook_cfg.has_value()) {
    if (variant_holds_alternative<codebook_config::type1>(csi_rep_cfg.codebook_cfg->codebook_type)) {
      const auto& type1 = variant_get<codebook_config::type1>(csi_rep_cfg.codebook_cfg->codebook_type);
      if (variant_holds_alternative<codebook_config::type1::single_panel>(type1.sub_type)) {
        using single_panel = codebook_config::type1::single_panel;
        const auto& panel  = variant_get<single_panel>(type1.sub_type);

        if (variant_holds_alternative<single_panel::two_antenna_ports_two_tx_codebook_subset_restriction>(
                panel.nof_antenna_ports)) {
          csi_rep.pmi_codebook = pmi_codebook_type::two;
        } else if (variant_holds_alternative<single_panel::more_than_two_antenna_ports>(panel.nof_antenna_ports)) {
          csi_rep.pmi_codebook = pmi_codebook_type::typeI_single_panel_4ports_mode1;
        } else {
          csi_rep.pmi_codebook = pmi_codebook_type::other;
        }

        csi_rep.ri_restriction = panel.typei_single_panel_ri_restriction;
      } else {
        report_fatal_error("Codebook panel type not supported");
      }
    } else {
      report_fatal_error("Codebook type not supported");
    }
  }

  return csi_rep;
}

bool srsran::is_valid(const csi_report_configuration& config)
{
  // The number of CSI resources in the corresponding resource set must be at least one and up to 64 (see TS38.331
  // Section 6.3.2, Information Element \c NZP-CSI-RS-ResourceSet).
  constexpr interval<unsigned, true> nof_csi_res_range(1, 64);
  if (!nof_csi_res_range.contains(config.nof_csi_rs_resources)) {
    return false;
  }

  // The PMI codebook type is not supported.
  if (config.pmi_codebook == pmi_codebook_type::other) {
    return false;
  }

  if (config.pmi_codebook != pmi_codebook_type::one) {
    unsigned nof_csi_rs_ports = csi_report_get_nof_csi_rs_antenna_ports(config.pmi_codebook);

    // The RI restriction set size is too small to cover all possible ranks given the number of CSI-RS ports.
    if (config.ri_restriction.size() < nof_csi_rs_ports) {
      return false;
    }

    // The RI Restriction set cannot allow a higher rank than the number of CSI-RS ports.
    if (config.ri_restriction.find_highest() >= static_cast<int>(nof_csi_rs_ports)) {
      return false;
    }
  }

  // The CSI report quantities are not supported.
  if (config.quantities == csi_report_quantities::other) {
    return false;
  }

  return true;
}
